#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "usipy_debug.h"
#include "public/usipy_sip_tm.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_method_db.h"
#include "usipy_sip_tid.h"
#include "usipy_sip_tm_internal.h"
#include "usipy_sip_tm_priv.h"

static void usipy_sip_tm_uas_run_out_consider(struct usipy_sip_tm_run_out *, uint64_t);
static void usipy_sip_tm_uas_mark_terminated(struct usipy_sip_tm_txi *);

static uint32_t
usipy_sip_tm_timer_j_ms(const struct usipy_sip_tm_timer_policy *tp)
{
    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(tp->timer_j_ms != 0 || tp->t1_ms != 0);

    if (tp->timer_j_ms != 0) {
        return (tp->timer_j_ms);
    }
    return (tp->t1_ms * 64u);
}

static uint32_t
usipy_sip_tm_timer_h_ms(const struct usipy_sip_tm_timer_policy *tp)
{
    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(tp->t1_ms != 0);

    return (tp->t1_ms * 64u);
}

static uint32_t
usipy_sip_tm_timer_i_ms(const struct usipy_sip_tm_timer_policy *tp)
{
    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(tp->t4_ms != 0);

    return (tp->t4_ms);
}

static int
usipy_sip_tm_uas_is_invite_non2xx_final(const struct usipy_sip_tm_txi *tp)
{
    USIPY_DASSERT(tp != NULL);

    return (tp->cache.method_type == USIPY_SIP_METHOD_INVITE &&
      tp->pub.role_data.uas.last_status_code >= 300);
}

static uint32_t
usipy_sip_tm_uas_invite_next_send_delay_ms(const struct usipy_sip_tm_txi *tp)
{
    uint64_t delay_ms;
    uint32_t t1_ms;
    uint32_t t2_ms;
    uint8_t shift;

    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(tp->pub.common.timers.t1_ms != 0);
    USIPY_DASSERT(tp->pub.common.timers.t2_ms != 0);

    t1_ms = tp->pub.common.timers.t1_ms;
    t2_ms = tp->pub.common.timers.t2_ms;
    shift = tp->pub.common.retransmit_count > 0 ? tp->pub.common.retransmit_count - 1 : 0;
    if (shift >= 31) {
        return (t2_ms);
    }
    delay_ms = ((uint64_t)t1_ms) << shift;
    if (delay_ms > t2_ms) {
        delay_ms = t2_ms;
    }
    return ((uint32_t)delay_ms);
}

static uint32_t
usipy_sip_tm_uas_ack_hash(const struct usipy_sip_tm_txi *tp)
{
    struct usipy_sip_hdr_cseq cseq = {
      .val = tp->pub.common.id.cseq,
      .method = &usipy_method_db[USIPY_SIP_METHOD_ACK],
    };
    struct usipy_sip_tid tid = {
      .call_id = &tp->pub.common.id.call_id,
      .from_tag = &tp->pub.common.id.from_tag,
      .vbranch = &tp->pub.common.id.branch,
      .cseq = &cseq,
    };

    USIPY_DASSERT(tp != NULL);

    return (usipy_sip_tid_hash(&tid));
}

static int
usipy_sip_tm_uas_ack_matches_tx(const struct usipy_sip_tid *tidp,
  const struct usipy_sip_tm_txi *tp)
{
    USIPY_DASSERT(tidp != NULL);
    USIPY_DASSERT(tp != NULL);

    if (tidp->cseq->method->cantype != USIPY_SIP_METHOD_ACK ||
      tp->pub.role != USIPY_SIP_TM_ROLE_UAS ||
      tp->pub.common.id.method_type != USIPY_SIP_METHOD_INVITE ||
      tp->pub.state != USIPY_SIP_TM_STATE_COMPLETED ||
      tp->cache.uas.ack_hash == 0 ||
      tidp->hash != tp->cache.uas.ack_hash) {
        return (0);
    }
    if (!usipy_str_eq(tidp->call_id, &tp->pub.common.id.call_id) ||
      !usipy_str_eq(tidp->from_tag, &tp->pub.common.id.from_tag) ||
      !usipy_str_eq(tidp->vbranch, &tp->pub.common.id.branch)) {
        return (0);
    }
    return (tidp->cseq->val == tp->pub.common.id.cseq);
}

