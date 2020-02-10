#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"

#include "usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_cseq.h"
#include "usipy_sip_tid.h"

#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "%s" #sname " = \"%.*s\"", log_pref, \
      USIPY_SFMT(tp->sname))

void
usipy_sip_tid_dump(const struct usipy_sip_tid *tp, const char *log_tag,
  const char *log_pref)
{
    const union usipy_sip_hdr_parsed up = {.cseq = tp->cseq};

    DUMP_STR(call_id);
    DUMP_STR(from_tag);
    DUMP_STR(vbranch);
    usipy_sip_hdr_cseq_dump(&up, log_tag, log_pref, "");
}

