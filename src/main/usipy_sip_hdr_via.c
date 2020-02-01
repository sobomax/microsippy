#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"

#include "usipy_msg_heap.h"
#include "usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_via.h"

#define VH_SIZEOF(nparams) ( \
  sizeof(struct usipy_sip_hdr_via) + (sizeof(struct usipy_sip_param) * (nparams)) \
)

struct usipy_sip_hdr_via *
usipy_sip_hdr_via_parse(struct usipy_msg_heap *mhp,
  const struct usipy_str *hvp)
{
    struct usipy_str s4;
    struct usipy_str sent_by, sent_by_port;
    struct usipy_sip_hdr_via tv, *vp;

    if (usipy_str_split3(hvp, '/', &tv.sent_protocol.name, &tv.sent_protocol.version, &s4) != 0) {
        return (NULL);
    }
    usipy_str_ltrm_b(&s4); /* UDP */
    if (usipy_str_splitlws(&s4, &tv.sent_protocol.transport, &s4) != 0) {
        return (NULL);
    }
    usipy_str_ltrm_e(&tv.sent_protocol.name); /* SIP */
    usipy_str_ltrm_b(&tv.sent_protocol.version); /* 2.0 */
    usipy_str_ltrm_e(&tv.sent_protocol.version);
    usipy_str_ltrm_b(&s4);
    struct usipy_str paramspace;
    if (usipy_str_split(&s4, ';', &sent_by, &paramspace) != 0) {
        sent_by = s4;
        paramspace.l = 0;
    } else {
        usipy_str_ltrm_e(&sent_by);
        usipy_str_ltrm_b(&paramspace);
    }
    if (sent_by.l == 0) {
        return (NULL);
    }
    if (usipy_str_split(&sent_by, ':', &tv.sent_by.host, &sent_by_port) == 0) {
        usipy_str_ltrm_e(&tv.sent_by.host);
        usipy_str_ltrm_b(&sent_by_port);
        if (usipy_str_atoui_range(&sent_by_port, &tv.sent_by.port, 1, 65535) != 0) {
            return (NULL);
        }
    } else {
        tv.sent_by.host = sent_by;
        tv.sent_by.port = 0;
    }
    vp = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_hdr_via));
    if (vp == NULL) {
        return (NULL);
    }
    tv.nparams = 0;
    while (paramspace.l != 0;) {
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
        if (usipy_msg_heap_aextend(mhp, vp, VH_SIZEOF(tv.nparams),
          VH_SIZEOF(tv.nparams + 1)) != 0) {
            return (NULL);
        }
        vp->params[tv.nparams].token = param_token;
        vp->params[tv.nparams].value = param_value;
        tv.nparams++;
    }
    *vp = tv;

    return (vp);
}

#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "  .parsed->via." #sname " = \"%.*s\"", vp->sname.l, vp->sname.s.ro)
#define DUMP_UINT(sname) \
    ESP_LOGI(log_tag, "  .parsed->via." #sname " = %u", vp->sname)

void
usipy_sip_hdr_via_dump(const union usipy_sip_hdr_parsed *up, const char *log_tag)
{
    const struct usipy_sip_hdr_via *vp = up->via;

    DUMP_STR(sent_protocol.name);
    DUMP_STR(sent_protocol.version);
    DUMP_STR(sent_protocol.transport);
    DUMP_STR(sent_by.host);
    if (vp->sent_by.port > 0)
        DUMP_UINT(sent_by.port);
    for (int i = 0; i < vp->nparams; i++) {
        DUMP_STR(params[i].token);
        DUMP_STR(params[i].value);
    }
}
