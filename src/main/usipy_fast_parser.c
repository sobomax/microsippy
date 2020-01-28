#include <stdint.h>
#include <stdlib.h>

#include "usipy_str.h"
#include "usipy_fast_parser.h"

struct usipy_str_iter {
    const struct usipy_str *sp;
    uint32_t store;
    int i;
    char loaded;
};

static int
usipy_str_iter_next(struct usipy_str_iter *ip)
{
    int rval;
    int remain;

    remain = ip->sp->l - ip->i;
    if (remain == 0)
       return (-1);
    if (ip->i == 0 || remain < sizeof(ip->store)) {
        typeof(ip->store) load = sizeof(ip->store) - ((typeof(ip->store))(ip->sp->s.ro + ip->i) & (sizeof(ip->store) - 1));
        if (load != sizeof(ip->store)) {
            typeof(ip->store) nstore = 0;
            for (int j = 0; j < load; j++) {
                nstore |= (typeof(ip->store))ip->sp->s.ro[ip->i + j] << (8 * j);
            }
            ip->store = nstore;
            ip->loaded = load - 1;
            goto done;
        }
        goto loadwhole;
    }
    if (ip->loaded == 0) {
loadwhole:
        memcpy(&ip->store, ip->sp->s.ro + ip->i, sizeof(ip->store));
        ip->loaded = sizeof(ip->store) - 1;
    } else {
        ip->loaded -= 1;
    }
done:
    rval = (ip->store & 0xff);
    ip->store >>= 8;
    ip->i += 1;
    return (rval);
}

int
usipy_fp_classify(const struct usipy_fast_parser *fp, const struct usipy_str *sp)
{
    size_t i;
    unsigned char ch, res;
    struct usipy_str_iter sip = {.sp = &sp, .store = 0, .i = 0, .loaded = 0};

    res = 0;
    while ((ch = usipy_str_iter_next(&sip)) != -1) {
        if (ch > 127)
            return (-1);
        ch = fp->to5bit[ch];
        res = fp->pearson[res ^ ch];
    }
    return ((int)(fp->toid[res]));
}
