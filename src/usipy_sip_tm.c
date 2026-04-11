#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "usipy_port/network.h"

#include "usipy_debug.h"
#include "public/usipy_msg_heap.h"
#include "public/usipy_str.h"
#include "public/usipy_sip_method_types.h"
#include "public/usipy_sip_tm.h"
#include "public/usipy_sip_hdr_types.h"
#include "public/usipy_sip_sline.h"
#include "public/usipy_sip_msg.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_hdr_cseq.h"
#include "usipy_sip_hdr_auth.h"
#include "usipy_sip_hdr_authz.h"
#include "usipy_sip_method_db.h"
#include "usipy_sip_tid.h"
#include "usipy_tvpair.h"
#include "usipy_sip_hdr_nameaddr.h"
#include "usipy_sip_hdr_via.h"
#include "usipy_sip_uri.h"

#define USIPY_SIP_TM_HEAP_SIZE 512u
#define USIPY_SIP_TM_TX_SCRATCH_SIZE 1024u
#define USIPY_SIP_TM_TX_NCHECKPOINTS 2u

struct usipy_sip_tm_txcache {
    struct usipy_str method_name;
    struct usipy_sip_uri *request_uri;
    struct usipy_str call_id;
    struct usipy_str branch;
    struct usipy_str from_tag;
    struct usipy_str from_uri;
    struct usipy_str to_uri;
    struct usipy_str contact_uri;
    uint32_t contact_expires;
    struct usipy_sip_hdr_cseq cseq;
};

struct usipy_sip_tm_txi {
    struct usipy_sip_tm_tx pub;
    struct usipy_sip_tm_txcache cache;
    struct usipy_sip_tm_uac_callbacks callbacks;
    struct {
        size_t checkpoint;
        struct usipy_sip_tm_outbound pub;
    } outbound;
    struct usipy_msg_heap scratch;
    size_t scratch_checkpoints[USIPY_SIP_TM_TX_NCHECKPOINTS];
    void *scratch_buf;
    size_t scratch_capacity;
    int active;
};

struct usipy_sip_tm_default_via {
    struct usipy_sip_hdr_via via;
    struct usipy_tvpair params[2];
};

struct usipy_sip_tm_default_nameaddr {
    struct usipy_sip_hdr_nameaddr nameaddr;
    struct usipy_tvpair params[1];
};

struct usipy_sip_tm {
    int sock;
    enum usipy_sip_tm_transport transport;
    size_t max_transactions;
    size_t nactive;
    struct usipy_msg_heap heap;
    void *heap_buf;
    struct usipy_sip_tm_addr laddr;
    struct usipy_str luri;
    struct usipy_sip_tm_default_via default_via;
    struct usipy_sip_tm_default_nameaddr default_from;
    struct usipy_sip_tm_default_nameaddr default_to;
    struct usipy_sip_tm_default_nameaddr default_contact;
    struct usipy_sip_tm_id_policy id_policy;
    struct usipy_sip_tm_txi *transactions;
};

static size_t
usipy_sip_tm_ctor_size(size_t max_transactions)
{
    size_t txs_size, scratch_size, total_size;

    USIPY_DASSERT(max_transactions > 0);
    USIPY_DASSERT(max_transactions <= SIZE_MAX / sizeof(struct usipy_sip_tm_txi));
    txs_size = sizeof(struct usipy_sip_tm_txi) * max_transactions;
    USIPY_DASSERT(max_transactions <= SIZE_MAX / USIPY_SIP_TM_TX_SCRATCH_SIZE);
    scratch_size = USIPY_SIP_TM_TX_SCRATCH_SIZE * max_transactions;
    USIPY_DASSERT(sizeof(struct usipy_sip_tm) <= SIZE_MAX - txs_size);
    total_size = sizeof(struct usipy_sip_tm) + txs_size;
    USIPY_DASSERT(total_size <= SIZE_MAX - USIPY_SIP_TM_HEAP_SIZE);
    total_size += USIPY_SIP_TM_HEAP_SIZE;
    USIPY_DASSERT(total_size <= SIZE_MAX - scratch_size);
    total_size += scratch_size;
    return (total_size);
}

static struct usipy_str
usipy_sip_tm_transport_name(enum usipy_sip_tm_transport transport)
{
    switch (transport) {
    case USIPY_SIP_TM_TRANSPORT_UDP:
        return ((struct usipy_str)USIPY_2STR("UDP"));

    case USIPY_SIP_TM_TRANSPORT_TCP:
        return ((struct usipy_str)USIPY_2STR("TCP"));

    case USIPY_SIP_TM_TRANSPORT_TLS:
        return ((struct usipy_str)USIPY_2STR("TLS"));

    case USIPY_SIP_TM_TRANSPORT_SCTP:
        return ((struct usipy_str)USIPY_2STR("SCTP"));

    default:
        USIPY_DABORT();
    }
}

static struct usipy_sip_tm_txi *
usipy_sip_tm_alloc_slot(struct usipy_sip_tm *tm, size_t *indexp)
{
    for (size_t i = 0; i < tm->max_transactions; i++) {
        if (tm->transactions[i].active) {
            continue;
        }
        if (indexp != NULL) {
            *indexp = i;
        }
        return (&tm->transactions[i]);
    }
    if (indexp != NULL) {
        *indexp = USIPY_SIP_TM_TX_INDEX_NONE;
    }
    return (NULL);
}

static int
usipy_sip_tm_transition_allowed(const struct usipy_sip_tm_txi *tp,
  enum usipy_sip_tm_state next_state)
{
    USIPY_DASSERT(tp != NULL);

    if (tp->pub.role != USIPY_SIP_TM_ROLE_UAC ||
      next_state != USIPY_SIP_TM_STATE_CALLING) {
        return (0);
    }
    return (tp->pub.state == USIPY_SIP_TM_STATE_TERMINATED ||
      tp->pub.state == USIPY_SIP_TM_STATE_COMPLETED);
}

static int
usipy_sip_tm_transition(struct usipy_sip_tm_txi *tp,
  enum usipy_sip_tm_state next_state)
{
    USIPY_DASSERT(tp != NULL);

    if (!usipy_sip_tm_transition_allowed(tp, next_state)) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    tp->pub.state = next_state;
    tp->pub.common.flags &= ~USIPY_SIP_TM_F_TERMINATED;
    return (USIPY_SIP_TM_OK);
}

static int
usipy_sip_tm_alloc_msgbuf(struct usipy_sip_tm_txi *tp, size_t capacity)
{
    tp->pub.common.msgbuf.items = calloc(capacity, sizeof(tp->pub.common.msgbuf.items[0]));
    if (tp->pub.common.msgbuf.items == NULL) {
        return (-1);
    }
    tp->pub.common.msgbuf.capacity = capacity;
    tp->pub.common.msgbuf.request_index = USIPY_SIP_TM_MSG_INDEX_NONE;
    tp->pub.common.msgbuf.response_index = USIPY_SIP_TM_MSG_INDEX_NONE;
    return (0);
}

