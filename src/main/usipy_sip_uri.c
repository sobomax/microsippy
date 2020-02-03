#include <stdint.h>
#include <stdlib.h>

#include "usipy_msg_heap.h"
#include "usipy_str.h"
#include "usipy_sip_uri.h"

struct usipy_sip_uri *
usipy_sip_uri_parse(struct usipy_msg_heap *mhp, const struct usipy_str *up)
{
    struct usipy_str iup, pspace, hspace, pnum;
    struct usipy_sip_uri rval, *rp;

    iup = *up;
    if (usipy_str_split_elem(&iup, ':', &rval.proto) != 0)
        return (NULL);
    if (usipy_str_split_elem(&iup, ';', &rval.host) != 0) {
        pspace = USIPY_STR_NULL;
        if (usipy_str_split_elem(&iup, '?', &rval.host) != 0) {
            rval.host = iup;
            iup = USIPY_STR_NULL;
            hspace = USIPY_STR_NULL;
        } else {
            hspace = iup;
        }
    } else {
        if (usipy_str_split_elem(&iup, '?', &pspace) != 0) {
            pspace = iup;
            hspace = USIPY_STR_NULL;
        } else {
            hspace = iup;
        }
    }
    if (rval.host.l == 0)
        return (NULL);
    }
    if (usipy_str_split_elem(&rval.host, '@', &rval.user) == 0) {
        if (usipy_str_split_elem(&rval.user, ':', &rval.password) != 0) {
            rval.password = USIPY_STR_NULL;
        }
    } else {
        rval.user = USIPY_STR_NULL;
        rval.password = USIPY_STR_NULL;
    }
    if (usipy_str_split_elem(&rval.host, ':', &pnum) == 0) {
        if (usipy_str_atoui_range(&pnum, &rval.port, 1, 65535) != 0) {
            return (NULL);
        }
    } else {
        rval.port = 0;
    }

    rval.parameters = pspace;
    rval.headers = hspace;
    rp = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_uri));
    if (rp == NULL) {
        return (NULL);
    }
    *rp = rval;
    return (rp)
}
