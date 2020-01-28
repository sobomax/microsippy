#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "usipy_str.h"
#include "usipy_fast_parser.h"

int
usipy_fp_classify(const struct usipy_fast_parser *fp, const struct usipy_str *sp)
{
    size_t i;
    unsigned char ch, res;

    res = 0;
    for (i = 0; i < sp->l; i++) {
        ch = sp->s.ro[i];
        if (ch > 127)
            return (-1);
        ch = fp->to5bit[ch];
        res = fp->pearson[res ^ ch];
    }
    return ((int)(fp->toid[res]));
}