static void
usipy_sip_tm_uas_post_send_invite_final(struct usipy_sip_tm_txi *tp,
  const struct usipy_sip_tm_run_in *inp, struct usipy_sip_tm_run_out *outp)
{
    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(inp != NULL);

    if (tp->pub.common.timer.due_at_ms == USIPY_SIP_TM_TIME_NONE) {
        tp->pub.common.timer.due_at_ms = inp->now_ms + tp->pub.common.timer.value_ms;
    }
    if ((tp->pub.common.flags & USIPY_SIP_TM_F_RELIABLE_TRANSPORT) == 0) {
        tp->outbound.pub.next_send_at_ms = inp->now_ms +
          usipy_sip_tm_uas_invite_next_send_delay_ms(tp);
        tp->pub.common.outbound = tp->outbound.pub;
        usipy_sip_tm_uas_run_out_consider(outp, tp->outbound.pub.next_send_at_ms);
    }
    usipy_sip_tm_uas_run_out_consider(outp, tp->pub.common.timer.due_at_ms);
}

static void
usipy_sip_tm_uas_post_send_final(struct usipy_sip_tm_txi *tp, uint64_t now_ms,
  struct usipy_sip_tm_run_out *outp)
{
    USIPY_DASSERT(tp != NULL);

    if ((tp->pub.common.flags & USIPY_SIP_TM_F_RELIABLE_TRANSPORT) != 0) {
        usipy_sip_tm_uas_mark_terminated(tp);
    } else {
        tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_J;
        tp->pub.common.timer.value_ms = usipy_sip_tm_timer_j_ms(&tp->pub.common.timers);
        tp->pub.common.timer.due_at_ms = now_ms + tp->pub.common.timer.value_ms;
        usipy_sip_tm_uas_run_out_consider(outp, tp->pub.common.timer.due_at_ms);
    }
}

static int
usipy_sip_tm_uas_handle_ack(const struct usipy_sip_tm_handle_incoming_in *inp,
  struct usipy_sip_tm_handle_incoming_out *outp, struct usipy_sip_tm_txi *tp,
  size_t tx_index)
{
    USIPY_DASSERT(inp != NULL);
    USIPY_DASSERT(tp != NULL);

    tp->outbound.pub.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
    tp->pub.common.outbound = tp->outbound.pub;
    if ((tp->pub.common.flags & USIPY_SIP_TM_F_RELIABLE_TRANSPORT) != 0) {
        usipy_sip_tm_uas_mark_terminated(tp);
    } else {
        tp->pub.state = USIPY_SIP_TM_STATE_CONFIRMED;
        tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_I;
        tp->pub.common.timer.value_ms = usipy_sip_tm_timer_i_ms(&tp->pub.common.timers);
        tp->pub.common.timer.due_at_ms = inp->now_ms + tp->pub.common.timer.value_ms;
    }
    if (outp != NULL) {
        outp->error = USIPY_SIP_TM_OK;
        outp->consumed = 1;
        outp->match_kind = USIPY_SIP_TM_MATCH_EXISTING;
        outp->event = USIPY_SIP_TM_EVENT_ACK_RX;
        outp->transaction_index = tx_index;
        outp->message = NULL;
    }
    return (USIPY_SIP_TM_OK);
}

static void
usipy_sip_tm_uas_run_out_consider(struct usipy_sip_tm_run_out *outp, uint64_t when_ms)
{
    USIPY_DASSERT(outp != NULL);

    if (when_ms == USIPY_SIP_TM_TIME_NONE) {
        return;
    }
    if (outp->next_run_at_ms == USIPY_SIP_TM_TIME_NONE || when_ms < outp->next_run_at_ms) {
        outp->next_run_at_ms = when_ms;
    }
}

