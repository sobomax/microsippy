#include <stdlib.h>

#include "usipy_types.h"
#include "public/usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_onetoken.h"

union usipy_sip_hdr_parsed
usipy_sip_hdr_1token_parse(struct usipy_msg_heap *hp, const struct usipy_str *hvp)
{
    union usipy_sip_hdr_parsed ru;

    ru.generic = hvp;
    return (ru);
}
