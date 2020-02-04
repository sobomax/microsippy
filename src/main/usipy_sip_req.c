#include <stdint.h>

#include "usipy_sip_str.h"
#include "usipy_sip_sline.h"
#include "usipy_sip_msg.h"
#include "usipy_sip_req.h"

int
usipy_sip_req_parse_ruri(struct usipy_msg *mp)
{
    assert(mp->kind == USIPY_SIP_MSG_REQ);

    if (mp->sline.parsed.rl.ruri != NULL)
        return (-1);

    mp->sline.parsed.rl.ruri = usipy_sip_uri_parse(&mp->heap,
      &mp->sline.parsed.rl.onwire.ruri);
    if (mp->sline.parsed.rl.ruri == NULL)
        return (-1);
    return (0);
}
