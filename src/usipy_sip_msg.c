#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "usipy_port/log.h"
#include "usipy_port/byteorder.h"

#include "usipy_debug.h"
#include "usipy_types.h"
#include "usipy_str.h"
#include "usipy_msg_heap.h"
#include "usipy_msg_heap_rb.h"
#include "usipy_msg_heap_inl.h"
#include "usipy_sip_sline.h"
#include "usipy_sip_msg.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_method_db.h"
#include "usipy_sip_hdr_cseq.h"
#include "usipy_tvpair.h"
#include "usipy_sip_hdr_nameaddr.h"
#include "usipy_sip_hdr_via.h"
#include "usipy_sip_uri.h"
#include "usipy_sip_tid.h"

#define USIPY_HFS_NMIN (30)

struct usipy_sip_msg_iterator {
    struct usipy_str msg_onwire;
    struct usipy_str *msg_copy;
    int i;
    bool carry;
    uintptr_t oword[2];
    char cshift;
};
static int usipy_sip_msg_break_down(struct usipy_sip_msg_iterator *);

#define HT_SIZEOF(nhdrs) (sizeof(struct usipy_sip_hdr) * ((nhdrs) + 1))

struct usipy_msg *
usipy_sip_msg_ctor_fromwire(const char *buf, size_t len,
  struct usipy_msg_parse_err *perrp)
{
    struct usipy_msg *rp;
    size_t alloc_len, hf_prealloclen;
    uintptr_t ralgn;
    struct usipy_sip_msg_iterator mit;

    hf_prealloclen = USIPY_ALIGNED_SIZE(len < (sizeof(struct usipy_sip_hdr) * USIPY_HFS_NMIN) ?
      sizeof(struct usipy_sip_hdr) * USIPY_HFS_NMIN : len);
    alloc_len = sizeof(struct usipy_msg) + USIPY_ALIGNED_SIZE(len) +
      hf_prealloclen;
    rp = malloc(alloc_len);
    if (rp == NULL) {
        goto e0;
    }
    void *heapstart = rp->_storage + len;
    size_t heapsize = alloc_len - offsetof(typeof(*rp), _storage) - len;
    memset(rp, '\0', sizeof(struct usipy_msg));
    memset(heapstart, '\0', heapsize);
    /*memcpy(rp->_storage, buf, len);*/
    rp->onwire.s.rw = rp->_storage;
    rp->onwire.l = len;

    usipy_msg_heap_init(&rp->heap, heapstart, heapsize);

