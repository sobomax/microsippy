#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "public/microsippy.h"
#include "external/mackron_md5/md5.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_auth.h"
#include "usipy_sip_hdr_authz.h"
#include "usipy_sip_hdr_cseq.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_hdr_nameaddr.h"
#include "usipy_sip_res.h"
#include "usipy_sip_tid.h"
#include "usipy_tm_uac.h"

struct test_cbarg {
    size_t nsent;
    size_t nresponses;
    size_t ntimeouts;
    uint16_t last_status_code;
    uint64_t start_ms;
    struct usipy_sip_tm *tm;
    struct usipy_str username;
    struct usipy_str password;
    struct usipy_str qop;
    struct usipy_sip_tm_extra_header extra_hdrs[2];
    int auth_retry_started;
    int stop;
};

static int
stdout_send_to(void *arg, size_t tx_index, const struct usipy_sip_tm_tx *txp,
  const struct usipy_sip_tm_outbound *outp)
{
    struct test_cbarg *carg = arg;
    const uint64_t now_ms = usipy_tm_uac_mono_ms();

    (void)arg;
    (void)tx_index;
    (void)txp;
    carg->nsent += 1;
    if (fprintf(stdout, "[%llu ms]\n",
      (unsigned long long)(now_ms - carg->start_ms)) < 0) {
        return (-1);
    }
    return (fwrite(outp->raw.s.ro, 1, outp->raw.l, stdout) == outp->raw.l ? 0 : -1);
}

static void
uac_response(void *arg, size_t tx_index, const struct usipy_sip_tm_tx *txp,
  const struct usipy_msg *msg)
{
    struct test_cbarg *carg = arg;
    const struct usipy_msg *cmsg = msg;
    const struct usipy_sip_hdr_auth *ap = NULL;

    (void)tx_index;
    (void)txp;
    assert(msg != NULL);
    assert(msg->kind == USIPY_SIP_MSG_RES);
    assert(usipy_sip_msg_parse_hdrs((struct usipy_msg *)cmsg,
      USIPY_HFT_MASK(USIPY_HF_WWWAUTHENTICATE), 0) == 0);
    for (unsigned int i = 0; i < cmsg->nhdrs; i++) {
        const struct usipy_sip_hdr *shp = &cmsg->hdrs[i];

        if (shp->hf_type->cantype != USIPY_HF_WWWAUTHENTICATE) {
            continue;
        }
        ap = shp->parsed.auth;
        break;
    }
    assert(ap != NULL);
    assert(ap->scheme.l == 6 && memcmp(ap->scheme.s.ro, "Digest", 6) == 0);
    assert(ap->realm.l == 12 && memcmp(ap->realm.s.ro, "example.test", 12) == 0);
    assert(ap->nonce.l == 6 && memcmp(ap->nonce.s.ro, "abcdef", 6) == 0);
    assert(ap->algorithm.l == 3 && memcmp(ap->algorithm.s.ro, "MD5", 3) == 0);
    assert(ap->qop.l == 4 && memcmp(ap->qop.s.ro, "auth", 4) == 0);
    carg->nresponses += 1;
    carg->last_status_code = msg->sline.parsed.sl.status.code;
    if (msg->sline.parsed.sl.status.code == 401 && txp->common.id.cseq == 2 &&
      !carg->auth_retry_started) {
        assert(usipy_tm_uac_register_reply_auth(carg->tm, tx_index, msg,
          &carg->username, &carg->password, &carg->qop, &carg->extra_hdrs[0],
          1) == USIPY_SIP_TM_OK);
        carg->auth_retry_started = 1;
    }
}

static void
md5_hex_join(char out[MD5_SIZE_FORMATTED], size_t nparts,
  const struct usipy_str *partsp)
{
    unsigned char hash[MD5_SIZE];
    md5_context ctx;

    md5_init(&ctx);
    for (size_t i = 0; i < nparts; i++) {
        if (i != 0) {
            md5_update(&ctx, ":", 1);
        }
        if (partsp[i].l != 0) {
            md5_update(&ctx, partsp[i].s.ro, partsp[i].l);
        }
    }
    md5_finalize(&ctx, hash);
    md5_format(out, MD5_SIZE_FORMATTED, hash);
}