static int
usipy_sip_tm_store_msg(struct usipy_sip_tm_txi *tp, struct usipy_msg *msg,
  size_t *indexp)
{
    USIPY_DASSERT(tp->pub.common.msgbuf.items != NULL);
    if (tp->pub.common.msgbuf.nitems >= tp->pub.common.msgbuf.capacity) {
        return (-1);
    }
    const size_t index = tp->pub.common.msgbuf.nitems++;
    tp->pub.common.msgbuf.items[index] = msg;
    if (indexp != NULL) {
        *indexp = index;
    }
    return (0);
}

static void
usipy_sip_tm_tx_clear_msgbuf(struct usipy_sip_tm_txi *tp)
{
    USIPY_DASSERT(tp != NULL);

    if (tp->pub.common.msgbuf.items != NULL) {
        for (size_t i = 0; i < tp->pub.common.msgbuf.nitems; i++) {
            if (tp->pub.common.msgbuf.items[i] != NULL) {
                usipy_sip_msg_dtor(tp->pub.common.msgbuf.items[i]);
                tp->pub.common.msgbuf.items[i] = NULL;
            }
        }
    }
    tp->pub.common.msgbuf.nitems = 0;
    tp->pub.common.msgbuf.request_index = USIPY_SIP_TM_MSG_INDEX_NONE;
    tp->pub.common.msgbuf.response_index = USIPY_SIP_TM_MSG_INDEX_NONE;
}

static int
usipy_sip_tm_format_local_uri(struct usipy_sip_tm_txi *tp, const struct usipy_sip_tm *tm,
  const struct usipy_str *userp, int include_port, struct usipy_str *urip)
{
    if (userp->l == 0) {
        switch (tm->laddr.af) {
        case AF_INET:
            if (include_port) {
                return (usipy_msg_heap_sprintf(&tp->scratch, urip, "sip:%.*s:%u",
                  USIPY_SFMT(&tm->laddr.host), tm->laddr.port));
            }
            return (usipy_msg_heap_sprintf(&tp->scratch, urip, "sip:%.*s",
              USIPY_SFMT(&tm->laddr.host)));
#ifdef IPPROTO_IPV6
        case AF_INET6:
            if (include_port) {
                return (usipy_msg_heap_sprintf(&tp->scratch, urip, "sip:[%.*s]:%u",
                  USIPY_SFMT(&tm->laddr.host), tm->laddr.port));
            }
            return (usipy_msg_heap_sprintf(&tp->scratch, urip, "sip:[%.*s]",
              USIPY_SFMT(&tm->laddr.host)));
#endif
        default:
            errno = EAFNOSUPPORT;
            return (-1);
        }
    }
    switch (tm->laddr.af) {
    case AF_INET:
        if (include_port) {
            return (usipy_msg_heap_sprintf(&tp->scratch, urip, "sip:%.*s@%.*s:%u",
              USIPY_SFMT(userp), USIPY_SFMT(&tm->laddr.host), tm->laddr.port));
        }
        return (usipy_msg_heap_sprintf(&tp->scratch, urip, "sip:%.*s@%.*s",
          USIPY_SFMT(userp), USIPY_SFMT(&tm->laddr.host)));
#ifdef IPPROTO_IPV6
    case AF_INET6:
        if (include_port) {
            return (usipy_msg_heap_sprintf(&tp->scratch, urip, "sip:%.*s@[%.*s]:%u",
              USIPY_SFMT(userp), USIPY_SFMT(&tm->laddr.host), tm->laddr.port));
        }
        return (usipy_msg_heap_sprintf(&tp->scratch, urip, "sip:%.*s@[%.*s]",
          USIPY_SFMT(userp), USIPY_SFMT(&tm->laddr.host)));
#endif
    default:
        errno = EAFNOSUPPORT;
        return (-1);
    }
}

static int
usipy_sip_tm_copy_uri(struct usipy_sip_tm_txi *tp, const struct usipy_str *srcp,
  struct usipy_str *dstp)
{
    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(srcp != NULL);
    USIPY_DASSERT(dstp != NULL);

    if (srcp->l == 0) {
        *dstp = USIPY_STR_NULL;
        return (0);
    }
    return (usipy_msg_heap_sprintf(&tp->scratch, dstp, "%.*s", USIPY_SFMT(srcp)));
}

static int
usipy_sip_tm_prepare_default_ids(struct usipy_sip_tm_txi *tp, size_t tx_index,
  struct usipy_sip_tm_id_policy_out *outp)
{
    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(outp != NULL);

    if (usipy_msg_heap_sprintf(&tp->scratch, &outp->branch, "z9hG4bK-%u-%u",
      (unsigned int)tx_index, tp->cache.cseq.val) != 0) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    if (usipy_msg_heap_sprintf(&tp->scratch, &outp->local_tag, "t%u-%u",
      (unsigned int)tx_index, tp->cache.cseq.val) != 0) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    return (USIPY_SIP_TM_OK);
}

static int
usipy_sip_tm_prepare_ids(const struct usipy_sip_tm *tm, struct usipy_sip_tm_txi *tp,
  size_t tx_index)
{
    struct usipy_sip_tm_id_policy_out ids = {0};
    struct usipy_sip_tm_id_policy_in in = {
      .transaction_index = tx_index,
      .cseq = tp->cache.cseq.val,
      .method_type = tp->pub.common.id.method_type,
    };
    struct usipy_sip_tid tid;

    USIPY_DASSERT(tp != NULL);
    USIPY_DASSERT(tm != NULL);

    if (tm->id_policy.cb != NULL) {
        if (tm->id_policy.cb(tm->id_policy.arg, &tp->scratch, &in, &ids) != 0) {
            return (USIPY_SIP_TM_ERR_NOSPC);
        }
    } else if (usipy_sip_tm_prepare_default_ids(tp, tx_index, &ids) != USIPY_SIP_TM_OK) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    if (ids.branch.l == 0 || ids.local_tag.l == 0) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    tp->cache.branch = ids.branch;
    tp->cache.from_tag = ids.local_tag;
    tid.call_id = &tp->cache.call_id;
    tid.from_tag = &tp->cache.from_tag;
    tid.vbranch = &tp->cache.branch;
    tid.cseq = &tp->cache.cseq;
    tid.hash = usipy_sip_tid_hash(&tid);
    tp->pub.common.id.hash = tid.hash;
    tp->pub.common.id.branch = *tid.vbranch;
    tp->pub.common.id.call_id = *tid.call_id;
    tp->pub.common.id.from_tag = *tid.from_tag;
    tp->pub.common.id.method_name = tp->cache.method_name;
    tp->pub.common.id.cseq = tp->cache.cseq.val;
    return (USIPY_SIP_TM_OK);
}

