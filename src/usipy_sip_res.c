#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "usipy_debug.h"
#include "usipy_types.h"
#include "usipy_str.h"
#include "usipy_sip_sline.h"
#include "usipy_msg_heap.h"
#include "usipy_sip_msg.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_res.h"

static void
scode2str(unsigned int scode, char *res)
{

    res[0] = '0' + (scode / 100);
    res[1] = '0' + (scode % 100) / 10;
    res[2] = '0' + (scode % 10);
    res[3] = ' ';
}

struct usipy_msg *
usipy_sip_res_ctor_fromreq(const struct usipy_msg *reqp,
  const struct usipy_sip_status *slp)
{
    uint64_t copyfirst = USIPY_HFT_MASK(USIPY_HF_FROM) | USIPY_HFT_MASK(USIPY_HF_CALLID) | \
      USIPY_HFT_MASK(USIPY_HF_TO) | USIPY_HFT_MASK(USIPY_HF_CSEQ);
    uint64_t copyall = USIPY_HFT_MASK(USIPY_HF_VIA) | USIPY_HFT_MASK(USIPY_HF_RECORDROUTE);
    size_t tlen;
    int nhdrs;
    struct usipy_msg *rp;
    const struct usipy_sip_request_line *rlin = &(reqp->sline.parsed.rl);

    tlen = sizeof(struct usipy_msg) + (reqp->heap.first +
      reqp->heap.tsize) - (void *)&(reqp->_storage[0]);
    rp = malloc(tlen);
    if (rp == NULL) {
        return (NULL);
    }
    memset(rp, '\0', sizeof(*rp));

    rp->kind = USIPY_SIP_MSG_RES;
    char *cp;
    struct usipy_sip_status_line *slout = &(rp->sline.parsed.sl);
    cp = rp->onwire.s.rw = &(rp->_storage[0]);
    rp->hdrs = (struct usipy_sip_hdr *)&(rp->_storage[0]) + USIPY_ALIGNED_SIZE(reqp->onwire.l);

    slout->version.s.rw = cp;
    memcpy(cp, rlin->onwire.version.s.ro, rlin->onwire.version.l);
    slout->version.l = rlin->onwire.version.l;
    cp += rlin->onwire.version.l;
    cp[0] = ' ';
    cp += 1;
    scode2str(slp->code, cp);
    slout->status.code = slp->code;
    cp += 4;
    slout->status.reason_phrase.s.rw = cp;
    memcpy(cp, slp->reason_phrase.s.ro, slp->reason_phrase.l);
    slout->status.reason_phrase.l = slp->reason_phrase.l;
    cp += slp->reason_phrase.l;
    memcpy(cp, "\r\n", 2);
    cp += 2;

    for (int i = 0; i < reqp->nhdrs; i++) {
        const struct usipy_sip_hdr *shp = &reqp->hdrs[i];
        struct usipy_sip_hdr *ohp = &rp->hdrs[rp->nhdrs];

        if (USIPY_HF_ISMSET(copyfirst, shp->hf_type->cantype)) {
            ohp->hf_type = ohp->onwire.hf_type = shp->hf_type;
            memcpy(cp, shp->hf_type->name.s.ro, shp->hf_type->name.l);
            ohp->onwire.name.s.ro = ohp->onwire.full.s.ro = cp;
            ohp->onwire.name.l = shp->hf_type->name.l;
            cp += shp->hf_type->name.l;
            memcpy(cp, ": ", 2);
            cp += 2;
            memcpy(cp, shp->onwire.value.s.ro, shp->onwire.value.l);
            ohp->onwire.value.s.ro = cp;
            ohp->onwire.value.l = shp->onwire.value.l;
            cp += shp->onwire.value.l;
            ohp->onwire.full.l = cp - ohp->onwire.full.s.ro;
            memcpy(cp, "\r\n", 2);
            cp += 2;
            rp->nhdrs += 1;
            copyfirst &= ~USIPY_HFT_MASK(shp->hf_type->cantype);
            continue;
        }
        if (USIPY_HF_ISMSET(copyall, shp->hf_type->cantype)) {
            ohp->hf_type = ohp->onwire.hf_type = shp->hf_type;
            memcpy(cp, shp->hf_type->name.s.ro, shp->hf_type->name.l);
            ohp->onwire.name.s.ro = ohp->onwire.full.s.ro = cp;
            ohp->onwire.name.l = shp->hf_type->name.l;
            cp += shp->hf_type->name.l;
            memcpy(cp, ": ", 2);
            cp += 2;
            memcpy(cp, shp->onwire.value.s.ro, shp->onwire.value.l);
            ohp->onwire.value.s.ro = cp;
            ohp->onwire.value.l = shp->onwire.value.l;
            cp += shp->onwire.value.l;
            ohp->onwire.full.l = cp - ohp->onwire.full.s.ro;
            memcpy(cp, "\r\n", 2);
            cp += 2;
            rp->nhdrs += 1;
        }
    }
    if (copyfirst != 0) {
        goto e0;
    }

    memcpy(cp, "\r\n", 2);
    cp += 2;
    rp->onwire.l = cp - rp->onwire.s.rw;

    return (rp);
e0:
    free(rp);
    return (NULL);
}