static void
test_gen_auth_hf(void)
{
    char chall_storage[1024], auth_storage[1024], buf[1024], expbuf[1024];
    char ha1[MD5_SIZE_FORMATTED], ha2[MD5_SIZE_FORMATTED];
    char response[MD5_SIZE_FORMATTED];
    size_t chall_cpts[4], auth_cpts[4];
    struct usipy_msg_heap chall_heap, auth_heap;
    const struct usipy_str challenge_noqop = USIPY_2STR(
      "Digest realm=\"testrealm@host.com\",nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\",opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"");
    const struct usipy_str challenge_qop = USIPY_2STR(
      "Digest realm=\"testrealm@host.com\",nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\",qop=auth,algorithm=MD5,opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"");
    const struct usipy_str username = USIPY_2STR("Mufasa");
    const struct usipy_str password = USIPY_2STR("Circle Of Life");
    const struct usipy_str method = USIPY_2STR("GET");
    const struct usipy_str uri = USIPY_2STR("/dir/index.html");
    const struct usipy_str qop = USIPY_2STR("auth");
    union usipy_sip_hdr_parsed up;
    struct usipy_sip_hdr_auth *challengep;
    struct usipy_sip_hdr_authz *authzp;
    const struct usipy_hdr_db_entr *hfp;
    struct usipy_str parts[6], ha1s, ha2s;
    int blen;

    usipy_msg_heap_init(&chall_heap, chall_storage, sizeof(chall_storage),
      chall_cpts, sizeof(chall_cpts) / sizeof(chall_cpts[0]));
    usipy_msg_heap_init(&auth_heap, auth_storage, sizeof(auth_storage),
      auth_cpts, sizeof(auth_cpts) / sizeof(auth_cpts[0]));
    challengep = usipy_sip_hdr_auth_parse(&chall_heap, &challenge_noqop).auth;
    assert(challengep != NULL);
    authzp = gen_auth_hf(&auth_heap, challengep, &username, &password, &method,
      &uri, NULL, NULL);
    assert(authzp != NULL);
    hfp = usipy_hdr_db_byid(USIPY_HF_AUTHORIZATION);
    assert(hfp->build != NULL);
    up.authz = authzp;
    blen = hfp->build(&up, buf, sizeof(buf));
    assert(blen > 0);
    assert((size_t)blen == sizeof("Digest username=\"Mufasa\",realm=\"testrealm@host.com\",nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\",uri=\"/dir/index.html\",response=\"670fd8c2df070c60b045671b8b24ff02\",opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"") - 1);
    assert(memcmp(buf,
      "Digest username=\"Mufasa\",realm=\"testrealm@host.com\",nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\",uri=\"/dir/index.html\",response=\"670fd8c2df070c60b045671b8b24ff02\",opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"",
      (size_t)blen) == 0);

    usipy_msg_heap_init(&auth_heap, auth_storage, sizeof(auth_storage),
      auth_cpts, sizeof(auth_cpts) / sizeof(auth_cpts[0]));
    challengep = usipy_sip_hdr_auth_parse(&chall_heap, &challenge_qop).auth;
    assert(challengep != NULL);
    authzp = gen_auth_hf(&auth_heap, challengep, &username, &password, &method,
      &uri, NULL, &qop);
    assert(authzp != NULL);
    assert(authzp->cnonce.l == 8);
    up.authz = authzp;
    blen = hfp->build(&up, buf, sizeof(buf));
    assert(blen > 0);
    parts[0] = username;
    parts[1] = challengep->realm;
    parts[2] = password;
    md5_hex_join(ha1, 3, parts);
    parts[0] = method;
    parts[1] = uri;
    md5_hex_join(ha2, 2, parts);
    ha1s = (struct usipy_str){.s.ro = ha1, .l = MD5_SIZE_FORMATTED - 1};
    ha2s = (struct usipy_str){.s.ro = ha2, .l = MD5_SIZE_FORMATTED - 1};
    parts[0] = ha1s;
    parts[1] = challengep->nonce;
    parts[2] = authzp->nc;
    parts[3] = authzp->cnonce;
    parts[4] = qop;
    parts[5] = ha2s;
    md5_hex_join(response, 6, parts);
    assert(snprintf(expbuf, sizeof(expbuf),
      "Digest username=\"%.*s\",realm=\"%.*s\",nonce=\"%.*s\",uri=\"%.*s\",response=\"%s\",algorithm=%.*s,qop=%.*s,nc=%.*s,cnonce=\"%.*s\",opaque=\"%.*s\"",
      USIPY_SFMT(&username), USIPY_SFMT(&challengep->realm),
      USIPY_SFMT(&challengep->nonce), USIPY_SFMT(&uri), response,
      USIPY_SFMT(&challengep->algorithm), USIPY_SFMT(&qop),
      USIPY_SFMT(&authzp->nc), USIPY_SFMT(&authzp->cnonce),
      USIPY_SFMT(&challengep->opaque)) == blen);
    assert(memcmp(buf, expbuf, (size_t)blen) == 0);
}

static void
test_register_expires_helpers(void)
{
    static const char msgbuf[] =
      "SIP/2.0 200 OK\r\n"
      "Via: SIP/2.0/UDP 127.0.0.1;branch=z9hG4bK-a\r\n"
      "From: <sip:alice@example.test>;tag=f1\r\n"
      "To: <sip:alice@example.test>;tag=t1\r\n"
      "Call-ID: reg-1@example.test\r\n"
      "CSeq: 1 REGISTER\r\n"
      "Contact: <sip:alice@example.test>;expires=120\r\n"
      "Contact: <sip:bob@example.test>\r\n"
      "Expires: 300\r\n"
      "Content-Length: 0\r\n"
      "\r\n";
    struct usipy_msg_parse_err perr = USIPY_MSG_PARSE_ERR_init;
    struct usipy_sip_hdr_match *contact_hdrs;
    struct usipy_msg *msg;
    unsigned int expires = 0;
    const struct usipy_str alice = USIPY_2STR("alice");
    const struct usipy_str bob = USIPY_2STR("bob");
    const struct usipy_str carol = USIPY_2STR("carol");

    msg = usipy_sip_msg_ctor_fromwire(msgbuf, sizeof(msgbuf) - 1, &perr);
    assert(msg != NULL);
    contact_hdrs = __builtin_alloca(USIPY_SIP_HDR_MATCH_SIZE(8));
    contact_hdrs->hdrslen = 8;
    assert(usipy_sip_msg_parse_hdrs_get(msg, USIPY_HFT_MASK(USIPY_HF_CONTACT), 0,
      contact_hdrs) == 0);
    assert(contact_hdrs->nhdrs == 2);

    assert(usipy_tm_uac_extract_register_expires(msg, &alice, &expires) == 0);
    assert(expires == 120);

    assert(usipy_tm_uac_extract_register_expires(msg, &bob, &expires) == 0);
    assert(expires == 300);

    assert(usipy_tm_uac_extract_register_expires(msg, &carol, &expires) == 0);
    assert(expires == 300);

    usipy_sip_msg_dtor(msg);
}

static void
uac_timeout(void *arg, size_t tx_index, const struct usipy_sip_tm_tx *txp)
{
    struct test_cbarg *carg = arg;

    (void)tx_index;
    carg->ntimeouts += 1;
    if (carg->auth_retry_started && txp->common.id.cseq == 3) {
        carg->stop = 1;
    }
}

static int
bind_loopback_udp(void)
{
    struct sockaddr_in sin;
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock >= 0);
    memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(0);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(sock, (const struct sockaddr *)&sin, sizeof(sin)) == 0);
    return (sock);
}

static struct usipy_sip_hdr *
append_parsed_header(struct usipy_msg *mp, const struct usipy_hdr_db_entr *hfp)
{
    struct usipy_sip_hdr *nhdrs;
    const size_t nsize = sizeof(mp->hdrs[0]) * (mp->nhdrs + 1);

    assert(mp != NULL);
    assert(hfp != NULL);

    nhdrs = usipy_msg_heap_alloc(&mp->heap, nsize);
    assert(nhdrs != NULL);
    memcpy(nhdrs, mp->hdrs, sizeof(mp->hdrs[0]) * mp->nhdrs);
    mp->hdrs = nhdrs;
    memset(&mp->hdrs[mp->nhdrs], '\0', sizeof(mp->hdrs[0]));
    mp->hdrs[mp->nhdrs].hf_type = hfp;
    mp->hdr_masks.present |= USIPY_HFT_MASK(hfp->cantype);
    mp->nhdrs += 1;
    return (&mp->hdrs[mp->nhdrs - 1]);
}

static void
prepare_msg_for_build(struct usipy_msg *mp)
{
    uint64_t present_mask = 0;
    uint64_t parsed_mask = 0;

    assert(mp != NULL);
    for (unsigned int i = 0; i < mp->nhdrs; i++) {
        struct usipy_sip_hdr *shp = &mp->hdrs[i];

        present_mask |= USIPY_HFT_MASK(shp->hf_type->cantype);
        if (shp->hf_type->parse != NULL) {
            shp->parsed = shp->hf_type->parse(&mp->heap, &shp->onwire.value);
            assert(shp->parsed.generic != NULL);
            parsed_mask |= USIPY_HFT_MASK(shp->hf_type->cantype);
            continue;
        }
        shp->parsed.generic = &shp->onwire.value;
    }
    mp->hdr_masks.present |= present_mask;
    mp->hdr_masks.parsed |= parsed_mask;
}

static void
set_header_tag(struct usipy_msg *mp, int htype, const struct usipy_str *tagp)
{
    struct usipy_str hvalue;

    assert(mp != NULL);
    assert(tagp != NULL);
    for (unsigned int i = 0; i < mp->nhdrs; i++) {
        struct usipy_sip_hdr *shp = &mp->hdrs[i];

        if (shp->hf_type->cantype != htype) {
            continue;
        }
        if (shp->parsed.to == NULL) {
            shp->parsed.to = usipy_sip_hdr_nameaddr_parse(&mp->heap,
              &shp->onwire.value).to;
            assert(shp->parsed.to != NULL);
        }
        assert(usipy_msg_heap_sprintf(&mp->heap, &hvalue, "%.*s%.*s",
          USIPY_SFMT(&shp->onwire.value), USIPY_SFMT(tagp)) == 0);
        shp->parsed.to = usipy_sip_hdr_nameaddr_parse(&mp->heap, &hvalue).to;
        assert(shp->parsed.to != NULL);
        mp->hdr_masks.parsed |= USIPY_HFT_MASK(htype);
        return;
    }
    assert(0);
}

static void
append_www_authenticate(struct usipy_msg *mp, const struct usipy_str *valuep)
{
    struct usipy_sip_hdr *shp;

    assert(mp != NULL);
    assert(valuep != NULL);
    shp = append_parsed_header(mp, usipy_hdr_db_byid(USIPY_HF_WWWAUTHENTICATE));
    shp->parsed.auth = usipy_sip_hdr_auth_parse(&mp->heap, valuep).auth;
    assert(shp->parsed.auth != NULL);
    mp->hdr_masks.parsed |= USIPY_HFT_MASK(USIPY_HF_WWWAUTHENTICATE);
}

static struct usipy_msg *
build_unauth_response(const struct usipy_msg *reqp, const struct usipy_sip_status *slp,
  const struct usipy_str *to_tagp, const struct usipy_str *www_authp)
{
    struct usipy_msg *rp;
    struct usipy_str onwire;

    assert(reqp != NULL);
    assert(slp != NULL);
    assert(to_tagp != NULL);
    assert(www_authp != NULL);
    rp = usipy_sip_res_ctor_fromreq(reqp, slp);
    assert(rp != NULL);
    prepare_msg_for_build(rp);
    set_header_tag(rp, USIPY_HF_TO, to_tagp);
    append_www_authenticate(rp, www_authp);
    assert(usipy_sip_msg_build(&rp->heap, rp, &onwire) == 0);
    return (rp);
}

int
main(void)
{
    struct test_cbarg carg = {0};
    const struct usipy_sip_status unauth_status = {
      .code = 401,
      .reason_phrase = (struct usipy_str)USIPY_2STR("Unauthorized"),
    };
    const struct usipy_str to_tag = USIPY_2STR(";tag=r");
    const struct usipy_str www_auth = USIPY_2STR(
      "Digest realm=\"example.test\",nonce=\"abcdef\",algorithm=MD5,qop=auth");
    struct usipy_sip_tm_new_transaction_params tp = {
      .call_id = (struct usipy_str)USIPY_2STR("reg-1@example.test"),
      .cseq = 1,
      .method_type = USIPY_SIP_METHOD_REGISTER,
      .request_uri = (struct usipy_str)USIPY_2STR("sip:registrar.example.test"),
      .target = {
        .af = AF_INET,
        .port = 5060,
        .transport = USIPY_SIP_TM_TRANSPORT_UDP,
        .host = (struct usipy_str)USIPY_2STR("127.0.0.1"),
      },
      .contact_username = (struct usipy_str)USIPY_2STR("alice"),
      .from_username = (struct usipy_str)USIPY_2STR("alice"),
      .to_username = (struct usipy_str)USIPY_2STR("alice"),
      .callbacks = {
        .arg = &carg,
        .response = uac_response,
        .timeout = uac_timeout,
      },
    };
    struct usipy_sip_tm_handle_incoming_in hin;
    struct usipy_sip_tm_handle_incoming_out hout;
    struct usipy_sip_tm_run_in rin;
    struct usipy_sip_tm_run_out rout;
    struct usipy_sip_tm_ctor_params tm_ctorp;
    struct usipy_sip_tm_tx *txp;
    struct usipy_msg_parse_err perr = USIPY_MSG_PARSE_ERR_init;
    struct usipy_msg *reqp, *resp;
    struct usipy_sip_tid tid;
    size_t total_sent = 0, total_timeouts = 0;
    int response_injected = 0;
    struct usipy_sip_tm *tm;
    size_t tx_index;
    int second_started = 0;
    int sock;

    test_gen_auth_hf();
    test_register_expires_helpers();
    carg.username = (struct usipy_str)USIPY_2STR("alice");
    carg.password = (struct usipy_str)USIPY_2STR("secret");
    carg.qop = (struct usipy_str)USIPY_2STR("auth");
    carg.extra_hdrs[0].hf_type = USIPY_HF_EXPIRES;
    carg.extra_hdrs[0].value = (struct usipy_str)USIPY_2STR("60");
    carg.start_ms = usipy_tm_uac_mono_ms();
    sock = bind_loopback_udp();
    memset(&tm_ctorp, '\0', sizeof(tm_ctorp));
    tm_ctorp.sock = sock;
    tm_ctorp.transport = USIPY_SIP_TM_TRANSPORT_UDP;
    tm_ctorp.max_transactions = 4;
    tm = usipy_sip_tm_ctor(&tm_ctorp);
    assert(tm != NULL);
    carg.tm = tm;
    assert(usipy_sip_tm_new_transaction(tm, &tp, &tx_index) == USIPY_SIP_TM_OK);
    txp = (struct usipy_sip_tm_tx *)usipy_sip_tm_get_transaction(tm, tx_index);
    assert(txp != NULL);
    txp->common.timers.t1_ms = 50;
    txp->common.timers.t2_ms = 200;
    txp->common.timers.timer_f_ms = 500;
    rin.tm = tm;
    rin.send_to = stdout_send_to;
    rin.send_to_arg = &carg;
    memset(&hin, '\0', sizeof(hin));
    hin.tm = tm;
    while (!carg.stop) {
        rin.now_ms = usipy_tm_uac_mono_ms();
        assert(usipy_sip_tm_run(&rin, &rout) == USIPY_SIP_TM_OK);
        total_sent += rout.nsent;
        total_timeouts += rout.ntimeouts;
        if (!second_started && carg.ntimeouts == 1) {
            assert(usipy_sip_tm_next_transaction(tm, tx_index, &carg.extra_hdrs[0], 1) ==
              USIPY_SIP_TM_OK);
            second_started = 1;
            txp = (struct usipy_sip_tm_tx *)usipy_sip_tm_get_transaction(tm, tx_index);
            assert(txp != NULL);
            txp->common.timers.t1_ms = 50;
            txp->common.timers.t2_ms = 200;
            txp->common.timers.timer_f_ms = 500;
            continue;
        }
        if (second_started && !response_injected && txp->common.id.cseq == 2 &&
          txp->common.retransmit_count == 1) {
            reqp = usipy_sip_msg_ctor_fromwire(txp->common.outbound.raw.s.ro,
              txp->common.outbound.raw.l, &perr);
            assert(reqp != NULL);
            assert(usipy_sip_msg_get_tid(reqp, &tid) == 0);
            assert(tid.cseq != NULL);
            assert(tid.cseq->val == 2);
            resp = build_unauth_response(reqp, &unauth_status, &to_tag, &www_auth);
            assert(fwrite(resp->onwire.s.ro, 1, resp->onwire.l, stdout) == resp->onwire.l);
            hin.now_ms = usipy_tm_uac_mono_ms();
            hin.peer = tp.target;
            hin.local = txp->common.local;
            hin.buf = resp->onwire.s.ro;
            hin.len = resp->onwire.l;
            assert(usipy_sip_tm_handle_incoming(&hin, &hout) == USIPY_SIP_TM_OK);
            assert(hout.error == USIPY_SIP_TM_OK);
            assert(hout.consumed != 0);
            assert(hout.match_kind == USIPY_SIP_TM_MATCH_EXISTING);
            assert(hout.event == USIPY_SIP_TM_EVENT_RESPONSE_FINAL);
            assert(hout.transaction_index == tx_index);
            assert(carg.nresponses == 1);
            assert(carg.last_status_code == 401);
            response_injected = 1;
            txp = (struct usipy_sip_tm_tx *)usipy_sip_tm_get_transaction(tm, tx_index);
            assert(txp != NULL);
            usipy_sip_msg_dtor(resp);
            usipy_sip_msg_dtor(reqp);
            continue;
        }
        if (!carg.stop) {
            assert(rout.next_run_at_ms != USIPY_SIP_TM_TIME_NONE);
            usipy_tm_uac_sleep_until_ms(rout.next_run_at_ms);
        }
    }
    assert(txp->common.outbound.raw.s.ro == NULL);
    assert(second_started != 0);
    assert(carg.auth_retry_started != 0);
    assert(response_injected != 0);
    assert(total_sent == 9);
    assert(carg.nsent == 9);
    assert(carg.nresponses == 1);
    assert(carg.last_status_code == 401);
    assert(total_timeouts == 2);
    assert(carg.ntimeouts == 2);
    assert(txp->common.id.cseq == 3);
    assert(txp->common.retransmit_count == 4);
    assert(txp->common.outbound.next_send_at_ms == USIPY_SIP_TM_TIME_NONE);
    assert(txp->state == USIPY_SIP_TM_STATE_TERMINATED);
    usipy_sip_tm_dtor(tm);
    close(sock);
    return (0);
}