static void
usipy_sip_tm_tx_reset_uac_runtime(struct usipy_sip_tm_txi *tp)
{
    USIPY_DASSERT(tp != NULL);

    usipy_sip_tm_tx_clear_msgbuf(tp);
    tp->pub.common.flags &= ~USIPY_SIP_TM_F_TERMINATED;
    tp->pub.common.id.hash = 0;
    tp->pub.common.id.branch = USIPY_STR_NULL;
    tp->pub.common.id.from_tag = USIPY_STR_NULL;
    tp->pub.common.id.cseq = tp->cache.cseq.val;
    tp->pub.common.retransmit_count = 0;
    tp->pub.common.created_at_ms = 0;
    tp->pub.common.updated_at_ms = 0;
    tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_NONE;
    tp->pub.common.timer.value_ms = 0;
    tp->pub.common.timer.due_at_ms = 0;
    tp->pub.role_data.uac.last_status_code = 0;
    tp->pub.role_data.uac.response_class = 0;
    tp->outbound.pub.raw = USIPY_STR_NULL;
    tp->outbound.pub.next_send_at_ms = 0;
    tp->pub.common.outbound = tp->outbound.pub;
}

static void
usipy_sip_tm_release_outbound(struct usipy_sip_tm_txi *tp)
{
    USIPY_DASSERT(tp != NULL);

    if (tp->outbound.checkpoint != USIPY_MSG_HEAP_CHECKPOINT_NONE) {
        usipy_msg_heap_rollback(&tp->scratch, tp->outbound.checkpoint);
    }
    tp->pub.common.id.hash = 0;
    tp->pub.common.id.branch = USIPY_STR_NULL;
    tp->pub.common.id.from_tag = USIPY_STR_NULL;
    tp->outbound.pub.raw = USIPY_STR_NULL;
    tp->outbound.pub.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
    tp->pub.common.outbound = tp->outbound.pub;
}

static int
usipy_sip_tm_str_eq(const struct usipy_str *a, const struct usipy_str *b)
{
    USIPY_DASSERT(a != NULL);
    USIPY_DASSERT(b != NULL);

    return (a->l == b->l && (a->l == 0 || memcmp(a->s.ro, b->s.ro, a->l) == 0));
}

static int
usipy_sip_tm_tid_matches_tx(const struct usipy_sip_tid *tidp,
  const struct usipy_sip_tm_tx *txp)
{
    USIPY_DASSERT(tidp != NULL);
    USIPY_DASSERT(txp != NULL);
    USIPY_DASSERT(tidp->call_id != NULL);
    USIPY_DASSERT(tidp->from_tag != NULL);
    USIPY_DASSERT(tidp->vbranch != NULL);
    USIPY_DASSERT(tidp->cseq != NULL);

    if (!usipy_sip_tm_str_eq(tidp->call_id, &txp->common.id.call_id) ||
      !usipy_sip_tm_str_eq(tidp->from_tag, &txp->common.id.from_tag) ||
      !usipy_sip_tm_str_eq(tidp->vbranch, &txp->common.id.branch)) {
        return (0);
    }
    if (tidp->cseq->val != txp->common.id.cseq ||
      tidp->cseq->method->cantype != txp->common.id.method_type) {
        return (0);
    }
    return (1);
}

struct usipy_sip_tm_build_uri_arg {
    const struct usipy_sip_uri *urip;
};

static int
usipy_sip_tm_build_uri_cb(void *arg, char *buf, size_t len)
{
    const struct usipy_sip_tm_build_uri_arg *uarg = arg;

    USIPY_DASSERT(uarg != NULL);
    USIPY_DASSERT(uarg->urip != NULL);
    return (usipy_sip_uri_build(uarg->urip, buf, len));
}

struct usipy_sip_tm_build_authz_arg {
    const struct usipy_sip_hdr_authz *authzp;
};

static int
usipy_sip_tm_build_authz_cb(void *arg, char *buf, size_t len)
{
    const struct usipy_sip_tm_build_authz_arg *aarg = arg;
    union usipy_sip_hdr_parsed up;

    USIPY_DASSERT(aarg != NULL);
    USIPY_DASSERT(aarg->authzp != NULL);
    up.authz = (struct usipy_sip_hdr_authz *)aarg->authzp;
    return (usipy_sip_hdr_authz_build(&up, buf, len));
}

static int
usipy_sip_tm_handle_incoming_response(const struct usipy_sip_tm_handle_incoming_in *inp,
  struct usipy_msg *msg, const struct usipy_sip_tid *tidp,
  struct usipy_sip_tm_handle_incoming_out *outp)
{
    struct usipy_sip_tm *tm = inp->tm;
    const unsigned int scode = msg->sline.parsed.sl.status.code;
    const uint8_t sclass = (uint8_t)(scode / 100u);

    for (size_t i = 0; i < tm->max_transactions; i++) {
        struct usipy_sip_tm_txi *tp = &tm->transactions[i];

        if (!tp->active || tp->pub.role != USIPY_SIP_TM_ROLE_UAC) {
            continue;
        }
        if (!usipy_sip_tm_tid_matches_tx(tidp, &tp->pub)) {
            continue;
        }
        tp->pub.common.updated_at_ms = inp->now_ms;
        tp->pub.common.outbound.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
        tp->pub.common.outbound.raw = USIPY_STR_NULL;
        tp->outbound.pub = tp->pub.common.outbound;
        tp->pub.role_data.uac.last_status_code = (uint16_t)scode;
        tp->pub.role_data.uac.response_class = sclass;
        if (sclass >= 2) {
            tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_NONE;
            tp->pub.common.timer.value_ms = 0;
            tp->pub.common.timer.due_at_ms = 0;
            tp->pub.state = USIPY_SIP_TM_STATE_COMPLETED;
        }
        if (tp->callbacks.response != NULL) {
            tp->callbacks.response(tp->callbacks.arg, i, &tp->pub, msg);
        }
        if (outp != NULL) {
            outp->error = USIPY_SIP_TM_OK;
            outp->consumed = 1;
            outp->match_kind = USIPY_SIP_TM_MATCH_EXISTING;
            outp->event = sclass < 2 ? USIPY_SIP_TM_EVENT_RESPONSE_1XX :
              USIPY_SIP_TM_EVENT_RESPONSE_FINAL;
            outp->transaction_index = i;
            outp->message_index = USIPY_SIP_TM_MSG_INDEX_NONE;
            outp->message = NULL;
        }
        if (sclass >= 2 && tp->pub.state != USIPY_SIP_TM_STATE_CALLING) {
            usipy_sip_tm_release_outbound(tp);
        }
        usipy_sip_msg_dtor(msg);
        return (USIPY_SIP_TM_OK);
    }
    return (USIPY_SIP_TM_ERR_NOT_FOUND);
}

