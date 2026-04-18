#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "public/usipy_sip_msg.h"
#include "public/usipy_str.h"
#include "usipy_sip_method_db.h"
#include "usipy_sip_res.h"
#include "usipy_tvpair.h"
#include "usipy_sip_uri.h"
#include "usipy_tm_uac.h"
#include "usipy_tm_uac_cli.h"

enum ua_cli_id_mode {
    UA_CLI_ID_DEBUG = 0,
    UA_CLI_ID_PRODUCTION
};

struct ua_cli_ctx {
    int sock;
    int stop;
    int error;
    int report_activity;
    int auth_retry_started;
    int registering;
    int registered_once;
    size_t max_transactions;
    uint16_t register_status;
    uint32_t requested_expires;
    uint32_t next_register_cseq;
    unsigned int expires;
    uint64_t next_refresh_at_ms;
    uint64_t hangup_at_ms;
    size_t invite_index;
    struct usipy_sip_tm *tm;
    struct usipy_sip_dialog *dialogp;
    struct usipy_sip_tm_addr peer;
    struct usipy_sip_tm_addr local;
    struct usipy_str username;
    struct usipy_str password;
    struct usipy_str qop;
    struct usipy_str request_uri;
    struct usipy_str call_id;
    struct usipy_str sdp;
    const char *stop_reason;
    char sdp_buf[512];
};

static const struct usipy_sip_status ua_cli_res_forbidden = {
  .code = 403,
  .reason_phrase = USIPY_2STR("Forbidden"),
};

static const struct usipy_sip_status ua_cli_res_not_found = {
  .code = 404,
  .reason_phrase = USIPY_2STR("Not Found"),
};

static void register_response(void *, size_t, const struct usipy_sip_tm_tx *,
  const struct usipy_msg *);
static void register_timeout(void *, size_t, const struct usipy_sip_tm_tx *,
  enum usipy_sip_tm_uac_timeout_id);

static const char *sip_tm_err_name(int);
static void format_first_line(char *, size_t, const char *, size_t);

static void
usage(const char *argv0)
{
    fprintf(stderr,
      "usage: %s [--activity] [--id-policy debug|production] [--expires seconds] [--port port] [--timeout-ms ms] server-ip username password\n",
      argv0);
}

static const char *
sip_tm_err_name(int err)
{
    switch (err) {
    case USIPY_SIP_TM_OK:
        return ("ok");

    case USIPY_SIP_TM_ERR_PARSE:
        return ("parse");

    case USIPY_SIP_TM_ERR_BADMSG:
        return ("badmsg");

    case USIPY_SIP_TM_ERR_NOT_FOUND:
        return ("not-found");

    case USIPY_SIP_TM_ERR_UNSUPPORTED:
        return ("unsupported");

    case USIPY_SIP_TM_ERR_INVAL:
        return ("invalid");

    case USIPY_SIP_TM_ERR_NOMEM:
        return ("nomem");

    case USIPY_SIP_TM_ERR_NOSPC:
        return ("nospc");
    }
    return ("unknown");
}

static void
format_first_line(char *buf, size_t len, const char *raw, size_t rawlen)
{
    size_t linelen;

    if (buf == NULL || len == 0) {
        return;
    }
    buf[0] = '\0';
    if (raw == NULL || rawlen == 0) {
        return;
    }
    linelen = 0;
    while (linelen < rawlen && raw[linelen] != '\r' && raw[linelen] != '\n') {
        linelen += 1;
    }
    if (linelen >= len) {
        linelen = len - 1;
    }
    memcpy(buf, raw, linelen);
    buf[linelen] = '\0';
}

static void
report_activityf(const struct ua_cli_ctx *ctx, const char *fmt, ...)
{
    va_list ap;

    if (ctx == NULL || !ctx->report_activity) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
    fflush(stdout);
}