static void
usipy_sip_tm_uas_mark_terminated(struct usipy_sip_tm_txi *tp)
{
    USIPY_DASSERT(tp != NULL);

    tp->pub.state = USIPY_SIP_TM_STATE_TERMINATED;
    tp->pub.common.flags |= USIPY_SIP_TM_F_TERMINATED;
    tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_NONE;
    tp->outbound.pub.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
    tp->pub.common.outbound = tp->outbound.pub;
}

static void
usipy_sip_tm_uas_scode2str(unsigned int scode, char *res)
{
    res[0] = '0' + (scode / 100);
    res[1] = '0' + (scode % 100) / 10;
    res[2] = '0' + (scode % 10);
    res[3] = ' ';
}

static int
usipy_sip_tm_uas_copy_hdrs(struct usipy_msg_heap *mhp, struct usipy_str **dstpp,
  size_t *ndstp, const struct usipy_sip_hdr *const *srcp, size_t nsrc)
{
    struct usipy_str *dstp;

    USIPY_DASSERT(mhp != NULL);
    USIPY_DASSERT(dstpp != NULL);
    USIPY_DASSERT(ndstp != NULL);

    *dstpp = NULL;
    *ndstp = 0;
    if (nsrc == 0) {
        return (0);
    }
    dstp = usipy_msg_heap_alloc(mhp, sizeof(*dstp) * nsrc);
    if (dstp == NULL) {
        return (-1);
    }
    for (size_t i = 0; i < nsrc; i++) {
        if (usipy_msg_heap_append(mhp, &dstp[i], &srcp[i]->onwire.value) != 0) {
            return (-1);
        }
    }
    *dstpp = dstp;
    *ndstp = nsrc;
    return (0);
}

static int
usipy_sip_tm_uas_cache_request(const struct usipy_msg *reqp, struct usipy_sip_tm_txi *tp)
{
    const struct usipy_sip_hdr *fromp = NULL, *top = NULL, *cseqp = NULL;
    const struct usipy_sip_hdr *viasp[32];
    const struct usipy_sip_hdr *rrsp[32];
    size_t nvias = 0, nrrs = 0;

    USIPY_DASSERT(reqp != NULL);
    USIPY_DASSERT(tp != NULL);

    for (unsigned int i = 0; i < reqp->nhdrs; i++) {
        const struct usipy_sip_hdr *shp = &reqp->hdrs[i];

        switch (shp->hf_type->cantype) {
        case USIPY_HF_VIA:
            USIPY_DASSERT(nvias < sizeof(viasp) / sizeof(viasp[0]));
            viasp[nvias++] = shp;
            break;

        case USIPY_HF_FROM:
            if (fromp == NULL) {
                fromp = shp;
            }
            break;

        case USIPY_HF_TO:
            if (top == NULL) {
                top = shp;
            }
            break;

        case USIPY_HF_CSEQ:
            if (cseqp == NULL) {
                cseqp = shp;
            }
            break;

        case USIPY_HF_RECORDROUTE:
            USIPY_DASSERT(nrrs < sizeof(rrsp) / sizeof(rrsp[0]));
            rrsp[nrrs++] = shp;
            break;
        }
    }
    USIPY_DASSERT(fromp != NULL);
    USIPY_DASSERT(top != NULL);
    USIPY_DASSERT(cseqp != NULL);
    if (nvias == 0 || usipy_msg_heap_append(&tp->scratch, &tp->cache.uas.from,
      &fromp->onwire.value) != 0 ||
      usipy_msg_heap_append(&tp->scratch, &tp->cache.uas.to, &top->onwire.value) != 0 ||
      usipy_sip_tm_uas_copy_hdrs(&tp->scratch, &tp->cache.uas.vias,
        &tp->cache.uas.nvias, viasp, nvias) != 0 ||
      usipy_sip_tm_uas_copy_hdrs(&tp->scratch, &tp->cache.uas.record_routes,
        &tp->cache.uas.nrecord_routes, rrsp, nrrs) != 0) {
        return (-1);
    }
    tp->cache.cseq = *cseqp->parsed.cseq;
    return (0);
}

