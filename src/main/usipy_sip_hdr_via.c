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
    struct usipy_str sent_by, sent_by_port_s;
    struct usipy_sip_hdr_via *vp;
    struct usipy_str sent_protocol_name;
    struct usipy_str sent_protocol_version;
    struct usipy_str sent_protocol_transport;
    struct usipy_str sent_by_host;
    unsigned int sent_by_port;

    if (usipy_str_split3(hvp, '/', &sent_protocol_name, &sent_protocol_version, &s4) != 0) {
        return (NULL);
    }
    usipy_str_ltrm_b(&s4); /* UDP */
    if (usipy_str_splitlws(&s4, &sent_protocol_transport, &s4) != 0) {
        return (NULL);
    }
    usipy_str_ltrm_e(&sent_protocol_name); /* SIP */
    usipy_str_ltrm_b(&sent_protocol_version); /* 2.0 */
    usipy_str_ltrm_e(&sent_protocol_version);
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
    if (usipy_str_split(&sent_by, ':', &sent_by_host, &sent_by_port_s) == 0) {
        usipy_str_ltrm_e(&sent_by_host);
        usipy_str_ltrm_b(&sent_by_port_s);
        if (usipy_str_atoui_range(&sent_by_port_s, &sent_by_port, 1, 65535) != 0) {
            return (NULL);
        }
    } else {
        sent_by_host = sent_by;
        sent_by_port = 0;
    }
    vp = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_hdr_via));
    if (vp == NULL) {
        return (NULL);
    }
    vp->sent_protocol.name = sent_protocol_name;
    vp->sent_protocol.version = sent_protocol_version;
    vp->sent_protocol_transport = sent_protocol_transport;
    vp->sent_by.host = sent_by_host;
    vp->sent_by.port = sent_by_port;

    return (vp);
}

#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "  ." #sname " = \"%.*s\"", vp->sname.l, vp->sname.s.ro)
#define DUMP_UINT(sname) \
    ESP_LOGI(log_tag, "  ." #sname " = %u", vp->sname)

void
usipy_sip_hdr_via_dump(const char *log_tag, const union usipy_sip_hdr_parsed *up)
{
    const struct usipy_sip_hdr_via *vp = up->via;

    DUMP_STR(sent_protocol.name);
    DUMP_STR(sent_protocol.version);
    DUMP_STR(sent_protocol.transport);
    DUMP_STR(sent_by.host);
    if (vp->sent_by.port > 0)
        DUMP_UINT(sent_by.port);
}