static int
socket_send_to(void *arg, size_t tx_index, const struct usipy_sip_tm_tx *txp,
  const struct usipy_sip_tm_outbound *outp)
{
    const struct ua_cli_ctx *ctx = arg;
    ssize_t sent;

    (void)tx_index;
    (void)txp;
    sent = send(ctx->sock, outp->raw.s.ro, outp->raw.l, 0);
    return (sent == (ssize_t)outp->raw.l ? 0 : -1);
}

static int
peer_matches_server(const struct ua_cli_ctx *ctx, const struct usipy_sip_tm_addr *peerp)
{
    if (ctx == NULL || peerp == NULL) {
        return (0);
    }
    return (peerp->af == ctx->peer.af &&
      peerp->port == ctx->peer.port &&
      peerp->transport == ctx->peer.transport &&
      usipy_str_eq(&peerp->host, &ctx->peer.host));
}

static int
request_targets_user(const struct usipy_msg *msg, const struct usipy_str *usernamep)
{
    const struct usipy_sip_uri *urip;

    if (msg == NULL || usernamep == NULL || msg->kind != USIPY_SIP_MSG_REQ) {
        return (0);
    }
    urip = msg->sline.parsed.rl.ruri;
    if (urip == NULL) {
        return (0);
    }
    return (usipy_str_eq(&urip->user, usernamep));
}

static int
build_fixed_sdp(struct ua_cli_ctx *ctx)
{
    const char *net_type;
    int blen;

    if (ctx == NULL) {
        return (-1);
    }
    net_type = ctx->local.af == AF_INET6 ? "IP6" : "IP4";
    blen = snprintf(ctx->sdp_buf, sizeof(ctx->sdp_buf),
      "v=0\r\n"
      "o=- 0 0 IN %s %.*s\r\n"
      "s=ua_cli\r\n"
      "c=IN %s %.*s\r\n"
      "t=0 0\r\n"
      "m=audio 40000 RTP/AVP 0\r\n"
      "a=rtpmap:0 PCMU/8000\r\n",
      net_type, USIPY_SFMT(&ctx->local.host), net_type, USIPY_SFMT(&ctx->local.host));
    if (blen < 0 || (size_t)blen >= sizeof(ctx->sdp_buf)) {
        return (-1);
    }
    ctx->sdp = (struct usipy_str){.s.ro = ctx->sdp_buf, .l = (size_t)blen};
    return (0);
}

static void
clear_call(struct ua_cli_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->dialogp != NULL) {
        usipy_sip_dialog_dtor(ctx->dialogp);
        ctx->dialogp = NULL;
    }
    if (ctx->invite_index != USIPY_SIP_TM_TX_INDEX_NONE) {
        const struct usipy_sip_tm_tx *txp = usipy_sip_tm_get_transaction(ctx->tm,
          ctx->invite_index);

        if (txp != NULL) {
            (void)usipy_sip_tm_drop_transaction(ctx->tm, ctx->invite_index);
        }
        ctx->invite_index = USIPY_SIP_TM_TX_INDEX_NONE;
    }
    ctx->hangup_at_ms = USIPY_SIP_TM_TIME_NONE;
}

static int
schedule_refresh(struct ua_cli_ctx *ctx, uint64_t now_ms)
{
    uint64_t refresh_delay_s;

    if (ctx == NULL || ctx->expires == 0) {
        return (-1);
    }
    if (ctx->expires > 90) {
        refresh_delay_s = ctx->expires - 30u;
    } else {
        refresh_delay_s = ctx->expires / 2u;
    }
    if (refresh_delay_s == 0) {
        refresh_delay_s = 1;
    }
    ctx->next_refresh_at_ms = now_ms + (refresh_delay_s * 1000u);
    return (0);
}

