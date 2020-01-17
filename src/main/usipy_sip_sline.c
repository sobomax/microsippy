#include <stdint.h>
#include <stdlib.h>

#include "usipy_misc.h"
#include "usipy_str.h"
#include "usipy_sip_sline.h"
#include "usipy_msg_heap.h"
#include "usipy_sip_msg.h"

enum usipy_sip_msg_kind
usipy_sip_sline_parse(struct usipy_sip_sline *slp)
{
    struct usipy_str s1, s2, s3, s4;
    enum usipy_sip_msg_kind r;

    if (usipy_str_split(&slp->onwire, USIPY_SP, &s1, &s2) != 0)
        return (USIPY_SIP_MSG_UNKN);
    if (usipy_str_split(&s2, USIPY_SP, &s3, &s4) != 0)
        return (USIPY_SIP_MSG_UNKN);
    if (usipy_verify_sip_version(&s1)) {
        if (usipy_str_atoui_range(&s3, &slp->parsed.sl.status_code,
          USIPY_SIP_SCODE_MIN, USIPY_SIP_SCODE_MAX) != 0)
            return (USIPY_SIP_MSG_UNKN);
        slp->parsed.sl.version = s1;
        slp->parsed.sl.reason_phrase = s4;
        r = USIPY_SIP_MSG_RES;
    } else if (usipy_verify_sip_version(&s4)) {
        slp->parsed.rl.method = s1;
        slp->parsed.rl.ruri = s3;
        slp->parsed.rl.version = s4;
        r = USIPY_SIP_MSG_REQ;
    } else {
	return (USIPY_SIP_MSG_UNKN);
    }
    return (r);
}
