#include <stdint.h>
#include <stdlib.h>

#include "usipy_debug.h"
#include "usipy_str.h"
#include "usipy_tvpair.h"
#include "usipy_sip_sline.h"
#include "usipy_sip_uri.h"
#include "usipy_msg_heap.h"
#include "usipy_sip_msg.h"
#include "usipy_sip_req.h"

int
usipy_sip_req_parse_ruri(struct usipy_msg *mp)
{
    USIPY_DASSERT(mp->kind == USIPY_SIP_MSG_REQ);

    if (mp->sline.parsed.rl.ruri != NULL)
        return (-1);

    mp->sline.parsed.rl.ruri = usipy_sip_uri_parse(&mp->heap,
      &mp->sline.parsed.rl.onwire.ruri);
    if (mp->sline.parsed.rl.ruri == NULL)
        return (-1);
    return (0);
}

#if 0
struct usipy_msg *
usipy_sip_res_ctor_fromreq(const struct usipy_msg *reqp)
{
    uint64_t copyfirst = USIPY_HFT_MASK(USIPY_HF_FROM) | USIPY_HFT_MASK(USIPY_HF_CALLID) | \
      USIPY_HFT_MASK(USIPY_HF_TO) | USIPY_HFT_MASK(USIPY_HF_CSEQ);
    uint64_t copyall = USIPY_HFT_MASK(USIPY_HF_VIA) | USIPY_HFT_MASK(USIPY_HF_RECORDROUTE);
    size_t tlen = 0;

    for (int i = 0; i < reqp->nhdrs; i++) {
        struct usipy_sip_hdr *shp = &mp->hdrs[i];

        if (USIPY_HF_ISMSET(copyfirst, shp->hf_type->cantype)) {
            tlen += shp->onwire.full.l + 2;
            copyfirst &= ~USIPY_HFT_MASK(shp->hf_type->cantype);
            continue;
        }
        if (USIPY_HF_ISMSET(copyall, shp->hf_type->cantype)) {
            tlen += shp->onwire.full.l + 2;
        }
    }
    if (copyfirst != 0) {
        return (NULL);
    }
    return (struct usipy_msg *)((void *)tlen);

}
#endif