static int
start_register(struct ua_cli_ctx *ctx, size_t *indexp)
{
    struct usipy_sip_tm_new_uac_tr_params tp = {0};
    size_t tx_index;
    int rval;

    if (ctx == NULL || ctx->tm == NULL || ctx->registering || indexp == NULL) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    tp.request_id.call_id = ctx->call_id;
    tp.request_id.cseq = ctx->next_register_cseq;
    tp.request_id.method_type = USIPY_SIP_METHOD_REGISTER;
    tp.request_target.request_uri = ctx->request_uri;
    tp.request_target.target = ctx->peer;
    tp.parties_by_username.from = ctx->username;
    tp.parties_by_username.to = ctx->username;
    tp.parties_by_username.contact = ctx->username;
    tp.contact_expires = ctx->requested_expires;
    tp.callbacks.arg = ctx;
    tp.callbacks.response = register_response;
    tp.callbacks.timeout = register_timeout;
    ctx->auth_retry_started = 0;
    ctx->registering = 1;
    ctx->register_status = 0;
    report_activityf(ctx, "register cseq=%u", tp.request_id.cseq);
    rval = usipy_sip_tm_new_uac_tr(ctx->tm, &tp, &tx_index);
    if (rval != USIPY_SIP_TM_OK) {
        ctx->registering = 0;
        return (rval);
    }
    *indexp = tx_index;
    return (USIPY_SIP_TM_OK);
}

static int
send_simple_response(struct ua_cli_ctx *ctx,
  const struct usipy_sip_tm_handle_incoming_in *hin, const struct usipy_msg *msg,
  const struct usipy_sip_status *statusp)
{
    struct usipy_sip_tm_new_uas_tr_params tp = {
      .request = msg,
      .timers = hin->timers,
      .peer = hin->peer,
      .local = hin->local,
    };
    struct usipy_sip_tm_uas_response_params rp = {
      .status = *statusp,
    };
    size_t tx_index;
    int rval;

    rval = usipy_sip_tm_new_uas_tr(ctx->tm, &tp, &tx_index);
    if (rval != USIPY_SIP_TM_OK) {
        return (rval);
    }
    return (usipy_sip_tm_send_uas_response(ctx->tm, tx_index, &rp));
}