static int
usipy_sip_tm_build_request(struct usipy_sip_tm_txi *tp, size_t tx_index,
  const struct usipy_sip_tm *tm, const struct usipy_sip_tm_extra_header *ehp, size_t neh)
{
    struct usipy_msg tmsg = {0};
    struct usipy_sip_hdr thdrs[7 + neh];
    struct usipy_hdr_db_entr ehdb[neh];
    struct usipy_sip_tm_default_via via;
    struct usipy_sip_tm_default_nameaddr from;
    struct usipy_sip_tm_default_nameaddr to;
    struct usipy_sip_tm_default_nameaddr contact;
    const struct usipy_hdr_db_entr *hfp;
    struct usipy_str clen = USIPY_2STR("0");
    struct usipy_str expires = USIPY_STR_NULL;
    struct usipy_str rawmsg;
    size_t hindex;
    int rval;

    USIPY_DASSERT(tp->outbound.checkpoint != USIPY_MSG_HEAP_CHECKPOINT_NONE);
    usipy_msg_heap_rollback(&tp->scratch, tp->outbound.checkpoint);
    rval = usipy_sip_tm_prepare_ids(tm, tp, tx_index);
    if (rval != USIPY_SIP_TM_OK) {
        return (rval);
    }
    memset(thdrs, '\0', sizeof(thdrs));
    via = tm->default_via;
    via.params[0].value = tp->cache.branch;
    from = tm->default_from;
    from.nameaddr.addr_spec = tp->cache.from_uri;
    from.params[0].value = tp->cache.from_tag;
    to = tm->default_to;
    to.nameaddr.addr_spec = tp->cache.to_uri;
    contact = tm->default_contact;
    contact.nameaddr.addr_spec = tp->cache.contact_uri;
    if (tp->cache.contact_expires != 0) {
        if (usipy_msg_heap_sprintf(&tp->scratch, &expires, "%u",
          tp->cache.contact_expires) != 0) {
            return (USIPY_SIP_TM_ERR_NOSPC);
        }
        contact.nameaddr.nparams = 1;
        contact.params[0].token = (struct usipy_str)USIPY_2STR("expires");
        contact.params[0].value = expires;
    }

    tmsg.kind = USIPY_SIP_MSG_REQ;
    tmsg.sline.parsed.rl.method = tp->cache.cseq.method;
    tmsg.sline.parsed.rl.ruri = tp->cache.request_uri;
    tmsg.sline.parsed.rl.version = (struct usipy_str)USIPY_2STR("SIP/2.0");
    tmsg.hdrs = thdrs;
    tmsg.nhdrs = 7 + neh;

    hindex = 0;
    hfp = usipy_hdr_db_byid(USIPY_HF_VIA);
    thdrs[hindex].hf_type = hfp;
    thdrs[hindex].parsed.via = &via.via;
    hindex += 1;

    hfp = usipy_hdr_db_byid(USIPY_HF_FROM);
    thdrs[hindex].hf_type = hfp;
    thdrs[hindex].parsed.from = &from.nameaddr;
    hindex += 1;

    hfp = usipy_hdr_db_byid(USIPY_HF_TO);
    thdrs[hindex].hf_type = hfp;
    thdrs[hindex].parsed.to = &to.nameaddr;
    hindex += 1;

    hfp = usipy_hdr_db_byid(USIPY_HF_CONTACT);
    thdrs[hindex].hf_type = hfp;
    thdrs[hindex].parsed.contact = &contact.nameaddr;
    hindex += 1;

    hfp = usipy_hdr_db_byid(USIPY_HF_CALLID);
    thdrs[hindex].hf_type = hfp;
    thdrs[hindex].parsed.generic = &tp->cache.call_id;
    hindex += 1;

    hfp = usipy_hdr_db_byid(USIPY_HF_CSEQ);
    thdrs[hindex].hf_type = hfp;
    thdrs[hindex].parsed.cseq = &tp->cache.cseq;
    hindex += 1;

    for (size_t i = 0; i < neh; i++) {
        hfp = usipy_hdr_db_byid(ehp[i].hf_type);
        if (hfp == NULL || hfp->cantype != ehp[i].hf_type) {
            return (USIPY_SIP_TM_ERR_INVAL);
        }
        thdrs[hindex].hf_type = hfp;
        if (ehp[i].value_kind == USIPY_SIP_TM_EH_PARSED) {
            switch (ehp[i].hf_type) {
            case USIPY_HF_AUTHORIZATION:
            case USIPY_HF_PROXYAUTHORIZATION:
                thdrs[hindex].parsed.authz =
                  (struct usipy_sip_hdr_authz *)ehp[i].parsed;
                break;

            default:
                return (USIPY_SIP_TM_ERR_INVAL);
            }
            hindex += 1;
            continue;
        }
        if (ehp[i].value_kind != USIPY_SIP_TM_EH_RAW) {
            return (USIPY_SIP_TM_ERR_INVAL);
        }
        if (hfp->build != NULL) {
            ehdb[i] = *hfp;
            ehdb[i].build = NULL;
            hfp = &ehdb[i];
            thdrs[hindex].hf_type = hfp;
        }
        thdrs[hindex].parsed.generic = &ehp[i].value;
        hindex += 1;
    }

    hfp = usipy_hdr_db_byid(USIPY_HF_CONTENTLENGTH);
    thdrs[hindex].hf_type = hfp;
    thdrs[hindex].parsed.generic = &clen;

    if (usipy_sip_msg_build(&tp->scratch, &tmsg, &rawmsg) != 0) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    tp->outbound.pub.raw = rawmsg;
    tp->pub.common.outbound = tp->outbound.pub;
    USIPY_DCODE({
        struct usipy_msg_parse_err perr = USIPY_MSG_PARSE_ERR_init;
        struct usipy_msg *msgp;

        msgp = usipy_sip_msg_ctor_fromwire(rawmsg.s.ro, rawmsg.l, &perr);
        USIPY_DASSERT(msgp != NULL);
        if (msgp != NULL) {
            usipy_sip_msg_dtor(msgp);
        }
    });
    return (USIPY_SIP_TM_OK);
}

static uint32_t
usipy_sip_tm_timer_f_ms(const struct usipy_sip_tm_timer_policy *tp)
{
    uint64_t fms;

    if (tp->timer_f_ms != 0) {
        return (tp->timer_f_ms);
    }
    fms = (uint64_t)tp->t1_ms * 64u;
    USIPY_DASSERT(fms <= UINT32_MAX);
    return ((uint32_t)fms);
}

