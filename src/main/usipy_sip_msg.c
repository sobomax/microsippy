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

#define CRLF_MAP_SIZE(splen, store_t) ( \
  ( \
   (splen / (sizeof(store_t) * 8)) + \
   ((splen & ((sizeof(store_t) * 8) - 1)) != 0) \
  ) * sizeof(store_t) \
)

#define USIPY_HFS_NMIN (12)

struct usipy_msg *
usipy_sip_msg_ctor_fromwire(const char *buf, size_t len,
  struct usipy_msg_parse_err *perrp)
{
    struct usipy_msg *rp;
    size_t alloc_len, hf_prealloclen;
    uintptr_t ralgn;
    const char *allocend;

    hf_prealloclen = len < (sizeof(struct usipy_sip_hdr) * USIPY_HFS_NMIN) ?
      sizeof(struct usipy_sip_hdr) * USIPY_HFS_NMIN : USIPY_ALIGNED_SIZE(len);
    alloc_len = sizeof(struct usipy_msg) + USIPY_ALIGNED_SIZE(len) +
      hf_prealloclen + USIPY_ALIGNED_SIZE(CRLF_MAP_SIZE(len, uint32_t));
    rp = malloc(alloc_len);
    if (rp == NULL) {
        goto e0;
    }
    allocend = ((const char *)rp) + alloc_len;
    memset(rp, '\0', sizeof(struct usipy_msg));
    memset(rp->_storage + len, '\0', allocend - rp->_storage - len);
    memcpy(rp->_storage, buf, len);
    rp->onwire.s.rw = rp->_storage;
    rp->onwire.l = len;
    rp->heap.first = (void *)(rp->_storage + USIPY_ALIGNED_SIZE(len));
    rp->hdrs = (struct usipy_sip_hdr *)(rp->heap.first);
    int nempty = 0;
    struct usipy_sip_hdr *shp = NULL;
    for (struct usipy_str cp = rp->onwire; cp.l > 0;) {
        const char *chp = memmem(cp.s.ro, cp.l, USIPY_CRLF, USIPY_CRLF_LEN);
        if (chp == NULL)
            break;
        if (rp->nhdrs > 0) {
            if (chp == cp.s.ro) {
                /* End of headers reached */
                if (nempty > 0)
                    break;
                nempty += 1;
                continue;
            }
            if (USIPY_ISWS(cp.s.ro[0])) {
                /* Continuation */
                goto multi_line;
            }
        } else if (rp->sline.onwire.l == 0) {
            rp->sline.onwire.s.ro = cp.s.ro;
            rp->sline.onwire.l = chp - cp.s.ro;
            rp->kind = usipy_sip_sline_parse(&rp->sline);
            if (rp->kind == USIPY_SIP_MSG_UNKN)
                goto e1;
            goto next_line;
        }
        shp = &rp->hdrs[rp->nhdrs];
        if ((void *)(shp + 1) > (void *)((char *)(rp) + alloc_len))
            goto e1;
        rp->nhdrs += 1;
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
        if (usipy_sip_hdr_preparse(tsp) != 0)
            goto e1;
        rp->hdr_masks.present |= USIPY_HF_MASK(tsp);
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

#include <netinet/in.h>

/*
 * Input string: "foo\r\nbar\r\nfoo\r\nbar\r\nfoo\r\nbar\r\nfoo\r\nbar\r\n"
 * Output bytes |   0......7  8......15 16.....23 24.....31
 *                [ 00011000  11000110  00110001  10001100 ]
 *                [ 01100011  00000000  00000000  00000000 ]
 */

int
usipy_sip_msg_break_down(const struct usipy_str *sp, uint32_t *omap)
{
    static const uint32_t mskB = ('\r' << 0) | ('\n' << 8) | ('\r' << 16) | ('\n' << 24);
    static const uint32_t mskA = ('\n' << 0) | ('\r' << 8) | ('\n' << 16) | ('\r' << 24);
    uint32_t val, mvalA, mvalB, oword;
    int i, over, last;
    uint32_t *opp = omap;

    over = 0;
    last = 0;
    oword = 0;
    char cshift = 0;
    for (i = 0; i < sp->l; i += sizeof(val)) {
        memcpy(&val, sp->s.ro + i, sizeof(val));
        val = ntohl(val);
onemotime:
        mvalA = val ^ mskA;
        mvalB = val ^ mskB;
        int chkover = 0, chkcarry = 0;
        if (mvalA == 0) {
            oword |= 0b1111 << cshift;
        } else if ((mvalA & 0xFFFF0000) == 0) {
            oword |= 0b0011 << cshift;
            chkcarry = 1;
        } else if ((mvalA &0x0000FFFF) == 0) {
            oword |= 0b1100 << cshift;
            chkover = 1;
        } else if ((mvalB &0x00FFFF00) == 0) {
            oword |= 0b0110 << cshift;
            chkcarry = 1;
            chkover = 1;
        } else {
            //oword |= 0b0000;
            chkcarry = 1;
            chkover = 1;
        }
        if (over) {
            if (chkcarry && (mvalB & 0xFF000000) == 0) {
                if (cshift == 0) {
                    oword |= 0b0001;
                    opp[-1] |= 1 << 31;
                } else {
                    oword |= 0b00011000 << (cshift - 4);
                }
            }
            over = 0;
        }
        if (chkover) {
            if ((mvalB & 0x000000FF) == 0) {
                over = 1;
            }
        }
        if (cshift == 28) {
            if (oword != 0) {
                *opp = oword;
                oword = 0;
            }
            opp += 1;
            cshift = 0;
        } else {
            cshift += 4;
        }
    }
    if (i > sp->l && !last) {
        const char *cp = sp->s.ro + i - sizeof(val);
        switch (sizeof(val) - (i - sp->l)) {
            val = (uint32_t)(cp[0]);
            break;
        case 2:
            val = (uint32_t)(cp[0]) | (8 << cp[1]);
            break;
        case 3:
            val = (uint32_t)(cp[0]) | (8 << cp[1]) | (16 << cp[2]);
            break;
        }
        last = 1;
        goto onemotime;
    }
    if (cshift != 0 && oword != 0) {
        *opp = oword;
        opp += 1;
    }

    return (opp - omap);
}
