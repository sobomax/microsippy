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

    iup = *surip;
    if (usipy_str_split_elem_nlws(&iup, ':', &rval.proto) != 0)
        return (NULL);
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
    if (usipy_str_split_elem_nlws(&pnum, '@', &rval.user) == 0) {
        if (usipy_str_split_elem_nlws(&rval.user, ':', &rval.password) != 0) {
            rval.password = USIPY_STR_NULL;
        }
    } else {
        rval.user = USIPY_STR_NULL;
        rval.password = USIPY_STR_NULL;
    }
    if (usipy_str_split_elem_nlws(&pnum, ':', &rval.host) == 0) {
        if (usipy_str_atoui_range(&pnum, &rval.port, 1, 65535) != 0) {
            return (NULL);
        }
    } else {
        rval.host = pnum;
        rval.port = 0;
    }

    rval.headers = hspace;
    up = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_uri));
    if (up == NULL) {
        return (NULL);
    }
    rval.nparams = 0;
    rval.parameters = NULL;
    while (pspace.l != 0) {
        if (rval.nparams == 0)
            rval.parameters = &up->params[0];
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
        if (usipy_msg_heap_aextend(mhp, up, URI_SIZEOF(rval.nparams + 1)) != 0) {
            return (NULL);
        }
        up->parameters[rval.nparams].token = param_token;
        up->params[rval.nparams].value = param_value;
        rval.nparams++;
    }

    *up = rval;
    return (rp);
}

#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "%s" #sname " = \"%.*s\"", log_pref, up->sname.l, \
      up->sname.s.ro)
#define DUMP_UINT(sname) \
    ESP_LOGI(log_tag, "%s" #sname " = %u", log_pref, up->sname)
#define DUMP_PARAM(sname, idx) \
    ESP_LOGI(log_tag, "%svia." #sname "[%d] = \"%.*s\"=\"%.*s\"", log_pref, \
      idx, vp->sname[i].token.l, vp->sname[i].token.s.ro, vp->sname[i].value.l, \
      vp->sname[i].value.s.ro)

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
    DUMP_STR(headers);
}
