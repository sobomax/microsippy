#include <string.h>

int usipy_str_split(const struct usipy_str *x, unsigned char dlm,
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