    struct usipy_sip_hdr *shp = NULL, *ehp;
    ehp = (struct usipy_sip_hdr *)((char *)(rp) + alloc_len);
    memset(&mit, '\0', sizeof(mit));
    mit.msg_onwire = (struct usipy_str){.s.ro = buf, .l = len};
    mit.msg_copy = &rp->onwire;
    struct usipy_str cp;
    struct usipy_msg_heap_cnt cnt;
    memset(&cnt, '\0', sizeof(cnt));
    for (cp = rp->onwire; cp.l > 0;) {
        int crlf_off = usipy_sip_msg_break_down(&mit);
        if (crlf_off < 0)
            break;
        const char *chp = rp->onwire.s.ro + crlf_off;
        if (shp == NULL) {
            if (rp->sline.onwire.l == 0) {
                rp->sline.onwire.s.ro = cp.s.ro;
                rp->sline.onwire.l = crlf_off;
                rp->kind = usipy_sip_sline_parse(&rp->heap, &rp->sline);
                if (rp->kind == USIPY_SIP_MSG_UNKN)
                    goto e1;
                goto next_line;
            }
        } else {
            int nextra;

            if (USIPY_ISWS(cp.s.ro[0])) {
                /* Continuation */
                goto multi_line;
            }
            nextra = usipy_sip_hdr_preparse(shp, ehp);
            if (nextra < 0) {
                goto e1;
            }
            rp->hdr_masks.present |= USIPY_HF_MASK(shp);
            rp->nhdrs += 1 + nextra;
            if (nextra > 0) {
                if (usipy_msg_heap_aextend(&rp->heap, HT_SIZEOF(rp->nhdrs),
                  &cnt) != 0)
                    goto e1;
            }
        }
        if (chp == cp.s.ro) {
            cp.l -= 2;
            cp.s.ro += 2;
            /* End of headers reached */
            break;
        }
        if (shp == NULL) {
            rp->hdrs = usipy_msg_heap_alloc_cnt(&rp->heap, HT_SIZEOF(rp->nhdrs), &cnt);
            if (rp->hdrs == NULL)
                goto e1;
        } else {
            if (usipy_msg_heap_aextend(&rp->heap, HT_SIZEOF(rp->nhdrs),
              &cnt) != 0)
                goto e1;
        }
        shp = &rp->hdrs[rp->nhdrs];
        shp->onwire.full.s.ro = cp.s.ro;
multi_line:
        shp->onwire.full.l = chp - shp->onwire.full.s.ro;
next_line:
        chp += 2;
        cp.l -= chp - cp.s.ro;
        cp.s.ro = chp;
    }
    if (rp->nhdrs == 0) {
        goto e1;
    }
    if (cp.l > 0) {
        rp->body = cp;
        if (mit.i < len) {
            memcpy(rp->onwire.s.rw + mit.i, buf + mit.i, len - mit.i);
        }
    }
    return (rp);
e1:
    free(rp);
e0:
    if (perrp != NULL)
        perrp->erRNo = ENOMEM;
    return (NULL);
}

void
usipy_sip_msg_dtor(struct usipy_msg *msg)
{

    free(msg);
}

void
usipy_sip_msg_dump(const struct usipy_msg *msg, const char *log_tag)
{

    USIPY_LOGI(log_tag, "start line = \"%.*s\"", USIPY_SFMT(&msg->sline.onwire));

    switch (msg->kind) {
    case USIPY_SIP_MSG_RES:
        USIPY_LOGI(log_tag, "Message[%p] is SIP RESPONSE: status_code = %u, "
          "reason_phrase = \"%.*s\"", msg,
          msg->sline.parsed.sl.status.code, USIPY_SFMT(&msg->sline.parsed.sl.status.reason_phrase));
        break;

    case USIPY_SIP_MSG_REQ:
        USIPY_LOGI(log_tag, "Message[%p] is SIP REQUEST: method(onwire) = \"%.*s\", "
          "method(canonic) = \"%.*s\", ruri = \"%.*s\"", msg,
          USIPY_SFMT(&msg->sline.parsed.rl.onwire.method),
          USIPY_SFMT(&msg->sline.parsed.rl.mtype->name),
          USIPY_SFMT(&msg->sline.parsed.rl.onwire.ruri));
        if (msg->sline.parsed.rl.ruri != NULL) {
            usipy_sip_uri_dump(msg->sline.parsed.rl.ruri, log_tag,
              "  .parsed.rl.ruri->");
        }
        break;

    default:
        abort();
    }

    for (int i = 0; i < msg->nhdrs; i++) {
        const struct usipy_sip_hdr *shp = &msg->hdrs[i];
        USIPY_LOGI(log_tag, "header[%d @ %p], .hf_type = %p, .onwire.hf_type = %p", i,
          shp, shp->hf_type, shp->onwire.hf_type);
        USIPY_LOGI(log_tag, "  .onwire.type = %u", shp->onwire.hf_type->cantype);
	USIPY_LOGI(log_tag, "  .name = \"%.*s\"", USIPY_SFMT(&shp->onwire.name));
	USIPY_LOGI(log_tag, "  .value = \"%.*s\"", USIPY_SFMT(&shp->onwire.value));
        if (shp->parsed.generic != NULL && shp->hf_type->dump != NULL) {
            shp->hf_type->dump(&shp->parsed, log_tag, "  .parsed->",
              shp->hf_type->parsed_memb_name);
        }
    }
    USIPY_LOGI(log_tag, "body[%lu] = \"%.*s\"", (unsigned long)msg->body.l,
      USIPY_SFMT(&msg->body));
    USIPY_LOGI(log_tag, "heap remaining %lu",
      (unsigned long)usipy_msg_heap_remaining(&msg->heap));
}