struct usipy_sip_tm_uas_build_arg {
    const struct usipy_sip_tm_txi *tp;
    const struct usipy_sip_status *slp;
};

static int
usipy_sip_tm_uas_build_response_cb(void *arg, char *buf, size_t len)
{
    static const struct usipy_str sip20_sp = USIPY_2STR("SIP/2.0 ");
    static const struct usipy_str colon_sp = USIPY_2STR(": ");
    static const struct usipy_str tag_param = USIPY_2STR(";tag=");
    static const struct usipy_str server_value = USIPY_2STR("uSippy");
    static const struct usipy_str clen_value = USIPY_2STR("0");
    const struct usipy_sip_tm_uas_build_arg *barg = arg;
    const struct usipy_sip_tm_txi *tp = barg->tp;
    const struct usipy_sip_status *slp = barg->slp;
    const struct usipy_hdr_db_entr *via_hfp = usipy_hdr_db_byid(USIPY_HF_VIA);
    const struct usipy_hdr_db_entr *from_hfp = usipy_hdr_db_byid(USIPY_HF_FROM);
    const struct usipy_hdr_db_entr *to_hfp = usipy_hdr_db_byid(USIPY_HF_TO);
    const struct usipy_hdr_db_entr *callid_hfp = usipy_hdr_db_byid(USIPY_HF_CALLID);
    const struct usipy_hdr_db_entr *cseq_hfp = usipy_hdr_db_byid(USIPY_HF_CSEQ);
    const struct usipy_hdr_db_entr *rr_hfp = usipy_hdr_db_byid(USIPY_HF_RECORDROUTE);
    const struct usipy_hdr_db_entr *server_hfp = usipy_hdr_db_byid(USIPY_HF_SERVER);
    const struct usipy_hdr_db_entr *clen_hfp = usipy_hdr_db_byid(USIPY_HF_CONTENTLENGTH);
    size_t off = 0;
    char sbuf[4];
    int rval;

#define APPEND_MEM(bp, blen) do { \
        if (off + (blen) > len) return (-1); \
        memcpy(buf + off, (bp), (blen)); \
        off += (blen); \
    } while (0)
#define APPEND_STR(sp) APPEND_MEM((sp)->s.ro, (sp)->l)
    USIPY_DASSERT(via_hfp != NULL);
    USIPY_DASSERT(from_hfp != NULL);
    USIPY_DASSERT(to_hfp != NULL);
    USIPY_DASSERT(callid_hfp != NULL);
    USIPY_DASSERT(cseq_hfp != NULL);
    USIPY_DASSERT(rr_hfp != NULL);
    USIPY_DASSERT(server_hfp != NULL);
    USIPY_DASSERT(clen_hfp != NULL);
    APPEND_STR(&sip20_sp);
    usipy_sip_tm_uas_scode2str(slp->code, sbuf);
    APPEND_MEM(sbuf, sizeof(sbuf));
    APPEND_STR(&slp->reason_phrase);
    APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    for (size_t i = 0; i < tp->cache.uas.nvias; i++) {
        APPEND_STR(&via_hfp->name);
        APPEND_STR(&colon_sp);
        APPEND_STR(&tp->cache.uas.vias[i]);
        APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    }
    APPEND_STR(&from_hfp->name);
    APPEND_STR(&colon_sp);
    APPEND_STR(&tp->cache.uas.from);
    APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    APPEND_STR(&to_hfp->name);
    APPEND_STR(&colon_sp);
    APPEND_STR(&tp->cache.uas.to);
    if (tp->cache.to_tag.l != 0) {
        APPEND_STR(&tag_param);
        APPEND_STR(&tp->cache.to_tag);
    }
    APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    APPEND_STR(&callid_hfp->name);
    APPEND_STR(&colon_sp);
    APPEND_STR(&tp->cache.call_id);
    APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    APPEND_STR(&cseq_hfp->name);
    APPEND_STR(&colon_sp);
    rval = snprintf(buf + off, len - off, "%u %.*s", tp->cache.cseq.val,
      USIPY_SFMT(&usipy_method_db[tp->cache.method_type].name));
    if (rval < 0 || (size_t)rval >= len - off) {
        return (-1);
    }
    off += (size_t)rval;
    APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    for (size_t i = 0; i < tp->cache.uas.nrecord_routes; i++) {
        APPEND_STR(&rr_hfp->name);
        APPEND_STR(&colon_sp);
        APPEND_STR(&tp->cache.uas.record_routes[i]);
        APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    }
    APPEND_STR(&server_hfp->name);
    APPEND_STR(&colon_sp);
    APPEND_STR(&server_value);
    APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    APPEND_STR(&clen_hfp->name);
    APPEND_STR(&colon_sp);
    APPEND_STR(&clen_value);
    APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
    APPEND_MEM(USIPY_CRLF, USIPY_CRLF_LEN);
