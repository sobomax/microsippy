#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"

#include "usipy_msg_heap.h"
#include "usipy_str.h"
#include "usipy_tvpair.h"
#include "usipy_sip_uri.h"

#define URI_SIZEOF(nparams) ( \
  sizeof(struct usipy_sip_uri) + (sizeof(struct usipy_tvpair) * (nparams)) \
)

struct usipy_sip_uri *
usipy_sip_uri_parse(struct usipy_msg_heap *mhp, const struct usipy_str *surip)
{
    struct usipy_str iup, pspace, hspace, pnum;
    struct usipy_sip_uri rval, *up;
    struct usipy_msg_heap_cnt cnt;

    iup = *surip;
    if (usipy_str_split_elem_nlws(&iup, ':', &rval.proto) != 0)
        return (NULL);
    if (usipy_str_split_elem_nlws(&iup, '@', &rval.user) == 0) {
        if (usipy_str_split_elem_nlws(&rval.user, ':', &rval.password) != 0) {
            rval.password = USIPY_STR_NULL;
        }
    } else {
        rval.user = USIPY_STR_NULL;
        rval.password = USIPY_STR_NULL;
    }
    if (usipy_str_split_elem_nlws(&iup, ';', &pnum) != 0) {
        pspace = USIPY_STR_NULL;
        if (usipy_str_split_elem_nlws(&iup, '?', &pnum) != 0) {
            pnum = iup;
            iup = USIPY_STR_NULL;
            hspace = USIPY_STR_NULL;
        } else {
            hspace = iup;
        }
    } else {
        if (usipy_str_split_elem_nlws(&iup, '?', &pspace) != 0) {
            pspace = iup;
            hspace = USIPY_STR_NULL;
        } else {
            hspace = iup;
        }
    }
    if (pnum.l == 0) {
        return (NULL);
    }
    if (usipy_str_split_elem_nlws(&pnum, ':', &rval.host) == 0) {
        if (usipy_str_atoui_range(&pnum, &rval.port, 1, 65535) != 0) {
            return (NULL);
        }
    } else {
        rval.host = pnum;
        rval.port = 0;
    }

    if (pspace.l == 0 && hspace.l == 0) {
        up = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_uri));
    } else {
        up = usipy_msg_heap_alloc_cnt(mhp, sizeof(struct usipy_sip_uri), &cnt);
    }
    if (up == NULL) {
        return (NULL);
    }
    rval.nparams = 0;
    rval.parameters = NULL;
    while (pspace.l != 0) {
        if (rval.nparams == 0)
            rval.parameters = &up->_tvpstorage[0];
        struct usipy_str thisparam;
        if (usipy_str_split_elem_nlws(&pspace, ';', &thisparam) != 0) {
            thisparam = pspace;
            pspace.l = 0;
        }
        struct usipy_str param_token, param_value;
        if (usipy_str_split(&thisparam, '=', &param_token, &param_value) != 0) {
            param_token = thisparam;
            param_value = USIPY_STR_NULL;
        }
        if (usipy_msg_heap_aextend(mhp, up, URI_SIZEOF(rval.nparams + 1), &cnt) != 0) {
            return (NULL);
        }
        rval.parameters[rval.nparams].token = param_token;
        rval.parameters[rval.nparams].value = param_value;
        rval.nparams++;
    }
    rval.nhdrs = 0;
    rval.headers = NULL;
    while (hspace.l != 0) {
        if (rval.nhdrs == 0)
            rval.headers = &up->_tvpstorage[rval.nparams];
        struct usipy_str thishdr;
        if (usipy_str_split_elem_nlws(&hspace, '&', &thishdr) != 0) {
            thishdr = hspace;
            hspace.l = 0;
        }
        struct usipy_str hdr_token, hdr_value;
        if (usipy_str_split(&thishdr, '=', &hdr_token, &hdr_value) != 0) {
            hdr_token = thishdr;
            hdr_value = USIPY_STR_NULL;
        }
        if (usipy_msg_heap_aextend(mhp, up, URI_SIZEOF(rval.nhdrs + rval.nparams + 1), &cnt) != 0) {
            return (NULL);
        }
        rval.headers[rval.nhdrs].token = hdr_token;
        rval.headers[rval.nhdrs].value = hdr_value;
        rval.nhdrs++;
    }

    *up = rval;
    return (up);
}

#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "%s" #sname " = \"%.*s\"", log_pref, up->sname.l, \
      up->sname.s.ro)
#define DUMP_UINT(sname) \
    ESP_LOGI(log_tag, "%s" #sname " = %u", log_pref, up->sname)
#define DUMP_PARAM(sname, idx) \
    ESP_LOGI(log_tag, "%s" #sname "[%d] = \"%.*s\"=\"%.*s\"", log_pref, \
      idx, up->sname[i].token.l, up->sname[i].token.s.ro, up->sname[i].value.l, \
      up->sname[i].value.s.ro)

void
usipy_sip_uri_dump(const struct usipy_sip_uri *up, const char *log_tag,
  const char *log_pref)
{

    DUMP_STR(proto);
    DUMP_STR(user);
    DUMP_STR(password);
    DUMP_STR(host);
    if (up->port > 0)
        DUMP_UINT(port);
    for (int i = 0; i < up->nparams; i++) {
        DUMP_PARAM(parameters, i);
    }
    for (int i = 0; i < up->nhdrs; i++) {
        DUMP_PARAM(headers, i);
    }
}
