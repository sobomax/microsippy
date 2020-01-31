#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"

#include "usipy_msg_heap.h"
#include "usipy_str.h"
#include "usipy_sip_hdr_via.h"
#include "usipy_sip_method_db.h"

struct usipy_sip_hdr_via *
usipy_sip_hdr_via_parse(struct usipy_msg_heap *mhp,
  const struct usipy_str *hvp)
{
    struct usipy_str s1, s2, s3, s4;
    struct usipy_str sent_by;
    struct usipy_sip_hdr_via *vp;

    if (usipy_str_split3(hvp, '/', &s1, &s2, &s4) != 0) {
        return (NULL);
    }
    if (usipy_str_splitlws(&s4, &s3, &s4) != 0) {
        return (NULL);
    }
    usipy_str_ltrm_e(&s1); /* SIP */
    usipy_str_ltrm_b(&s2); /* 2.0 */
    usipy_str_ltrm_e(&s2);
    usipy_str_ltrm_b(&s3); /* UDP */
    usipy_str_ltrm_b(&s4);
    if (usipy_str_split(&s4, ';', &sent_by, &s4) != 0) {
        sent_by = s4;
    } else {
        usipy_str_ltrm_e(&sent_by);
    }

    vp = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_hdr_via));
    if (vp == NULL) {
        return (NULL);
    }
    vp->sent_by.host = sent_by;
    vp->sent_protocol.name = s1;
    vp->sent_protocol.version = s2;
    vp->sent_protocol.transport = s3;
    return (vp);
}

#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "  ." #sname " = \"%.*s\"", vp->sname.l, vp->sname.s.ro)

void
usipy_sip_hdr_via_dump(const struct usipy_sip_hdr_via *vp)
{

    DUMP_STR(sent_protocol.name);
    DUMP_STR(sent_protocol.version);
    DUMP_STR(sent_protocol.transport);
    DUMP_STR(sent_protocol.sent_by.host);
}
