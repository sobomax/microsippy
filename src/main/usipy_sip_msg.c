#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "usipy_str.h"
#include "usipy_msg_heap.h"
#include "usipy_sip_sline.h"
#include "usipy_sip_msg.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_method_db.h"
#include "usipy_sip_hdr_cseq.h"

#define USIPY_HFS_NMIN (12)

struct usipy_sip_msg_iterator {
    struct usipy_str msg_onwire;
    struct usipy_str *msg_copy;
    int i;
    int over;
    uint32_t oword;
    char cshift;
#if 0
    uint32_t imask;
    const char **ioffst;
#endif
};
static int usipy_sip_msg_break_down(struct usipy_sip_msg_iterator *);

#define USIPY_MSG_PARSE_ERR_init { \
  .erRNo = 0, .loc.fname = NULL, .loc.linen = 0, .loc.funcn = NULL \
}

#define MAKE_IMASK(ch) (((ch) << 24) | ((ch) << 16) | ((ch) << 8) | ch)

struct usipy_msg *
usipy_sip_msg_ctor_fromwire(const char *buf, size_t len,
  struct usipy_msg_parse_err *perrp)
{
    struct usipy_msg *rp;
    size_t alloc_len, hf_prealloclen;
    uintptr_t ralgn;
    const char *allocend;
    struct usipy_sip_msg_iterator mit;

