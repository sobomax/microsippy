#include <stdint.h>
#include <stdlib.h>

#include "usipy_msg_heap.h"
#include "usipy_str.h"
#include "usipy_sip_hdr_cseq.h"
#include "usipy_sip_method_db.h"

#define USIPY_SIP_SEQ_MIN 0
#define USIPY_SIP_SEQ_MAX 0xffffffff

struct usipy_sip_hdr_cseq *
usipy_sip_hdr_cseq_parse(struct usipy_msg_heap *mhp,
  const struct usipy_str *hvp)
{
    struct usipy_str s1, s2;
    uint32_t r;
    struct usipy_sip_hdr_cseq *csp;

    if (usipy_str_splitlws(hvp, &s1, &s2) != 0) {
        return (NULL);
    }
    if (usipy_str_atoui_range(&s1, &r, USIPY_SIP_SEQ_MIN, USIPY_SIP_SEQ_MAX) != 0) {
        return (NULL);
    }
    csp = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_hdr_cseq));
    if (csp == NULL) {
        return (NULL);
    }
    csp->val = r;
    csp->method = usipy_method_db_lookup(&s2)->cantype;
    return (csp);
}