static int
send_stateless_response(const struct ua_cli_ctx *ctx, const struct usipy_msg *msg,
  const struct usipy_sip_status *statusp)
{
    struct usipy_msg *resp;
    size_t raw_len;
    ssize_t sent;

    if (ctx == NULL || msg == NULL || statusp == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    resp = usipy_sip_res_ctor_fromreq(msg, statusp);
    if (resp == NULL) {
        return (USIPY_SIP_TM_ERR_NOMEM);
    }
    raw_len = resp->onwire.l;
    sent = send(ctx->sock, resp->onwire.s.ro, raw_len, 0);
    usipy_sip_msg_dtor(resp);
    if (sent != (ssize_t)raw_len) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    return (USIPY_SIP_TM_OK);
}

static void
register_response(void *arg, size_t tx_index, const struct usipy_sip_tm_tx *txp,
  const struct usipy_msg *msg)
{
    struct ua_cli_ctx *ctx = arg;
    const unsigned int scode = msg->sline.parsed.sl.status.code;
    uint64_t now_ms = usipy_tm_uac_mono_ms();

    (void)tx_index;
    ctx->register_status = (uint16_t)scode;
    if ((scode == 401 || scode == 407) && !ctx->auth_retry_started) {
        report_activityf(ctx, "register challenge %u", scode);
        if (usipy_tm_uac_register_reply_auth(ctx->tm, tx_index, msg, &ctx->username,
          &ctx->password, &ctx->qop, NULL, 0) != USIPY_SIP_TM_OK) {
            ctx->error = 1;
            ctx->stop = 1;
            ctx->stop_reason = "register-auth-build";
        } else {
            ctx->auth_retry_started = 1;
            report_activityf(ctx, "register auth-retry");
        }
        return;
    }
    ctx->registering = 0;
    ctx->auth_retry_started = 0;
    ctx->next_register_cseq = txp->common.id.cseq + 1;
    if (scode >= 200 && scode < 300) {
        if (usipy_tm_uac_extract_register_expires(msg, &ctx->username,
          &ctx->expires) != 0 || schedule_refresh(ctx, now_ms) != 0) {
            ctx->error = 1;
            ctx->stop = 1;
            return;
        }
        if (!ctx->registered_once) {
            printf("%u\n", ctx->expires);
            fflush(stdout);
            ctx->registered_once = 1;
        }
        report_activityf(ctx, "registered %u", ctx->expires);
        return;
    }
    report_activityf(ctx, "register failed %u", scode);
    ctx->error = 1;
    ctx->stop = 1;
    ctx->stop_reason = "register-final";
}

static void
register_timeout(void *arg, size_t tx_index, const struct usipy_sip_tm_tx *txp,
  enum usipy_sip_tm_uac_timeout_id timeout_id)
{
    struct ua_cli_ctx *ctx = arg;

    (void)tx_index;
    (void)txp;
    (void)timeout_id;
    ctx->registering = 0;
    report_activityf(ctx, "register timeout");
    ctx->error = 1;
    ctx->stop = 1;
    ctx->stop_reason = "register-timeout";
}

static void
incoming_request(void *arg, const struct usipy_sip_tm_handle_incoming_in *hin,
  const struct usipy_msg *msg)
{
    struct ua_cli_ctx *ctx = arg;
    uint8_t method_type;

    if (ctx == NULL || hin == NULL || msg == NULL || msg->kind != USIPY_SIP_MSG_REQ) {
        return;
    }
    method_type = msg->sline.parsed.rl.method->cantype;
    if (!peer_matches_server(ctx, &hin->peer)) {
        if (send_simple_response(ctx, hin, msg, &ua_cli_res_forbidden) != USIPY_SIP_TM_OK) {
            ctx->error = 1;
            ctx->stop = 1;
            ctx->stop_reason = "forbidden-response";
        } else {
            report_activityf(ctx, "rejected forbidden");
        }
        return;
    }
    if (!request_targets_user(msg, &ctx->username)) {
        if (send_simple_response(ctx, hin, msg, &ua_cli_res_not_found) != USIPY_SIP_TM_OK) {
            ctx->error = 1;
            ctx->stop = 1;
            ctx->stop_reason = "not-found-response";
        } else {
            report_activityf(ctx, "rejected wrong-user");
        }
        return;
    }
    if (method_type == USIPY_SIP_METHOD_INVITE) {
        struct usipy_sip_tm_new_uas_tr_params tp = {
          .request = msg,
          .timers = hin->timers,
          .peer = hin->peer,
          .local = hin->local,
        };
        struct usipy_sip_tm_uas_response_params rp = {
          .status = usipy_sip_res_ok,
          .content_type = USIPY_2STR("application/sdp"),
        };
        size_t tx_index;
        int rval;

        if (ctx->dialogp != NULL) {
            if (send_simple_response(ctx, hin, msg, &usipy_sip_res_busy_here) !=
              USIPY_SIP_TM_OK) {
                ctx->error = 1;
                ctx->stop = 1;
                ctx->stop_reason = "busy-response";
            } else {
                report_activityf(ctx, "rejected busy");
            }
            return;
        }
        report_activityf(ctx, "incoming-call");
        rp.body = ctx->sdp;
        rval = usipy_sip_tm_new_uas_tr(ctx->tm, &tp, &tx_index);
        if (rval != USIPY_SIP_TM_OK) {
            report_activityf(ctx,
              "incoming-call failed: new-uas %s (%d) method=%.*s active=%zu",
              sip_tm_err_name(rval), rval,
              USIPY_SFMT(&msg->sline.parsed.rl.onwire.method),
              usipy_sip_tm_nactive(ctx->tm));
            ctx->error = 1;
            ctx->stop = 1;
            ctx->stop_reason = "invite-new-uas";
            return;
        }
        ctx->dialogp = usipy_sip_dialog_uas_ctor(ctx->tm, tx_index, &rp);
        if (ctx->dialogp == NULL) {
            report_activityf(ctx,
              "incoming-call failed: answer method=%.*s tx=%zu active=%zu",
              USIPY_SFMT(&msg->sline.parsed.rl.onwire.method), tx_index,
              usipy_sip_tm_nactive(ctx->tm));
            ctx->error = 1;
            ctx->stop = 1;
            ctx->stop_reason = "invite-answer";
            return;
        }
        ctx->invite_index = tx_index;
        ctx->hangup_at_ms = hin->now_ms + 60000u;
        report_activityf(ctx, "answered");
        return;
    }
    if (method_type == USIPY_SIP_METHOD_BYE) {
        if (ctx->dialogp == NULL) {
            if (send_simple_response(ctx, hin, msg, &ua_cli_res_not_found) != USIPY_SIP_TM_OK) {
                ctx->error = 1;
                ctx->stop = 1;
                ctx->stop_reason = "bye-not-found-response";
            } else {
                report_activityf(ctx, "rejected no-call");
            }
            return;
        }
        if (send_simple_response(ctx, hin, msg, &usipy_sip_res_ok) != USIPY_SIP_TM_OK) {
            ctx->error = 1;
            ctx->stop = 1;
            ctx->stop_reason = "bye-ok-response";
            return;
        }
        report_activityf(ctx, "hangup remote");
        clear_call(ctx);
        return;
    }
    if (send_stateless_response(ctx, msg, &usipy_sip_res_not_impl) != USIPY_SIP_TM_OK) {
        ctx->error = 1;
        ctx->stop = 1;
        ctx->stop_reason = "unsupported-request";
    } else {
        report_activityf(ctx, "rejected unsupported: %.*s",
          USIPY_SFMT(&msg->sline.parsed.rl.onwire.method));
    }
}

static void
reap_terminated(struct ua_cli_ctx *ctx)
{
    size_t i;

    if (ctx == NULL || ctx->tm == NULL) {
        return;
    }
    for (i = 0; i < ctx->max_transactions; i++) {
        const struct usipy_sip_tm_tx *txp = usipy_sip_tm_get_transaction(ctx->tm, i);

        if (txp == NULL || txp->state != USIPY_SIP_TM_STATE_TERMINATED) {
            continue;
        }
        (void)usipy_sip_tm_drop_transaction(ctx->tm, i);
    }
}

int
main(int argc, char **argv)
{
    enum ua_cli_id_mode id_mode = UA_CLI_ID_PRODUCTION;
    struct usipy_tm_uac_production_ids production_ids = {0};
    struct ua_cli_ctx ctx = {
      .sock = -1,
      .max_transactions = 32,
      .qop = (struct usipy_str)USIPY_2STR("auth"),
      .next_register_cseq = 1,
      .next_refresh_at_ms = USIPY_SIP_TM_TIME_NONE,
      .hangup_at_ms = USIPY_SIP_TM_TIME_NONE,
      .invite_index = USIPY_SIP_TM_TX_INDEX_NONE,
    };
    struct usipy_sip_tm_ctor_params tm_ctorp = {
      .transport = USIPY_SIP_TM_TRANSPORT_UDP,
      .max_transactions = ctx.max_transactions,
      .callbacks = {
        .arg = &ctx,
        .incoming_request = incoming_request,
      },
    };
    struct sockaddr_storage peer_ss;
    socklen_t peer_slen = 0;
    struct usipy_sip_tm_run_in rin;
    struct usipy_sip_tm_run_out rout;
    struct usipy_sip_tm_handle_incoming_in hin;
    struct usipy_sip_tm_handle_incoming_out hout;
    struct pollfd pfd;
    const struct usipy_sip_tm_tx *txp;
    size_t reg_index;
    char uri_host[INET6_ADDRSTRLEN + 2];
    char request_uri_buf[128];
    char debug_call_id[96];
    char rxbuf[2048];
    const char *server_ip;
    const char *username;
    const char *password;
    uint32_t timeout_ms = 0;
    uint32_t port = 5060;
    uint64_t register_deadline_ms = USIPY_SIP_TM_TIME_NONE;
    const char *exit_reason = "normal";
    int blen;
    int argi = 1;
    int rval = 1;

    while (argi < argc && strncmp(argv[argi], "--", 2) == 0) {
        if (strcmp(argv[argi], "--activity") == 0) {
            ctx.report_activity = 1;
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--id-policy") == 0) {
            if (++argi >= argc) {
                usage(argv[0]);
                return (1);
            }
            if (strcmp(argv[argi], "debug") == 0) {
                id_mode = UA_CLI_ID_DEBUG;
            } else if (strcmp(argv[argi], "production") == 0) {
                id_mode = UA_CLI_ID_PRODUCTION;
            } else {
                usage(argv[0]);
                return (1);
            }
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--expires") == 0) {
            if (++argi >= argc ||
              usipy_tm_uac_cli_parse_u32(argv[argi], 1, UINT32_MAX,
                &ctx.requested_expires) != 0) {
                usage(argv[0]);
                return (1);
            }
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--port") == 0) {
            if (++argi >= argc ||
              usipy_tm_uac_cli_parse_u32(argv[argi], 1, 65535, &port) != 0) {
                usage(argv[0]);
                return (1);
            }
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--timeout-ms") == 0) {
            if (++argi >= argc ||
              usipy_tm_uac_cli_parse_u32(argv[argi], 1, UINT32_MAX, &timeout_ms) != 0) {
                usage(argv[0]);
                return (1);
            }
            argi++;
            continue;
        }
        usage(argv[0]);
        return (1);
    }
    if (argc - argi != 3) {
        usage(argv[0]);
        return (1);
    }
    if (ctx.requested_expires == 0) {
        ctx.requested_expires = 300;
    }
    server_ip = argv[argi++];
    username = argv[argi++];
    password = argv[argi++];
    ctx.username = (struct usipy_str){.s.ro = username, .l = strlen(username)};
    ctx.password = (struct usipy_str){.s.ro = password, .l = strlen(password)};
    if (usipy_tm_uac_cli_parse_target(server_ip, (uint16_t)port, &peer_ss, &peer_slen,
      &ctx.peer) != 0) {
        fprintf(stderr, "invalid server ip: %s\n", server_ip);
        return (1);
    }
    if (usipy_tm_uac_cli_format_uri_host(uri_host, sizeof(uri_host), &ctx.peer) < 0) {
        fprintf(stderr, "unable to format URI host\n");
        return (1);
    }
    blen = (port == 5060 ?
      snprintf(request_uri_buf, sizeof(request_uri_buf), "sip:%s", uri_host) :
      snprintf(request_uri_buf, sizeof(request_uri_buf), "sip:%s:%u", uri_host, port));
    if (blen < 0 || (size_t)blen >= sizeof(request_uri_buf)) {
        fprintf(stderr, "unable to format request URI\n");
        return (1);
    }
    ctx.request_uri = (struct usipy_str){.s.ro = request_uri_buf, .l = (size_t)blen};
    ctx.sock = socket(ctx.peer.af, SOCK_DGRAM, 0);
    if (ctx.sock < 0) {
        perror("socket");
        return (1);
    }
    if (connect(ctx.sock, (const struct sockaddr *)&peer_ss, peer_slen) != 0) {
        perror("connect");
        exit_reason = "connect";
        goto done;
    }
    if (id_mode == UA_CLI_ID_PRODUCTION) {
        if (usipy_tm_uac_production_ids_init(&production_ids) != 0) {
            fprintf(stderr, "unable to initialize production identifiers\n");
            exit_reason = "id-init";
            goto done;
        }
        tm_ctorp.id_policy.arg = &production_ids;
        tm_ctorp.id_policy.cb = usipy_tm_uac_production_id_policy;
        ctx.call_id = production_ids.call_id_s;
    } else {
        blen = snprintf(debug_call_id, sizeof(debug_call_id), "ua-1@%s", server_ip);
        if (blen < 0 || (size_t)blen >= sizeof(debug_call_id)) {
            fprintf(stderr, "unable to initialize debug call-id\n");
            exit_reason = "debug-call-id";
            goto done;
        }
        ctx.call_id = (struct usipy_str){.s.ro = debug_call_id, .l = (size_t)blen};
    }
    tm_ctorp.sock = ctx.sock;
    if (id_mode != UA_CLI_ID_PRODUCTION) {
        memset(&tm_ctorp.id_policy, '\0', sizeof(tm_ctorp.id_policy));
    }
    ctx.tm = usipy_sip_tm_ctor(&tm_ctorp);
    if (ctx.tm == NULL) {
        fprintf(stderr, "unable to initialize SIP TM\n");
        exit_reason = "tm-ctor";
        goto done;
    }
    if (start_register(&ctx, &reg_index) != USIPY_SIP_TM_OK) {
        fprintf(stderr, "unable to create REGISTER transaction\n");
        exit_reason = "register-start";
        goto done;
    }
    txp = usipy_sip_tm_get_transaction(ctx.tm, reg_index);
    if (txp == NULL) {
        fprintf(stderr, "unable to fetch REGISTER transaction\n");
        exit_reason = "register-fetch";
        goto done;
    }
    ctx.local = txp->common.local;
    if (build_fixed_sdp(&ctx) != 0) {
        fprintf(stderr, "unable to build fixed SDP\n");
        exit_reason = "build-sdp";
        goto done;
    }
    memset(&rin, '\0', sizeof(rin));
    rin.tm = ctx.tm;
    rin.send_to = socket_send_to;
    rin.send_to_arg = &ctx;
    memset(&hin, '\0', sizeof(hin));
    hin.tm = ctx.tm;
    hin.peer = ctx.peer;
    hin.local = ctx.local;
    if (timeout_ms != 0) {
        register_deadline_ms = usipy_tm_uac_mono_ms() + timeout_ms;
    }
    while (!ctx.stop) {
        uint64_t now_ms = usipy_tm_uac_mono_ms();
        uint64_t wake_at_ms = USIPY_SIP_TM_TIME_NONE;
        int wait_ms;
        ssize_t nread;
        int poll_r;

        if (!ctx.registered_once && register_deadline_ms != USIPY_SIP_TM_TIME_NONE &&
          now_ms >= register_deadline_ms) {
            fprintf(stderr, "register timeout\n");
            exit_reason = "register-deadline";
            goto done;
        }
        if (!ctx.registering && ctx.next_refresh_at_ms != USIPY_SIP_TM_TIME_NONE &&
          ctx.next_refresh_at_ms <= now_ms &&
          start_register(&ctx, &reg_index) != USIPY_SIP_TM_OK) {
            fprintf(stderr, "unable to refresh registration\n");
            exit_reason = "refresh-start";
            goto done;
        }
        if (ctx.dialogp != NULL && ctx.hangup_at_ms != USIPY_SIP_TM_TIME_NONE &&
          ctx.hangup_at_ms <= now_ms) {
            size_t bye_index;
            struct usipy_sip_tm_run_out bye_out;

            if (usipy_sip_dialog_end(ctx.dialogp, NULL, &bye_index) != USIPY_SIP_TM_OK) {
                fprintf(stderr, "unable to send BYE\n");
                exit_reason = "auto-bye";
                goto done;
            }
            report_activityf(&ctx, "hangup auto");
            rin.now_ms = now_ms;
            if (usipy_sip_tm_run(&rin, &bye_out) != USIPY_SIP_TM_OK) {
                fprintf(stderr, "unable to flush BYE\n");
                exit_reason = "auto-bye-run";
                goto done;
            }
            if (bye_out.nsent == 0) {
                fprintf(stderr, "BYE was not sent\n");
                exit_reason = "auto-bye-unsent";
                goto done;
            }
            clear_call(&ctx);
            reap_terminated(&ctx);
            continue;
        }
        rin.now_ms = now_ms;
        if (usipy_sip_tm_run(&rin, &rout) != USIPY_SIP_TM_OK) {
            fprintf(stderr, "tm run failed\n");
            exit_reason = "tm-run";
            goto done;
        }
        reap_terminated(&ctx);
        if (ctx.stop) {
            break;
        }
        if (rout.next_run_at_ms != USIPY_SIP_TM_TIME_NONE) {
            wake_at_ms = rout.next_run_at_ms;
        }
        if (!ctx.registered_once && register_deadline_ms != USIPY_SIP_TM_TIME_NONE &&
          (wake_at_ms == USIPY_SIP_TM_TIME_NONE || register_deadline_ms < wake_at_ms)) {
            wake_at_ms = register_deadline_ms;
        }
        if (!ctx.registering && ctx.next_refresh_at_ms != USIPY_SIP_TM_TIME_NONE &&
          (wake_at_ms == USIPY_SIP_TM_TIME_NONE || ctx.next_refresh_at_ms < wake_at_ms)) {
            wake_at_ms = ctx.next_refresh_at_ms;
        }
        if (ctx.dialogp != NULL && ctx.hangup_at_ms != USIPY_SIP_TM_TIME_NONE &&
          (wake_at_ms == USIPY_SIP_TM_TIME_NONE || ctx.hangup_at_ms < wake_at_ms)) {
            wake_at_ms = ctx.hangup_at_ms;
        }
        wait_ms = -1;
        if (wake_at_ms != USIPY_SIP_TM_TIME_NONE) {
            wait_ms = (wake_at_ms > now_ms) ? (int)(wake_at_ms - now_ms) : 0;
        }
        pfd.fd = ctx.sock;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll_r = poll(&pfd, 1, wait_ms);
        if (poll_r < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            exit_reason = "poll";
            goto done;
        }
        if (poll_r == 0 || (pfd.revents & POLLIN) == 0) {
            continue;
        }
        nread = recv(ctx.sock, rxbuf, sizeof(rxbuf), 0);
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            exit_reason = "recv";
            goto done;
        }
        hin.now_ms = usipy_tm_uac_mono_ms();
        hin.buf = rxbuf;
        hin.len = (size_t)nread;
        {
            char first_line[160];
            const int in_rval = usipy_sip_tm_handle_incoming(&hin, &hout);

            if (in_rval != USIPY_SIP_TM_OK && hout.error != USIPY_SIP_TM_ERR_NOT_FOUND) {
                format_first_line(first_line, sizeof(first_line), rxbuf, (size_t)nread);
                if (in_rval == USIPY_SIP_TM_ERR_PARSE ||
                  in_rval == USIPY_SIP_TM_ERR_BADMSG ||
                  in_rval == USIPY_SIP_TM_ERR_UNSUPPORTED) {
                    report_activityf(&ctx,
                      "ignored incoming %s (%d) len=%zd first-line=\"%s\"",
                      sip_tm_err_name(in_rval), in_rval, nread, first_line);
                    continue;
                }
                fprintf(stderr,
                  "incoming SIP handling failed: %s (%d), match=%d, event=%d, tx=%zu, first-line=\"%s\"\n",
                  sip_tm_err_name(in_rval), in_rval, hout.match_kind, hout.event,
                  hout.transaction_index, first_line);
                exit_reason = "incoming-fatal";
                goto done;
            }
        }
        reap_terminated(&ctx);
    }
    if (ctx.error) {
        exit_reason = (ctx.stop_reason != NULL ? ctx.stop_reason : "callback-error");
        goto done;
    }
    rval = 0;

done:
    if (ctx.report_activity) {
        report_activityf(&ctx,
          "exit reason=%s stop=%d error=%d registered=%d registering=%d status=%u active=%zu",
          (ctx.stop_reason != NULL ? ctx.stop_reason : exit_reason), ctx.stop,
          ctx.error, ctx.registered_once, ctx.registering, ctx.register_status,
          (ctx.tm != NULL ? usipy_sip_tm_nactive(ctx.tm) : 0));
    }
    clear_call(&ctx);
    if (ctx.tm != NULL) {
        usipy_sip_tm_dtor(ctx.tm);
    }
    if (ctx.sock >= 0) {
        close(ctx.sock);
    }
    return (rval);
}