static uint32_t
usipy_sip_tm_uac_next_send_delay_ms(const struct usipy_sip_tm_tx *txp)
{
    uint64_t delay = txp->common.timers.timer_e_ms != 0 ? txp->common.timers.timer_e_ms :
      txp->common.timers.t1_ms;

    for (uint8_t i = 1; i < txp->common.retransmit_count; i++) {
        delay <<= 1;
        if (delay >= txp->common.timers.t2_ms) {
            return (txp->common.timers.t2_ms);
        }
    }
    USIPY_DASSERT(delay <= UINT32_MAX);
    return ((uint32_t)delay);
}

static void
usipy_sip_tm_run_out_init(struct usipy_sip_tm_run_out *outp)
{
    if (outp == NULL) {
        return;
    }
    memset(outp, '\0', sizeof(*outp));
    outp->next_run_at_ms = USIPY_SIP_TM_TIME_NONE;
}

static void
usipy_sip_tm_run_out_consider(struct usipy_sip_tm_run_out *outp, uint64_t when_ms)
{
    if (outp == NULL || when_ms == USIPY_SIP_TM_TIME_NONE) {
        return;
    }
    if (outp->next_run_at_ms == USIPY_SIP_TM_TIME_NONE || when_ms < outp->next_run_at_ms) {
        outp->next_run_at_ms = when_ms;
    }
}

static void
usipy_sip_tm_run_out_consider_timer(struct usipy_sip_tm_run_out *outp,
  const struct usipy_sip_tm_timer *tp)
{
    if (tp->type == USIPY_SIP_TM_TIMER_NONE) {
        return;
    }
    usipy_sip_tm_run_out_consider(outp, tp->due_at_ms);
}

static int
usipy_sip_tm_uac_run(struct usipy_sip_tm_txi *tp, size_t index, const struct usipy_sip_tm *tm,
  const struct usipy_sip_tm_run_in *inp, struct usipy_sip_tm_run_out *outp)
{
    uint32_t delay_ms;
    int rval;

    (void)tm;
    if (tp->pub.state == USIPY_SIP_TM_STATE_TERMINATED ||
      (tp->pub.common.flags & USIPY_SIP_TM_F_TERMINATED) != 0) {
        return (USIPY_SIP_TM_OK);
    }
    if (tp->pub.common.timer.type != USIPY_SIP_TM_TIMER_NONE &&
      tp->pub.common.timer.due_at_ms <= inp->now_ms) {
        tp->pub.state = USIPY_SIP_TM_STATE_TERMINATED;
        tp->pub.common.flags |= USIPY_SIP_TM_F_TERMINATED;
        tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_NONE;
        tp->outbound.pub.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
        tp->pub.common.outbound = tp->outbound.pub;
        if (tp->callbacks.timeout != NULL) {
            tp->callbacks.timeout(tp->callbacks.arg, index, &tp->pub);
        }
        if (tp->pub.state != USIPY_SIP_TM_STATE_CALLING) {
            usipy_sip_tm_release_outbound(tp);
        }
        if (outp != NULL) {
            outp->ntimeouts += 1;
        }
        return (USIPY_SIP_TM_OK);
    }
    if (tp->outbound.pub.next_send_at_ms == USIPY_SIP_TM_TIME_NONE ||
      tp->outbound.pub.next_send_at_ms > inp->now_ms) {
        usipy_sip_tm_run_out_consider(outp, tp->outbound.pub.next_send_at_ms);
        usipy_sip_tm_run_out_consider_timer(outp, &tp->pub.common.timer);
        return (USIPY_SIP_TM_OK);
    }
    USIPY_DASSERT(inp->send_to != NULL);
    if (tp->outbound.pub.raw.l == 0) {
        rval = usipy_sip_tm_build_request(tp, index, tm, NULL, 0);
        if (rval != 0) {
            return (rval);
        }
    }
    rval = inp->send_to(inp->send_to_arg, index, &tp->pub, &tp->outbound.pub);
    if (rval != 0) {
        return (rval);
    }
    if (tp->pub.common.timer.type == USIPY_SIP_TM_TIMER_NONE) {
        tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_F;
        tp->pub.common.timer.value_ms = usipy_sip_tm_timer_f_ms(&tp->pub.common.timers);
        tp->pub.common.timer.due_at_ms = inp->now_ms + tp->pub.common.timer.value_ms;
    }
    if (tp->pub.common.created_at_ms == 0 && tp->pub.common.retransmit_count == 0) {
        tp->pub.common.created_at_ms = inp->now_ms;
    }
    tp->pub.common.updated_at_ms = inp->now_ms;
    tp->pub.common.retransmit_count += 1;
    if ((tp->pub.common.flags & USIPY_SIP_TM_F_RELIABLE_TRANSPORT) != 0) {
        tp->outbound.pub.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
    } else {
        delay_ms = usipy_sip_tm_uac_next_send_delay_ms(&tp->pub);
        tp->outbound.pub.next_send_at_ms = inp->now_ms + delay_ms;
    }
    tp->pub.common.outbound = tp->outbound.pub;
    if (outp != NULL) {
        outp->nsent += 1;
    }
    usipy_sip_tm_run_out_consider(outp, tp->outbound.pub.next_send_at_ms);
    usipy_sip_tm_run_out_consider_timer(outp, &tp->pub.common.timer);
    return (USIPY_SIP_TM_OK);
}

static int
usipy_sip_tm_init_default_via(struct usipy_sip_tm *tm)
{
    tm->default_via.via.sent_protocol.name = (struct usipy_str)USIPY_2STR("SIP");
    tm->default_via.via.sent_protocol.version = (struct usipy_str)USIPY_2STR("2.0");
    tm->default_via.via.sent_protocol.transport = usipy_sip_tm_transport_name(tm->transport);
    tm->default_via.via.sent_by.host = tm->laddr.host;
    tm->default_via.via.sent_by.port = tm->laddr.port;
    tm->default_via.via.nparams = 2;
    tm->default_via.params[0].token = (struct usipy_str)USIPY_2STR("branch");
    tm->default_via.params[0].value = USIPY_STR_NULL;
    tm->default_via.params[1].token = (struct usipy_str)USIPY_2STR("rport");
    tm->default_via.params[1].value = USIPY_STR_NULL;
    return (0);
}

static int
usipy_sip_tm_init_default_nameaddrs(struct usipy_sip_tm *tm)
{
    tm->default_from.nameaddr.addr_spec = tm->luri;
    tm->default_from.nameaddr.nparams = 1;
    tm->default_from.params[0].token = (struct usipy_str)USIPY_2STR("tag");
    tm->default_from.params[0].value = USIPY_STR_NULL;

    tm->default_to.nameaddr.addr_spec = tm->luri;
    tm->default_to.nameaddr.nparams = 0;
    tm->default_to.params[0].token = (struct usipy_str)USIPY_2STR("tag");
    tm->default_to.params[0].value = USIPY_STR_NULL;

    tm->default_contact.nameaddr.addr_spec = tm->luri;
    tm->default_contact.nameaddr.nparams = 0;
    tm->default_contact.params[0].token = (struct usipy_str)USIPY_2STR("expires");
    tm->default_contact.params[0].value = USIPY_STR_NULL;
    return (0);
}