    hf_prealloclen = USIPY_ALIGNED_SIZE(len < (sizeof(struct usipy_sip_hdr) * USIPY_HFS_NMIN) ?
      sizeof(struct usipy_sip_hdr) * USIPY_HFS_NMIN : len);
    alloc_len = sizeof(struct usipy_msg) + USIPY_ALIGNED_SIZE(len) +
      hf_prealloclen;
    rp = malloc(alloc_len);
    if (rp == NULL) {
        goto e0;
    }
    allocend = ((const char *)rp) + alloc_len;
    memset(rp, '\0', sizeof(struct usipy_msg));
    memset(rp->_storage + len, '\0', allocend - rp->_storage - len);
    /*memcpy(rp->_storage, buf, len);*/
    rp->onwire.s.rw = rp->_storage;
    rp->onwire.l = len;
    rp->heap.first = (void *)(rp->_storage + USIPY_ALIGNED_SIZE(len));
    //usipy_sip_msg_break_down(&rp->onwire, rp->_crlf_map);
    rp->hdrs = (struct usipy_sip_hdr *)(rp->heap.first);
    struct usipy_sip_hdr *shp = NULL;
    memset(&mit, '\0', sizeof(mit));
    mit.msg_onwire = (struct usipy_str){.s.ro = buf, .l = len};
    mit.msg_copy = &rp->onwire;
#if 0
    mit.imask = MAKE_IMASK(':');
#endif
    struct usipy_str cp;
    for (cp = rp->onwire; cp.l > 0;) {
        int crlf_off = usipy_sip_msg_break_down(&mit);
        if (crlf_off < 0)
            break;
        const char *chp = rp->onwire.s.ro + crlf_off;
        if (rp->nhdrs > 0) {
            if (chp == cp.s.ro) {
                cp.l -= 2;
                cp.s.ro += 2;
                /* End of headers reached */
                break;
            }
            if (USIPY_ISWS(cp.s.ro[0])) {
                /* Continuation */
                goto multi_line;
            }
        } else if (rp->sline.onwire.l == 0) {
            rp->sline.onwire.s.ro = cp.s.ro;
            rp->sline.onwire.l = crlf_off;
            rp->kind = usipy_sip_sline_parse(&rp->sline);
            if (rp->kind == USIPY_SIP_MSG_UNKN)
                goto e1;
            goto next_line;
        }
        shp = &rp->hdrs[rp->nhdrs];
        if ((void *)(shp + 1) > (void *)((char *)(rp) + alloc_len))
            goto e1;
        rp->nhdrs += 1;
#if 0
        mit.ioffst = &(shp->col_offst);
#endif
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
    for (int i = 0; i < rp->nhdrs; i++) {
	struct usipy_sip_hdr *tsp = &(rp->hdrs[i]);
#if 0
        if (tsp->col_offst != NULL) {
            ESP_LOGI("foobar", " header[%d]: col_offst = %d", i,
              tsp->col_offst - tsp->onwire.full.s.ro);
        }
#endif
        if (usipy_sip_hdr_preparse(tsp) != 0) {
            goto e1;
        }
        rp->hdr_masks.present |= USIPY_HF_MASK(tsp);
    }
    if (cp.l > 0) {
        rp->body = cp;
        if (mit.i < len) {
            memcpy(rp->onwire.s.rw + mit.i, buf + mit.i, len - mit.i);
        }
    }
    rp->heap.first = (void *)&rp->hdrs[rp->nhdrs];
    ralgn = USIPY_REALIGN((uintptr_t)rp->heap.first);
    if ((void *)ralgn != rp->heap.first) {
        rp->heap.first = (void *)(ralgn + (1 << USIPY_MEM_ALIGNOF));
    }
    rp->heap.free = rp->heap.first;
    rp->heap.size = USIPY_REALIGN(alloc_len - (rp->heap.first - (void *)rp));
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

    switch (msg->kind) {
    case USIPY_SIP_MSG_RES:
        ESP_LOGI(log_tag, "Message[%p] is SIP RESPONSE: status_code = %u, "
          "reason_phrase = \"%.*s\", heap remaining %d", msg,
          msg->sline.parsed.sl.status_code, msg->sline.parsed.sl.reason_phrase.l,
          msg->sline.parsed.sl.reason_phrase.s.ro, usipy_msg_heap_remaining(&msg->heap));
        break;

    case USIPY_SIP_MSG_REQ:
        ESP_LOGI(log_tag, "Message[%p] is SIP REQUEST: method(onwire) = \"%.*s\", "
          "method(canonic) = \"%.*s\", ruri = \"%.*s\", heap remaining %d", msg,
          msg->sline.parsed.rl.method.l, msg->sline.parsed.rl.method.s.ro,
          msg->sline.parsed.rl.mtype->name.l, msg->sline.parsed.rl.mtype->name.s.ro,
          msg->sline.parsed.rl.ruri.l, msg->sline.parsed.rl.ruri.s.ro,
          usipy_msg_heap_remaining(&msg->heap));
        break;

    default:
        abort();
    }

    ESP_LOGI(log_tag, "start line = \"%.*s\"", msg->sline.onwire.l,
      msg->sline.onwire.s.ro);
    for (int i = 0; i < msg->nhdrs; i++) {
        const struct usipy_sip_hdr *shp = &msg->hdrs[i];
        ESP_LOGI(log_tag, "header[%d @ %p], .hf_type = %p, .onwire.hf_type = %p", i,
          shp, shp->hf_type, shp->onwire.hf_type);
        ESP_LOGI(log_tag, "  .onwire.type = %d", shp->onwire.hf_type->cantype);
	ESP_LOGI(log_tag, "  .name = \"%.*s\"", shp->onwire.name.l, shp->onwire.name.s.ro);
	ESP_LOGI(log_tag, "  .value = \"%.*s\"", shp->onwire.value.l, shp->onwire.value.s.ro);
    }
    ESP_LOGI(log_tag, "body = \"%.*s\"", msg->body.l, msg->body.s.ro);
}

#define USIPY_HF_ISMSET(msk, h) ((msk) & USIPY_HFT_MASK(h))
#define USIPY_MSG_HDR_PARSED(msp, h) (USIPY_HF_ISMSET(msp->hdr_masks.parsed, (h)))
#define USIPY_MSG_HDR_PRESENT(msp, h) (USIPY_HF_ISMSET(msp->hdr_masks.present, (h)))

int
usipy_sip_msg_parse_hdrs(struct usipy_msg *mp, uint64_t parsemask)
{

    if ((mp->hdr_masks.present & parsemask) != parsemask)
        return(-1);
    parsemask &= ~(mp->hdr_masks.parsed);
    for (int i = 0; i < mp->nhdrs; i++) {
        struct usipy_sip_hdr *shp = &mp->hdrs[i];
        if (!USIPY_HF_ISMSET(parsemask, shp->hf_type->cantype))
            continue;
        switch (shp->hf_type->cantype) {
        case USIPY_HF_CSEQ:
            shp->parsed.cseq = usipy_sip_hdr_cseq_parse(&mp->heap, &shp->onwire.value);
            if (shp->parsed.cseq == NULL)
                return (-1);
            break;

        case USIPY_HF_CALLID:
            shp->parsed.generic = &shp->onwire.value;
	    break;

        default:
            return (-1);
	}
    }
    mp->hdr_masks.parsed |= parsemask;
    return (0);
}

#include <lwip/def.h>

#ifndef BYTE_ORDER
#error BYTE_ORER is unknown
#endif

#if BYTE_ORDER == LITTLE_ENDIAN || 1
#  define LE32TOH(dp, sp) /* Nop */
#else
#  define LE32TOH(dp, sp) { \
    *(uint32_t *)(dp) = (uint32_t) \
     ((const char *)(sp))[0] | \
     ((const char *)(sp))[1] << 8 | \
     ((const char *)(sp))[2] << 16 | \
     ((const char *)(sp))[3] << 24; \
  }