int
usipy_sip_msg_parse_hdrs(struct usipy_msg *mp, uint64_t parsemask, int toponly)
{
    uint64_t topparsed = 0;

    if ((mp->hdr_masks.present & parsemask) != parsemask)
        return(-1);
    parsemask &= ~(mp->hdr_masks.parsed);
    for (int i = 0; i < mp->nhdrs; i++) {
        struct usipy_sip_hdr *shp = &mp->hdrs[i];
        if (!USIPY_HF_ISMSET(parsemask, shp->hf_type->cantype)) {
            if (toponly && USIPY_HF_ISMSET(topparsed, shp->hf_type->cantype)) {
                topparsed &= ~USIPY_HFT_MASK(shp->hf_type->cantype);
            }
            continue;
        }
        if (shp->hf_type->parse == NULL)
            return (-1);
        shp->parsed = shp->hf_type->parse(&mp->heap, &shp->onwire.value);
        if (shp->parsed.generic == NULL)
            return (-1);
        if (toponly) {
            parsemask &= ~USIPY_HFT_MASK(shp->hf_type->cantype);
            topparsed |= USIPY_HFT_MASK(shp->hf_type->cantype);
        }
    }
    if (toponly) {
       mp->hdr_masks.parsed |= topparsed;
    } else {
       mp->hdr_masks.parsed |= parsemask;
    }
    return (0);
}

int
usipy_sip_msg_get_tid(struct usipy_msg *mp, struct usipy_sip_tid *tp)
{
    uint64_t seenmask = ~USIPY_HF_TID_MASK;

    if (usipy_sip_msg_parse_hdrs(mp, USIPY_HF_TID_MASK, 1) != 0)
        return (-1);
    for (int i = 0; i < mp->nhdrs; i++) {
        struct usipy_sip_hdr *shp = &mp->hdrs[i];
        int j;

        if (USIPY_HF_ISMSET(seenmask, shp->hf_type->cantype))
            continue;
        USIPY_DASSERT(shp->parsed.generic != NULL);
        switch (shp->hf_type->cantype) {
        case USIPY_HF_CSEQ:
            tp->cseq = shp->parsed.cseq;
            break;
        case USIPY_HF_CALLID:
            tp->call_id = shp->parsed.generic;
            break;
        case USIPY_HF_VIA:
            for (j = 0; j < shp->parsed.via->nparams; j++) {
                const struct usipy_tvpair *pp = &shp->parsed.via->params[j];
                if (pp->token.l != 6 && memcmp(pp->token.s.ro, "branch", 6) != 0)
                   continue;
                if (pp->value.l == 0)
                   return (-1);
                tp->vbranch = &pp->value;
                break;
            }
            if (j == shp->parsed.via->nparams)
                return (-1);
            break;
        case USIPY_HF_FROM:
            for (j = 0; j < shp->parsed.from->nparams; j++) {
                const struct usipy_tvpair *pp = &shp->parsed.from->params[j];
                if (pp->token.l != 3 && memcmp(pp->token.s.ro, "tag", 3) != 0)
                   continue;
                if (pp->value.l == 0)
                   return (-1);
                tp->from_tag = &pp->value;
                break;
            }
            if (j == shp->parsed.from->nparams)
                return (-1);
            break;
        default:
            USIPY_DABORT();
            break;
        }
        seenmask |= USIPY_HFT_MASK(shp->hf_type->cantype);
    }
    if ((~seenmask) != 0)
        return (-1);
    tp->hash = usipy_sip_tid_hash(tp);
    return (0);
}