static int
usipy_sip_tm_init_luri(struct usipy_sip_tm *tm)
{
    switch (tm->laddr.af) {
    case AF_INET:
        return (usipy_msg_heap_sprintf(&tm->heap, &tm->luri, "sip:%.*s:%u",
          USIPY_SFMT(&tm->laddr.host), tm->laddr.port));

#ifdef IPPROTO_IPV6
    case AF_INET6:
        return (usipy_msg_heap_sprintf(&tm->heap, &tm->luri, "sip:[%.*s]:%u",
          USIPY_SFMT(&tm->laddr.host), tm->laddr.port));
#endif

    default:
        errno = EAFNOSUPPORT;
        return (-1);
    }
}

static int
usipy_sip_tm_init_laddr(struct usipy_sip_tm *tm)
{
    char abuf[INET6_ADDRSTRLEN];
    const char *aname;
    socklen_t alen;
    union {
        struct sockaddr sa;
        struct sockaddr_in sin;
#ifdef IPPROTO_IPV6
        struct sockaddr_in6 sin6;
#endif
    } name;

    memset(&name, '\0', sizeof(name));
    alen = sizeof(name);
    if (getsockname(tm->sock, &name.sa, &alen) != 0) {
        return (-1);
    }
    tm->laddr.transport = tm->transport;
    switch (name.sa.sa_family) {
    case AF_INET:
        aname = inet_ntop(AF_INET, &name.sin.sin_addr, abuf, sizeof(abuf));
        if (aname == NULL) {
            return (-1);
        }
        tm->laddr.af = AF_INET;
        tm->laddr.port = ntohs(name.sin.sin_port);
        if (usipy_msg_heap_sprintf(&tm->heap, &tm->laddr.host, "%s", aname) != 0) {
            return (-1);
        }
        break;

#ifdef IPPROTO_IPV6
    case AF_INET6:
        aname = inet_ntop(AF_INET6, &name.sin6.sin6_addr, abuf, sizeof(abuf));
        if (aname == NULL) {
            return (-1);
        }
        tm->laddr.af = AF_INET6;
        tm->laddr.port = ntohs(name.sin6.sin6_port);
        if (usipy_msg_heap_sprintf(&tm->heap, &tm->laddr.host, "%s", aname) != 0) {
            return (-1);
        }
        break;
#endif

    default:
        errno = EAFNOSUPPORT;
        return (-1);
    }
    if (usipy_sip_tm_init_luri(tm) != 0) {
        return (-1);
    }
    if (usipy_sip_tm_init_default_via(tm) != 0) {
        return (-1);
    }
    return (usipy_sip_tm_init_default_nameaddrs(tm));
}

static void
usipy_sip_tm_tx_reset(struct usipy_sip_tm_txi *tp)
{
    memset(&tp->pub, '\0', sizeof(tp->pub));
    memset(&tp->cache, '\0', sizeof(tp->cache));
    tp->pub.common.msgbuf.request_index = USIPY_SIP_TM_MSG_INDEX_NONE;
    tp->pub.common.msgbuf.response_index = USIPY_SIP_TM_MSG_INDEX_NONE;
    tp->pub.common.timer.type = USIPY_SIP_TM_TIMER_NONE;
    tp->pub.common.timer.value_ms = 0;
    tp->pub.common.timer.due_at_ms = 0;
    memset(&tp->callbacks, '\0', sizeof(tp->callbacks));
    memset(&tp->outbound, '\0', sizeof(tp->outbound));
    tp->outbound.checkpoint = USIPY_MSG_HEAP_CHECKPOINT_NONE;
    tp->outbound.pub.raw = USIPY_STR_NULL;
    tp->outbound.pub.next_send_at_ms = USIPY_SIP_TM_TIME_NONE;
    tp->pub.common.outbound = tp->outbound.pub;
    usipy_msg_heap_init(&tp->scratch, tp->scratch_buf, tp->scratch_capacity,
      tp->scratch_checkpoints, USIPY_SIP_TM_TX_NCHECKPOINTS);
    tp->active = 0;
}

static void
usipy_sip_tm_tx_fini(struct usipy_sip_tm_txi *tp)
{
    if (tp->pub.common.msgbuf.items != NULL) {
        for (size_t i = 0; i < tp->pub.common.msgbuf.nitems; i++) {
            if (tp->pub.common.msgbuf.items[i] != NULL) {
                usipy_sip_msg_dtor(tp->pub.common.msgbuf.items[i]);
            }
        }
        free(tp->pub.common.msgbuf.items);
    }
    usipy_sip_tm_tx_reset(tp);
}

struct usipy_sip_tm *
usipy_sip_tm_ctor(const struct usipy_sip_tm_ctor_params *ipp)
{
    struct usipy_sip_tm *rp;
    size_t total_size;

    if (ipp == NULL) {
        return (NULL);
    }
    USIPY_DASSERT(ipp != NULL);
    total_size = usipy_sip_tm_ctor_size(ipp->max_transactions);
    USIPY_DASSERT(ipp->max_transactions > 0);
    USIPY_DASSERT(ipp->transport != USIPY_SIP_TM_TRANSPORT_UNSPEC);
    rp = malloc(total_size);
    if (rp == NULL) {
        return (NULL);
    }
    memset(rp, '\0', total_size);
    rp->sock = ipp->sock;
    rp->transport = ipp->transport;
    rp->max_transactions = ipp->max_transactions;
    rp->transactions = (struct usipy_sip_tm_txi *)(rp + 1);
    rp->heap_buf = rp->transactions + ipp->max_transactions;
    rp->id_policy = ipp->id_policy;
    usipy_msg_heap_init(&rp->heap, rp->heap_buf, USIPY_SIP_TM_HEAP_SIZE, NULL, 0);
    if (usipy_sip_tm_init_laddr(rp) != 0) {
        free(rp);
        return (NULL);
    }
    unsigned char *scratch = (unsigned char *)rp->heap_buf + USIPY_SIP_TM_HEAP_SIZE;
    for (size_t i = 0; i < ipp->max_transactions; i++) {
        struct usipy_sip_tm_txi *tp = &rp->transactions[i];

        tp->scratch_buf = scratch + (i * USIPY_SIP_TM_TX_SCRATCH_SIZE);
        tp->scratch_capacity = USIPY_SIP_TM_TX_SCRATCH_SIZE;
        usipy_sip_tm_tx_reset(tp);
    }
    return (rp);
}

