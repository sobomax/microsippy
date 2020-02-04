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
    rp = hp->free;
    hp->free += alen;
    cntp->alen = alen;
    cntp->lastfree = hp->free;
    return (rp);
}

int
usipy_msg_heap_aextend(struct usipy_msg_heap *hp, size_t nlen
  struct usipy_msg_heap_cnt *cntp)
{
    size_t currfree, elen, alen;

    assert(cntp->lastfree == hp->free);
    alen = USIPY_ALIGNED_SIZE(nlen);
    assert(alen >= cntp->alen);
    elen = alen - cntp->alen;
    if (elen == 0) {
        return (0);
    }
    currfree = usipy_msg_heap_remaining(hp);
    if (currfree < elen) {
        return (-1);
    }
    hp->free += elen;
    cntp->alen = alen;
    cntp->lastfree = hp->free;
    return (0);
}
