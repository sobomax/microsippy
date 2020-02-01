#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include "usipy_msg_heap.h"

void *
usipy_msg_heap_alloc(struct usipy_msg_heap *hp, size_t len)
{
    size_t currfree, alen;
    void *rp;

    alen = USIPY_ALIGNED_SIZE(len);
    currfree = usipy_msg_heap_remaining(hp);
    if (currfree < len)
        return (NULL);
    rp = hp->free;
    hp->free += alen;
    hp->lastprov = rp;
    return (rp);
}

int
usipy_msg_heap_aextend(struct usipy_msg_heap *hp, void *origp, size_t len, int n)
{
    size_t currfree, elen;

    assert(origp == hp->lastprov);
    elen = USIPY_ALIGNED_SIZE(len * n) - USIPY_ALIGNED_SIZE(len * (n - 1));
    if (elen == 0) {
        return (0);
    }
    currfree = usipy_msg_heap_remaining(hp);
    if (currfree < elen)
        return (-1);
    }
    hp->free += elen;
    return (0);
}
