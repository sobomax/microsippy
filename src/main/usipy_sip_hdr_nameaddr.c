#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "usipy_debug.h"
#include "usipy_msg_heap.h"
#include "usipy_msg_heap_rb.h"
#include "usipy_msg_heap_inl.h"
#include "usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_tvpair.h"
#include "usipy_sip_hdr_nameaddr.h"

#define NA_SIZEOF(nparams) ( \
  sizeof(struct usipy_sip_hdr_nameaddr) + (sizeof(struct usipy_tvpair) * (nparams)) \
)

union usipy_sip_hdr_parsed
usipy_sip_hdr_nameaddr_parse(struct usipy_msg_heap *mhp,
  const struct usipy_str *hvp)
{
    struct usipy_str iup = *hvp;
    struct usipy_str paramspace = USIPY_STR_NULL;
    union usipy_sip_hdr_parsed usp;
    struct usipy_sip_hdr_nameaddr *nap;
    struct usipy_msg_heap_cnt cnt;

    usp.contact = usipy_msg_heap_alloc_cnt(mhp, NA_SIZEOF(0), &cnt);
    if (usp.contact == NULL) {
        return (usp);
    }
    nap = usp.contact;

    if (usipy_str_split_elem(&iup, '<', &nap->display_name) != 0) {
        if (usipy_str_split_elem(&iup, ';', &nap->addr_spec) == 0) {
            paramspace = iup;
        } else {
            nap->addr_spec = iup;
        }
    } else {
         if (usipy_str_split_elem(&iup, '>', &nap->addr_spec) != 0) {
            /* No closing '>' */
            goto rollback;
         }
         if (iup.l > 0) {
            if (iup.l == 1 || iup.s.ro[0] != ';') {
                /* Some junk after closing '>' */
                goto rollback;
            }
            paramspace.l = iup.l - 1;
            paramspace.s.ro = iup.s.ro + 1;
         }
    }

    while (paramspace.l != 0) {
        struct usipy_str thisparam;
        if (usipy_str_split_elem(&paramspace, ';', &thisparam) != 0) {
            thisparam = paramspace;
            paramspace.l = 0;
        }
        struct usipy_str param_token, param_value;
        if (usipy_str_split(&thisparam, '=', &param_token, &param_value) == 0) {
            usipy_str_ltrm_e(&param_token);
            usipy_str_ltrm_b(&param_value);
        } else {
            param_token = thisparam;
            param_value = USIPY_STR_NULL;
        }
        if (usipy_msg_heap_aextend(mhp, NA_SIZEOF(nap->nparams + 1), &cnt) != 0) {
            goto rollback;
        }
        nap->params[nap->nparams].token = param_token;
        nap->params[nap->nparams].value = param_value;
        nap->nparams++;
    }

    return (usp);
rollback:
    usipy_msg_heap_rollback(mhp, &cnt);
    usp.contact = NULL;
    return (usp);
}

#define DUMP_PARAM(sname, idx) \
    ESP_LOGI(log_tag, "%snameaddr." #sname "[%d] = \"%.*s\"=\"%.*s\"", log_pref, \
      idx, nap->sname[i].token.l, nap->sname[i].token.s.ro, nap->sname[i].value.l, \
      nap->sname[i].value.s.ro)
#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "%snameaddr." #sname " = \"%.*s\"", log_pref, nap->sname.l, \
      nap->sname.s.ro)

void
usipy_sip_hdr_nameaddr_dump(const union usipy_sip_hdr_parsed *up, const char *log_tag,
  const char *log_pref)
{
    const struct usipy_sip_hdr_nameaddr *nap = up->contact;

    DUMP_STR(display_name);
    DUMP_STR(addr_spec);
    for (int i = 0; i < nap->nparams; i++) {
        DUMP_PARAM(params, i);
    }
}
