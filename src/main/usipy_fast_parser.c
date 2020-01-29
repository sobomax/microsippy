#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "usipy_str.h"
#include "usipy_fast_parser.h"

int
usipy_fp_classify(const struct usipy_fast_parser *fp, const struct usipy_str *sp)
{
    unsigned char ch;
    uint32_t cval, res;

    res = 0;
    for (int i = 0; i < sp->l; i += sizeof(cval)) {
        int remain = sp->l - i;
        if (remain < sizeof(cval)) {
            cval = 0;
            for (int j = 0; j < remain; j++) {
                cval |= sp->s.ro[i + j] << (j * 8);
            }
        } else {
            memcpy(&cval, sp->s.ro + i, sizeof(cval));
        }
        /* Convert to lower case */
        cval |= 0x20202020;
        /* Apply Magick */
        cval ^= fp->magic;
        res ^= cval;
    }
    if (sp->l > 1)
        res = ((res >> (sp->l - 1)) ^ sp->l);
    return (fp->toid[res & 0xff]);
}
