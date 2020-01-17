#include <string.h>

#include "usipy_str.h"

int
usipy_str_split(const struct usipy_str *x, unsigned char dlm,
  struct usipy_str *y, struct usipy_str *z)
{
    const char *r;
    r = memchr(x->s.ro, dlm, x->l);
    if (r == NULL)
        return (-1);
    y->s.ro = x->s.ro;
    y->l = r - x->s.ro;
    z->l = x->l - y->l - 1;
    if (z->l > 0) {
        z->s.ro = r + 1;
    } else {
        z->s.ro = NULL;
    }

    return (0);
}

int
usipy_str_atoui_range(const struct usipy_str *x, unsigned int *res,
  unsigned int min, unsigned int max)
{
    int r = 0;
    const char *cp;

    for (cp = x->s.ro; cp < (x->s.ro + x->l); cp++) {
        if (*cp > '9' || *cp < '0')
            return -1;
        r *= 10;
        r += (unsigned char)(*cp - '0');
    }
    if (r < min || (max >= min && r > max)) {
        return -1;
    }
    *res = r;
    return (0);
}