void
usipy_sip_tm_dtor(struct usipy_sip_tm *tm)
{
    USIPY_DASSERT(tm != NULL);
    if (tm->transactions != NULL) {
        for (size_t i = 0; i < tm->max_transactions; i++) {
            usipy_sip_tm_tx_fini(&tm->transactions[i]);
        }
    }
    free(tm);
}

size_t
usipy_sip_tm_nactive(const struct usipy_sip_tm *tm)
{
    if (tm == NULL) {
        return (0);
    }
    return (tm->nactive);
}

const struct usipy_sip_tm_tx *
usipy_sip_tm_get_transaction(const struct usipy_sip_tm *tm, size_t index)
{
    if (tm == NULL || index >= tm->max_transactions) {
        return (NULL);
    }
    if (!tm->transactions[index].active) {
        return (NULL);
    }
    return (&tm->transactions[index].pub);
}

int
usipy_sip_tm_drop_transaction(struct usipy_sip_tm *tm, size_t index)
{
    if (tm == NULL || index >= tm->max_transactions) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    if (!tm->transactions[index].active) {
        return (USIPY_SIP_TM_ERR_NOT_FOUND);
    }
    usipy_sip_tm_tx_fini(&tm->transactions[index]);
    if (tm->nactive > 0) {
        tm->nactive -= 1;
    }
    return (USIPY_SIP_TM_OK);
}

int
usipy_sip_tm_set_timer_policy(struct usipy_sip_tm *tm, size_t index,
  const struct usipy_sip_tm_timer_policy *policy)
{
    if (tm == NULL || policy == NULL || index >= tm->max_transactions) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    if (!tm->transactions[index].active) {
        return (USIPY_SIP_TM_ERR_NOT_FOUND);
    }
    tm->transactions[index].pub.common.timers = *policy;
    return (USIPY_SIP_TM_OK);
}

int
usipy_sip_tm_new_transaction(struct usipy_sip_tm *tm,
  const struct usipy_sip_tm_new_transaction_params *tpp, size_t *indexp)
{
    struct usipy_sip_tm_txi *tp;
    const struct usipy_method_db_entr *mdp;
    size_t tx_index;

    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(tpp != NULL);
    USIPY_DASSERT(indexp != NULL);

    *indexp = USIPY_SIP_TM_TX_INDEX_NONE;
    if (tpp->method_type > USIPY_SIP_METHOD_max) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    mdp = &usipy_method_db[tpp->method_type];
    if (mdp->cantype != tpp->method_type || mdp->name.l == 0 ||
      tpp->call_id.l == 0 || tpp->request_uri.l == 0) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    tp = usipy_sip_tm_alloc_slot(tm, &tx_index);
    if (tp == NULL) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    usipy_sip_tm_tx_reset(tp);
    if (usipy_msg_heap_sprintf(&tp->scratch, &tp->cache.call_id, "%.*s",
      USIPY_SFMT(&tpp->call_id)) != 0) {
        goto nospc;
    }
    tp->cache.request_uri = usipy_sip_uri_parse(&tp->scratch, &tpp->request_uri);
    if (tp->cache.request_uri == NULL) {
        goto nospc;
    }
    if ((tpp->from_uri.l != 0 ?
      usipy_sip_tm_copy_uri(tp, &tpp->from_uri, &tp->cache.from_uri) :
      usipy_sip_tm_format_local_uri(tp, tm, &tpp->from_username, 0,
        &tp->cache.from_uri)) != 0 ||
      (tpp->to_uri.l != 0 ?
      usipy_sip_tm_copy_uri(tp, &tpp->to_uri, &tp->cache.to_uri) :
      usipy_sip_tm_format_local_uri(tp, tm, &tpp->to_username, 0,
        &tp->cache.to_uri)) != 0 ||
      (tpp->contact_uri.l != 0 ?
      usipy_sip_tm_copy_uri(tp, &tpp->contact_uri, &tp->cache.contact_uri) :
      usipy_sip_tm_format_local_uri(tp, tm, &tpp->contact_username, 1,
        &tp->cache.contact_uri)) != 0) {
        goto nospc;
    }
    tp->cache.method_name = mdp->name;
    tp->cache.contact_expires = tpp->contact_expires;
    tp->cache.cseq.val = tpp->cseq;
    tp->cache.cseq.method = mdp;
    tp->callbacks = tpp->callbacks;
    tp->outbound.checkpoint = usipy_msg_heap_checkpoint(&tp->scratch);
    tp->active = 1;
    tm->nactive += 1;
    tp->pub.role = USIPY_SIP_TM_ROLE_UAC;
    tp->pub.state = USIPY_SIP_TM_STATE_TRYING;
    tp->pub.common.flags = (tm->transport == USIPY_SIP_TM_TRANSPORT_UDP) ? 0 :
      USIPY_SIP_TM_F_RELIABLE_TRANSPORT;
    tp->pub.common.msgbuf.request_index = USIPY_SIP_TM_MSG_INDEX_NONE;
    tp->pub.common.peer = tpp->target;
    tp->pub.common.local = tm->laddr;
    tp->outbound.pub.target = tpp->target;
    tp->outbound.pub.raw = USIPY_STR_NULL;
    tp->outbound.pub.next_send_at_ms = 0;
    tp->pub.common.outbound = tp->outbound.pub;
    tp->pub.common.timers = USIPY_SIP_TM_TIMER_POLICY_RFC3261;
    tp->pub.common.id.hash = 0;
    tp->pub.common.id.branch = USIPY_STR_NULL;
    tp->pub.common.id.call_id = tp->cache.call_id;
    tp->pub.common.id.from_tag = USIPY_STR_NULL;
    tp->pub.common.id.method_name = mdp->name;
    tp->pub.common.id.cseq = tpp->cseq;
    tp->pub.common.id.method_type = tpp->method_type;
    tp->pub.role_data.uac.last_status_code = 0;
    tp->pub.role_data.uac.response_class = 0;
    *indexp = tx_index;
    return (USIPY_SIP_TM_OK);

nospc:
    usipy_sip_tm_tx_fini(tp);
    return (USIPY_SIP_TM_ERR_NOSPC);
}

int
usipy_sip_tm_gen_authz_hf(const struct usipy_sip_tm *tm, size_t index, uint8_t hf_type,
  struct usipy_msg_heap *mhp, const struct usipy_sip_hdr_auth *challengep,
  const struct usipy_str *usernamep, const struct usipy_str *passwordp,
  const struct usipy_str *bodyp, const struct usipy_str *qopp,
  struct usipy_sip_tm_extra_header *ehp)
{
    const struct usipy_sip_tm_txi *tp;
    struct usipy_sip_tm_build_uri_arg uarg;
    struct usipy_sip_hdr_authz *authzp;
    struct usipy_str uris;

