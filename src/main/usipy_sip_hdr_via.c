#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"

#include "usipy_msg_heap.h"
#include "usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_via.h"
//#include "usipy_sip_method_db.h"

struct usipy_sip_hdr_via *
usipy_sip_hdr_via_parse(struct usipy_msg_heap *mhp,
  const struct usipy_str *hvp)
{
    struct usipy_str s4;
    struct usipy_str sent_by, sent_by_port;
    struct usipy_sip_hdr_via *vp;

    vp = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_hdr_via));
    if (vp == NULL) {
        return (NULL);
    }
    if (usipy_str_split3(hvp, '/', &vp->sent_protocol.name, &vp->sent_protocol.version, &s4) != 0) {
        return (NULL);
    }
    usipy_str_ltrm_b(&s4); /* UDP */
    if (usipy_str_splitlws(&s4, &vp->sent_protocol.transport, &s4) != 0) {
        return (NULL);
    }
    usipy_str_ltrm_e(&vp->sent_protocol.name); /* SIP */
    usipy_str_ltrm_b(&vp->sent_protocol.version); /* 2.0 */
    usipy_str_ltrm_e(&vp->sent_protocol.version);
    usipy_str_ltrm_b(&s4);
    if (usipy_str_split(&s4, ';', &sent_by, &s4) != 0) {
        sent_by = s4;
        s4.l = 0;
    } else {
        usipy_str_ltrm_e(&sent_by);
    }
    if (sent_by.l == 0) {
        return (NULL);
    }
    if (usipy_str_split(&sent_by, ':', &vp->sent_by.host, &sent_by_port) == 0) {
        usipy_str_ltrm_e(&vp->sent_by.host);
        usipy_str_ltrm_b(&sent_by_port);
    } else {
        vp->sent_by.host = sent_by;
    }

    return (vp);
}

#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "  ." #sname " = \"%.*s\"", vp->sname.l, vp->sname.s.ro)

void
usipy_sip_hdr_via_dump(const char *log_tag, const union usipy_sip_hdr_parsed *up)
{
    const struct usipy_sip_hdr_via *vp = up->via;

    DUMP_STR(sent_protocol.name);
    DUMP_STR(sent_protocol.version);
    DUMP_STR(sent_protocol.transport);
    DUMP_STR(sent_by.host);
}