#if USIPY_BIGENDIAN
#  define LE32TOH(dp, sp) { \
    *(uint32_t *)(dp) = (uint32_t) \
     ((const char *)(sp))[0] | \
     ((const char *)(sp))[1] << 8 | \
     ((const char *)(sp))[2] << 16 | \
     ((const char *)(sp))[3] << 24; \
  }
#else
#  define LE32TOH(dp, sp) /* Nop */
#endif

struct crlfres {
    uint8_t v;
    bool carry;
};

#define BFILL(btype, c) (((~(btype)0) / 255) * (c))

static inline struct crlfres
crlfcompr(uintptr_t cval, bool carry)
{
    const uintptr_t mskA = BFILL(typeof(mskA), '\r'); /* '\r' * sizeof(mskA) */
    const uintptr_t mskB = BFILL(typeof(mskB), '\n'); /* '\n' * sizeof(mskB) */
    uintptr_t val, mvalA, mvalB;
    struct crlfres rval = {.v = 0};

    mvalA = cval ^ mskA; /* This produces 0x00 at positions with \r */
    mvalB = cval ^ mskB; /* This produces 0x00 at positions with \n */
    rval.carry = ((mvalA >> (sizeof(mvalA) - 1) * 8) == 0);

    mvalA = (((mvalA << 8) | (!carry)) | mvalB);
    mvalA = (((mvalA) - BFILL(typeof(mvalA), 0x01)) & ~(mvalA) & BFILL(typeof(mvalA), 0x80));
    /*
     * The outer for() loop is just our way to hint compiler as to how many iterations
     * we have, so it can unroll.
     */
    for (int i = 0; i < (sizeof(mvalA) / 2); i++) {
        int nbit = ffsl(mvalA);
        if (nbit == 0)
            break;
        mvalA ^= ((typeof(mskA))1 << (nbit - 1));
        rval.v |= ((typeof(rval.v))1 << ((nbit / 8) - 1));
    }

    return (rval);
}

/*
 * Input string: "foo\r\nbar\r\nfoo\r\nbar\r\nfoo\r\nbar\r\nfoo\r\nbar\r\n"
 * Output offsets: 3 8 13 18 23 28 33 38 -1
 */

static int
usipy_sip_msg_break_down(struct usipy_sip_msg_iterator *mip)
{
    typeof(mip->oword[0]) val;

    if (mip->cshift == 0 && mip->oword[0] != 0) {
        char boff;
gotresult:
        boff = ffsl(mip->oword[0]) - 1;
        mip->oword[0] ^= ((typeof(val))1 << boff);
        return (mip->i - (sizeof(val) * 8) + boff - 1);
    }

    struct crlfres ms = {.carry = mip->carry};
    for (; mip->i < mip->msg_onwire.l; mip->i += sizeof(val)) {
        int remain = mip->msg_onwire.l - mip->i;
        if (remain < sizeof(val)) {
            val = 0;
            memcpy(&val, mip->msg_onwire.s.ro + mip->i, remain);
            memcpy(mip->msg_copy->s.rw + mip->i, &val, remain);
        } else {
            memcpy(&val, mip->msg_onwire.s.ro + mip->i, sizeof(val));
            memcpy(mip->msg_copy->s.rw + mip->i, &val, sizeof(val));
        }
        LE32TOH(&val, &val);
        ms = crlfcompr(val, ms.carry);
        if (ms.v != 0) {
            mip->oword[0] |= (typeof(val))ms.v << mip->cshift;
        }
        if (mip->cshift == (sizeof(val) * 7)) {
            mip->cshift = 0;
            if (mip->oword[0] != 0) {
                mip->i += sizeof(val);
                mip->carry = ms.carry;
                goto gotresult;
            }
        } else {
            mip->cshift += sizeof(val);
        }
    }
    if (mip->cshift != 0 && mip->oword[0] != 0) {
        mip->i += (sizeof(val) * 8) - mip->cshift;
	mip->cshift = 0;
        goto gotresult;
    }

    return (-1);
}