    if (tm == NULL || mhp == NULL || challengep == NULL || usernamep == NULL ||
      passwordp == NULL || ehp == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    if (hf_type != USIPY_HF_AUTHORIZATION &&
      hf_type != USIPY_HF_PROXYAUTHORIZATION) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    if (index >= tm->max_transactions) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    tp = &tm->transactions[index];
    if (!tp->active || tp->pub.role != USIPY_SIP_TM_ROLE_UAC) {
        return (USIPY_SIP_TM_ERR_NOT_FOUND);
    }
    uarg.urip = tp->cache.request_uri;
    if (usipy_msg_heap_build(mhp, &uris, &uarg, usipy_sip_tm_build_uri_cb) != 0) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    authzp = gen_auth_hf(mhp, challengep, usernamep, passwordp,
      &tp->cache.method_name, &uris, bodyp, qopp);
    if (authzp == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    ehp->hf_type = hf_type;
    ehp->value_kind = USIPY_SIP_TM_EH_PARSED;
    ehp->value = USIPY_STR_NULL;
    ehp->parsed = authzp;
    return (USIPY_SIP_TM_OK);
}

int
usipy_sip_tm_next_transaction(struct usipy_sip_tm *tm, size_t index,
  const struct usipy_sip_tm_extra_header *ehp, size_t neh)
{
    struct usipy_sip_tm_txi *tp;
    int rval;

    USIPY_DASSERT(tm != NULL);
    if (neh != 0 && ehp == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    if (index >= tm->max_transactions) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    tp = &tm->transactions[index];
    if (!tp->active) {
        return (USIPY_SIP_TM_ERR_NOT_FOUND);
    }
    rval = usipy_sip_tm_transition(tp, USIPY_SIP_TM_STATE_CALLING);
    if (rval != USIPY_SIP_TM_OK) {
        return (rval);
    }
    tp->cache.cseq.val += 1;
    tp->pub.common.id.cseq = tp->cache.cseq.val;
    usipy_sip_tm_tx_reset_uac_runtime(tp);
    return (usipy_sip_tm_build_request(tp, index, tm, ehp, neh));
}

int
usipy_sip_tm_run(struct usipy_sip_tm_run_in *inp, struct usipy_sip_tm_run_out *outp)
{
    struct usipy_sip_tm *tm;
    int rval;

    USIPY_DASSERT(inp != NULL);
    USIPY_DASSERT(inp->tm != NULL);
    USIPY_DASSERT(inp->send_to != NULL);
    usipy_sip_tm_run_out_init(outp);
    tm = inp->tm;
    if (inp->send_to == NULL) {
        if (outp != NULL) {
            outp->error = USIPY_SIP_TM_ERR_INVAL;
        }
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    for (size_t i = 0; i < tm->max_transactions; i++) {
        struct usipy_sip_tm_txi *tp = &tm->transactions[i];

        if (!tp->active) {
            continue;
        }
        switch (tp->pub.role) {
        case USIPY_SIP_TM_ROLE_UAC:
            rval = usipy_sip_tm_uac_run(tp, i, tm, inp, outp);
            if (rval != 0) {
                if (outp != NULL) {
                    outp->error = rval;
                }
                return (rval);
            }
            break;
        case USIPY_SIP_TM_ROLE_UAS:
            break;
        default:
            USIPY_DABORT();
        }
    }
    if (outp != NULL) {
        outp->error = USIPY_SIP_TM_OK;
    }
    return (USIPY_SIP_TM_OK);
}

const struct usipy_msg *
usipy_sip_tm_tx_get_msg(const struct usipy_sip_tm_tx *txp, size_t index)
{
    if (txp == NULL || index >= txp->common.msgbuf.nitems ||
      txp->common.msgbuf.items == NULL) {
        return (NULL);
    }
    return (txp->common.msgbuf.items[index]);
}

const struct usipy_msg *
usipy_sip_tm_tx_request(const struct usipy_sip_tm_tx *txp)
{
    if (txp == NULL || txp->common.msgbuf.request_index == USIPY_SIP_TM_MSG_INDEX_NONE) {
        return (NULL);
    }
    return (usipy_sip_tm_tx_get_msg(txp, txp->common.msgbuf.request_index));
}

const struct usipy_msg *
usipy_sip_tm_tx_response(const struct usipy_sip_tm_tx *txp)
{
    if (txp == NULL || txp->common.msgbuf.response_index == USIPY_SIP_TM_MSG_INDEX_NONE) {
        return (NULL);
    }
    return (usipy_sip_tm_tx_get_msg(txp, txp->common.msgbuf.response_index));
}

int
usipy_sip_tm_handle_incoming(const struct usipy_sip_tm_handle_incoming_in *inp,
  struct usipy_sip_tm_handle_incoming_out *outp)
{
    struct usipy_msg_parse_err perr = USIPY_MSG_PARSE_ERR_init;
    struct usipy_sip_tid tid;
    struct usipy_msg *msg;
    int rval;

    USIPY_DASSERT(inp != NULL);
    USIPY_DASSERT(inp->tm != NULL);
    USIPY_DASSERT(inp->buf != NULL);
    if (outp != NULL) {
        memset(outp, '\0', sizeof(*outp));
        outp->error = USIPY_SIP_TM_ERR_UNSUPPORTED;
        outp->transaction_index = USIPY_SIP_TM_TX_INDEX_NONE;
        outp->message_index = USIPY_SIP_TM_MSG_INDEX_NONE;
    }
    msg = usipy_sip_msg_ctor_fromwire(inp->buf, inp->len, &perr);
    if (msg == NULL) {
        if (outp != NULL) {
            outp->error = USIPY_SIP_TM_ERR_PARSE;
        }
        return (USIPY_SIP_TM_ERR_PARSE);
    }
    if (msg->kind != USIPY_SIP_MSG_RES) {
        usipy_sip_msg_dtor(msg);
        if (outp != NULL) {
            outp->error = USIPY_SIP_TM_ERR_UNSUPPORTED;
        }
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    if (usipy_sip_msg_get_tid(msg, &tid) != 0) {
        usipy_sip_msg_dtor(msg);
        if (outp != NULL) {
            outp->error = USIPY_SIP_TM_ERR_BADMSG;
        }
        return (USIPY_SIP_TM_ERR_BADMSG);
    }
    rval = usipy_sip_tm_handle_incoming_response(inp, msg, &tid, outp);
    if (rval != USIPY_SIP_TM_OK) {
        usipy_sip_msg_dtor(msg);
        if (outp != NULL) {
            outp->error = rval;
        }
        return (rval);
    }
    return (USIPY_SIP_TM_OK);
}
