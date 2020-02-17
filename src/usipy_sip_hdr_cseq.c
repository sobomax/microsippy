#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"

#include "usipy_debug.h"
#include "usipy_msg_heap.h"
#include "usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_cseq.h"
#include "usipy_sip_method_db.h"

#define USIPY_SIP_SEQ_MIN 0
#define USIPY_SIP_SEQ_MAX 0xffffffff

union usipy_sip_hdr_parsed
usipy_sip_hdr_cseq_parse(struct usipy_msg_heap *mhp,
  const struct usipy_str *hvp)
{
    struct usipy_str s1, s2;
    uint32_t r;
    union usipy_sip_hdr_parsed usp = {.cseq = NULL};

    if (usipy_str_splitlws(hvp, &s1, &s2) != 0) {
        return (usp);
    }
    if (usipy_str_atoui_range(&s1, &r, USIPY_SIP_SEQ_MIN, USIPY_SIP_SEQ_MAX) != 0) {
        return (usp);
    }
    usp.cseq = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_hdr_cseq));
    if (usp.cseq == NULL) {
        return (usp);
    }
    usp.cseq->val = r;
    usp.cseq->method = usipy_method_db_lookup(&s2);
    usp.cseq->onwire.method = s2;
    return (usp);
}

#define DUMP_UINT(sname) \
    ESP_LOGI(log_tag, "%s%s." #sname " = %u", log_pref, canname, csp->sname)
#define DUMP_METHOD(sname) \
    ESP_LOGI(log_tag, "%s%s." #sname " = \"%.*s\" (%d)", log_pref, canname, \
      USIPY_SFMT(&csp->sname->name), csp->sname->cantype)

void
usipy_sip_hdr_cseq_dump(const union usipy_sip_hdr_parsed *up, const char *log_tag,
  const char *log_pref, const char *canname)
{
    const struct usipy_sip_hdr_cseq *csp = up->cseq;

    DUMP_UINT(val);
    DUMP_METHOD(method);
}