#undef APPEND_MEM
#undef APPEND_STR
    return ((int)off);
}

static int
usipy_sip_tm_uas_prepare_default_local_tag(struct usipy_sip_tm_txi *tp, size_t tx_index)
{
    USIPY_DASSERT(tp != NULL);

    return (usipy_msg_heap_sprintf(&tp->scratch, &tp->cache.to_tag, "t%zu-1", tx_index));
}

static int
usipy_sip_tm_uas_prepare_local_tag(const struct usipy_sip_tm *tm,
  struct usipy_sip_tm_txi *tp, size_t tx_index, uint32_t cseq, uint8_t method_type)
{
    struct usipy_sip_tm_id_policy_out ids = {0};
    struct usipy_sip_tm_id_policy_in in = {
      .transaction_index = tx_index,
      .cseq = cseq,
      .method_type = method_type,
    };

    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(tp != NULL);

    if (tm->id_policy.cb == NULL) {
        return (usipy_sip_tm_uas_prepare_default_local_tag(tp, tx_index));
    }
    if (tm->id_policy.cb(tm->id_policy.arg, &tp->scratch, &in, &ids) != 0 ||
      ids.local_tag.l == 0) {
        return (-1);
    }
    tp->cache.to_tag = ids.local_tag;
    return (0);
}

static int
usipy_sip_tm_find_uas_transaction(const struct usipy_sip_tm *tm,
  const struct usipy_sip_tid *tidp, size_t *indexp)
{
    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(tidp != NULL);
    USIPY_DASSERT(indexp != NULL);

    for (size_t i = 0; i < tm->max_transactions; i++) {
        const struct usipy_sip_tm_txi *tp = &tm->transactions[i];

        if (!tp->active || tp->pub.role != USIPY_SIP_TM_ROLE_UAS) {
            continue;
        }
        if (usipy_sip_tm_tid_matches_tx(tidp, &tp->pub)) {
            *indexp = i;
            return (0);
        }
    }
    *indexp = USIPY_SIP_TM_TX_INDEX_NONE;
    return (-1);
}

static int
usipy_sip_tm_find_uas_ack_transaction(const struct usipy_sip_tm *tm,
  const struct usipy_sip_tid *tidp, size_t *indexp)
{
    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(tidp != NULL);
    USIPY_DASSERT(indexp != NULL);

    for (size_t i = 0; i < tm->max_transactions; i++) {
        const struct usipy_sip_tm_txi *tp = &tm->transactions[i];

        if (!tp->active) {
            continue;
        }
        if (usipy_sip_tm_uas_ack_matches_tx(tidp, tp) &&
          usipy_sip_tm_uas_is_invite_non2xx_final(tp)) {
            *indexp = i;
            return (0);
        }
    }
    *indexp = USIPY_SIP_TM_TX_INDEX_NONE;
    return (-1);
}