#endif

/*
 * Input string: "foo\r\nbar\r\nfoo\r\nbar\r\nfoo\r\nbar\r\nfoo\r\nbar\r\n"
 * Output offsets: 3 8 13 18 23 28 33 38 -1
 */

static int
usipy_sip_msg_break_down(struct usipy_sip_msg_iterator *mip)
{
    static const uint32_t mskA = ('\r' << 0) | ('\n' << 8) | ('\r' << 16) | ('\n' << 24);
    static const uint32_t mskB = ('\n' << 0) | ('\r' << 8) | ('\n' << 16) | ('\r' << 24);
    uint32_t val, mvalA, mvalB, oword;

    if (mip->cshift == 0 && mip->oword != 0) {
        char boff;
gotresult:
        boff = ffs(mip->oword) - 1;
        mip->oword ^= (1 << boff);
        return (mip->i - (sizeof(val) * 8) + boff - 1);
    }

    for (; mip->i < mip->msg_onwire.l; mip->i += sizeof(val)) {
        int remain = mip->msg_onwire.l - mip->i;
        if (remain < sizeof(val)) {
            val = 0;
            for (int j = 0; j < remain; j++) {
                val |= mip->msg_onwire.s.ro[mip->i + j] << (j * 8);
                mip->msg_copy->s.rw[mip->i + j] = mip->msg_onwire.s.ro[mip->i + j];
            }
        } else {
            memcpy(&val, mip->msg_onwire.s.ro + mip->i, sizeof(val));
            memcpy(mip->msg_copy->s.rw + mip->i, &val, sizeof(val));
            LE32TOH(&val, &val);
        }
        mvalA = val ^ mskA;
        mvalB = val ^ mskB;
        int chkover = 0, chkcarry = 0;
        if (mvalA == 0) {
            oword = 0b1010 << mip->cshift;
        } else if ((mvalA & 0x0000FFFF) == 0) {
            oword = 0b0010 << mip->cshift;
            chkcarry = 1;
        } else if ((mvalA &0xFFFF0000) == 0) {
            oword = 0b1000 << mip->cshift;
            chkover = 1;
        } else if ((mvalB &0x00FFFF00) == 0) {
            oword = 0b0100 << mip->cshift;
            chkcarry = 1;
            chkover = 1;
        } else {
            oword = 0b0000;
            chkcarry = 1;
            chkover = 1;
        }
#if 0
        if (mip->imask != 0) {
            uint32_t tval = val ^ mip->imask;
            for (int j = 0; j < sizeof(tval); j++) {
                if ((tval & 0xff) == 0) {
                    *(mip->ioffst) = mip->msg_copy->s.ro + mip->i + j;
                    mip->imask = 0;
                    break;
                }
                tval >>= 8;
            }
        }
#endif
        if (mip->over) {
            mip->over = 0;
            if (chkcarry && (mvalB & 0x000000FF) == 0) {
                oword |= 0b0001 << mip->cshift;
            }
        }
        if (chkover) {
            if ((mvalB & 0xFF000000) == 0) {
                mip->over = 1;
            }
        }
        mip->oword |= oword;
        if (mip->cshift == 28) {
            mip->cshift = 0;
            if (mip->oword != 0) {
                mip->i += sizeof(val);
                goto gotresult;
            }
        } else {
            mip->cshift += 4;
        }
    }
    if (mip->cshift != 0 && mip->oword != 0) {
        mip->i += (sizeof(val) * 8) - mip->cshift;
	mip->cshift = 0;
        goto gotresult;
    }

    return (-1);
}
