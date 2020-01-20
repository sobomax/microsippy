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

int
usipy_str_splitlws(const struct usipy_str *ivp, struct usipy_str *ovp1,
  struct usipy_str *ovp2)
{
    const char *cp;

    for (cp = ivp->s.ro; cp < (ivp->s.ro + ivp->l); cp++) {
        if (USIPY_ISLWS(*cp))
            goto lws_found;
    }
    return (-1);
lws_found:
    ovp1->s.ro = ivp->s.ro;
    ovp1->l = cp - ivp->s.ro;
    ovp2->s.ro = cp;
    ovp2->l = ivp->l - ovp1->l - 1;
    usipy_str_ltrm_b(ovp2);
    return (0);
}