int
usipy_sip_tm_new_uas_tr(struct usipy_sip_tm *tm,
  const struct usipy_sip_tm_new_uas_tr_params *tpp, size_t *indexp)
{
    struct usipy_sip_tid tid;
    struct usipy_sip_tm_txi *tp;
    struct usipy_sip_tm_timer_policy timers;
    uint8_t method_type;
    size_t tx_index;

    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(tpp != NULL);
    USIPY_DASSERT(indexp != NULL);
    USIPY_DASSERT(tpp->request != NULL);
    USIPY_DASSERT(tpp->request->kind == USIPY_SIP_MSG_REQ);

    *indexp = USIPY_SIP_TM_TX_INDEX_NONE;
    method_type = tpp->request->sline.parsed.rl.method->cantype;
    if (method_type == USIPY_SIP_METHOD_generic ||
      method_type == USIPY_SIP_METHOD_ACK ||
      method_type == USIPY_SIP_METHOD_CANCEL) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    if (usipy_sip_msg_get_tid((struct usipy_msg *)tpp->request, &tid) != 0) {
        return (USIPY_SIP_TM_ERR_BADMSG);
    }
    if (usipy_sip_tm_find_uas_transaction(tm, &tid, &tx_index) == 0) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    tp = usipy_sip_tm_alloc_slot(tm, &tx_index);
    if (tp == NULL) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    tp->cache.method_type = method_type;
    if (usipy_msg_heap_append(&tp->scratch, &tp->cache.call_id, tid.call_id) != 0 ||
      usipy_msg_heap_append(&tp->scratch, &tp->cache.branch, tid.vbranch) != 0 ||
      usipy_msg_heap_append(&tp->scratch, &tp->cache.from_tag, tid.from_tag) != 0 ||
      usipy_sip_tm_uas_cache_request(tpp->request, tp) != 0) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    if (usipy_sip_tm_uas_prepare_local_tag(tm, tp, tx_index, tid.cseq->val,
      method_type) != 0) {
        usipy_sip_tm_tx_fini(tp);
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    tp->outbound.checkpoint = usipy_msg_heap_checkpoint(&tp->scratch);
    tp->active = 1;
    tm->nactive += 1;
    tp->pub.role = USIPY_SIP_TM_ROLE_UAS;
    tp->pub.state = USIPY_SIP_TM_STATE_TRYING;
    tp->pub.common.flags = (tm->transport == USIPY_SIP_TM_TRANSPORT_UDP) ? 0 :
      USIPY_SIP_TM_F_RELIABLE_TRANSPORT;
    tp->pub.common.peer = tpp->peer;
    tp->pub.common.local = tpp->local;
    tp->outbound.pub.target = tpp->peer;
    tp->outbound.pub.raw = USIPY_STR_NULL;
    tp->outbound.pub.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
    tp->pub.common.outbound = tp->outbound.pub;
    timers = tpp->timers.t1_ms != 0 ? tpp->timers :
      (struct usipy_sip_tm_timer_policy)USIPY_SIP_TM_TIMER_POLICY_RFC3261;
    tp->pub.common.timers = timers;
    tp->pub.common.id.hash = tid.hash;
    tp->pub.common.id.branch = tp->cache.branch;
    tp->pub.common.id.call_id = tp->cache.call_id;
    tp->pub.common.id.from_tag = tp->cache.from_tag;
    tp->pub.common.id.cseq = tid.cseq->val;
    tp->pub.common.id.method_type = tp->cache.method_type;
    tp->pub.role_data.uas.last_status_code = 0;
    tp->pub.role_data.uas.request_retransmits = 0;
    *indexp = tx_index;
    return (USIPY_SIP_TM_OK);
}

int
usipy_sip_tm_send_uas_response(struct usipy_sip_tm *tm, size_t index,
  const struct usipy_sip_tm_uas_response_params *rpp)
{
    struct usipy_sip_tm_txi *tp;
    struct usipy_sip_tm_uas_build_arg barg;
    const struct usipy_sip_status *slp;

    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(rpp != NULL);
    USIPY_DASSERT(index < tm->max_transactions);
    slp = &rpp->status;
    tp = &tm->transactions[index];
    if (!tp->active || tp->pub.role != USIPY_SIP_TM_ROLE_UAS) {
        return (USIPY_SIP_TM_ERR_NOT_FOUND);
    }
    if (tp->pub.state == USIPY_SIP_TM_STATE_TERMINATED) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    if (tp->pub.role_data.uas.last_status_code >= 200) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    USIPY_DASSERT(tp->outbound.checkpoint != USIPY_MSG_HEAP_CHECKPOINT_NONE);
    usipy_msg_heap_rollback(&tp->scratch, tp->outbound.checkpoint);
    barg.tp = tp;
    barg.slp = slp;
    if (usipy_msg_heap_build(&tp->scratch, &tp->outbound.pub.raw, &barg,
      usipy_sip_tm_uas_build_response_cb) != 0) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    tp->outbound.pub.target = tp->pub.common.peer;
    tp->outbound.pub.next_send_at_ms = 0;
    tp->pub.common.outbound = tp->outbound.pub;
    tp->pub.role_data.uas.last_status_code = slp->code;
    tp->pub.state = (slp->code < 200) ? USIPY_SIP_TM_STATE_PROCEEDING :
      USIPY_SIP_TM_STATE_COMPLETED;
    tp->cache.uas.ack_hash = 0;
    if (slp->code < 200) {
        tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_NONE;
        memset(&tp->uas_callbacks, '\0', sizeof(tp->uas_callbacks));
    } else if (tp->cache.method_type == USIPY_SIP_METHOD_INVITE && slp->code >= 300) {
        tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_H;
        tp->pub.common.timer.value_ms = usipy_sip_tm_timer_h_ms(&tp->pub.common.timers);
        tp->pub.common.timer.due_at_ms = USIPY_SIP_TM_TIME_NONE;
        tp->cache.uas.ack_hash = usipy_sip_tm_uas_ack_hash(tp);
        tp->uas_callbacks = rpp->callbacks;
    } else {
        tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_NONE;
        memset(&tp->uas_callbacks, '\0', sizeof(tp->uas_callbacks));
    }
    return (USIPY_SIP_TM_OK);
}

int
usipy_sip_tm_uas_run(struct usipy_sip_tm_txi *tp, size_t index,
  const struct usipy_sip_tm *tm, const struct usipy_sip_tm_run_in *inp,
  struct usipy_sip_tm_run_out *outp)
{
    int send_rval;

    (void)tm;
    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(inp != NULL);

    if (tp->pub.common.timer.type != USIPY_SIP_TM_TIMER_NONE &&
      tp->pub.common.timer.due_at_ms <= inp->now_ms) {
        if (tp->pub.common.timer.type == USIPY_SIP_TM_TIMER_H) {
            if (tp->uas_callbacks.no_ack != NULL) {
                tp->uas_callbacks.no_ack(tp->uas_callbacks.arg, index, &tp->pub);
            }
            if (outp != NULL) {
                outp->ntimeouts += 1;
            }
        }
        usipy_sip_tm_uas_mark_terminated(tp);
    }
    if (tp->outbound.pub.next_send_at_ms != USIPY_SIP_TM_TIME_NONE) {
        usipy_sip_tm_uas_run_out_consider(outp, tp->outbound.pub.next_send_at_ms);
    }
    if (tp->pub.common.timer.type != USIPY_SIP_TM_TIMER_NONE) {
        usipy_sip_tm_uas_run_out_consider(outp, tp->pub.common.timer.due_at_ms);
    }
    if (tp->pub.state == USIPY_SIP_TM_STATE_TERMINATED ||
      tp->outbound.pub.next_send_at_ms == USIPY_SIP_TM_TIME_NONE ||
      tp->outbound.pub.next_send_at_ms > inp->now_ms) {
        return (USIPY_SIP_TM_OK);
    }
    send_rval = inp->send_to(inp->send_to_arg, index, &tp->pub, &tp->outbound.pub);
    if (send_rval != 0) {
        return (send_rval);
    }
    if (outp != NULL) {
        outp->nsent += 1;
    }
    if (tp->pub.common.created_at_ms == 0 && tp->pub.common.retransmit_count == 0) {
        tp->pub.common.created_at_ms = inp->now_ms;
    }
    tp->pub.common.updated_at_ms = inp->now_ms;
    tp->pub.common.retransmit_count += 1;
    tp->outbound.pub.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
    tp->pub.common.outbound = tp->outbound.pub;
    if (tp->pub.role_data.uas.last_status_code >= 200 &&
      usipy_sip_tm_uas_is_invite_non2xx_final(tp)) {
        usipy_sip_tm_uas_post_send_invite_final(tp, inp, outp);
    } else if (tp->pub.role_data.uas.last_status_code >= 200) {
        usipy_sip_tm_uas_post_send_final(tp, inp->now_ms, outp);
    }
    return (USIPY_SIP_TM_OK);
}

int
usipy_sip_tm_handle_incoming_request(const struct usipy_sip_tm_handle_incoming_in *inp,
  struct usipy_msg *msg, const struct usipy_sip_tid *tidp,
  struct usipy_sip_tm_handle_incoming_out *outp)
{
    struct usipy_sip_tm *tm;
    size_t tx_index;

    USIPY_DASSERT(inp != NULL);
    USIPY_DASSERT(inp->tm != NULL);
    USIPY_DASSERT(msg != NULL);
    USIPY_DASSERT(tidp != NULL);

    tm = inp->tm;
    if (msg->sline.parsed.rl.method->cantype == USIPY_SIP_METHOD_ACK &&
      usipy_sip_tm_find_uas_ack_transaction(tm, tidp, &tx_index) == 0) {
        struct usipy_sip_tm_txi *tp = &tm->transactions[tx_index];

        return (usipy_sip_tm_uas_handle_ack(inp, outp, tp, tx_index));
    }
    if (usipy_sip_tm_find_uas_transaction(tm, tidp, &tx_index) == 0) {
        struct usipy_sip_tm_txi *tp = &tm->transactions[tx_index];

        tp->pub.role_data.uas.request_retransmits += 1;
        if (tp->pub.state != USIPY_SIP_TM_STATE_CONFIRMED &&
          tp->outbound.pub.raw.l != 0) {
            tp->outbound.pub.next_send_at_ms = inp->now_ms;
            tp->pub.common.outbound = tp->outbound.pub;
        }
        if (outp != NULL) {
            outp->error = USIPY_SIP_TM_OK;
            outp->consumed = 1;
            outp->match_kind = USIPY_SIP_TM_MATCH_EXISTING;
            outp->event = USIPY_SIP_TM_EVENT_REQUEST_RETRANSMIT;
            outp->transaction_index = tx_index;
            outp->message = NULL;
        }
        return (USIPY_SIP_TM_OK);
    }
    if (tm->callbacks.incoming_request == NULL) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    tm->callbacks.incoming_request(tm->callbacks.arg, inp, msg);
    if (outp != NULL) {
        outp->error = USIPY_SIP_TM_OK;
        outp->consumed = 1;
        outp->match_kind = USIPY_SIP_TM_MATCH_NONE;
        outp->event = USIPY_SIP_TM_EVENT_REQUEST_RX;
        outp->transaction_index = USIPY_SIP_TM_TX_INDEX_NONE;
        outp->message = NULL;
    }
    if (usipy_sip_tm_find_uas_transaction(tm, tidp, &tx_index) == 0 && outp != NULL) {
        outp->match_kind = USIPY_SIP_TM_MATCH_NEW;
        outp->transaction_index = tx_index;
    }
    return (USIPY_SIP_TM_OK);
}
