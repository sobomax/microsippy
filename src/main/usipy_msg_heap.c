#include <errno.h>
#include <stddef.h>

#include "usipy_debug.h"
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
    rp = hp->first + hp->alen;
    hp->alen += alen;
    return (rp);
}

void *
usipy_msg_heap_alloc_cnt(struct usipy_msg_heap *hp, size_t len,
  struct usipy_msg_heap_cnt *cntp)
{
    void *rp;
    size_t currfree, alen;

    alen = USIPY_ALIGNED_SIZE(len);
    currfree = usipy_msg_heap_remaining(hp);
    if (currfree < len)
       return (NULL);
    rp = hp->first + hp->alen;
    hp->alen += alen;
    cntp->alen = alen;
    USIPY_DCODE(cntp->lastalen = hp->alen);
    return (rp);
}

int
usipy_msg_heap_aextend(struct usipy_msg_heap *hp, size_t nlen,
  struct usipy_msg_heap_cnt *cntp)
{
    size_t currfree, elen, alen;

    USIPY_DASSERT(cntp->lastalen == hp->alen);
    alen = USIPY_ALIGNED_SIZE(nlen);
    USIPY_DASSERT(alen >= cntp->alen);
    elen = alen - cntp->alen;
    if (elen == 0) {
        return (0);
    }
    currfree = usipy_msg_heap_remaining(hp);
    if (currfree < elen) {
        return (-1);
    }
    hp->alen += elen;
    cntp->alen = alen;
    USIPY_DCODE(cntp->lastalen = hp->alen);
    return (0);
}
