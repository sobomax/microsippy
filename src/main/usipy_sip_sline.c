#include <stdlib.h>

#include "usipy_str.h"
#include "usipy_sip_sline.h"

int
usipy_sip_sline_parse(struct usipy_sip_sline *slp)
{
    struct usipy_str s1, s2, s3, s4;

    if (usipy_str_split(sp->onwire, USIPY_SP, &s1, &s2) != 0)
        return (-1);
    slp->parsed.sl.version = s1;
    if (usipy_str_split(&s2, USIPY_SP, &s3, &s4) != 0)
        return (-1);
    slp->parsed.sl.reason_phrase = s4;
    return (0);
}
