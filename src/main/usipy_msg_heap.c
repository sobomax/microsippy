#include <errno.h>
#include <stddef.h>

#include "usipy_msg_heap.h"

void *
usipy_msg_heap_alloc(struct usipy_msg_heap *hp, size_t len)
{
    size_t currfree, alen;
    void *rp;

    alen = USIPY_REALIGN(len);
    if (alen != len) {
        len = alen + (1 << USIPY_MEM_ALIGNOF);
    }
    currfree = usipy_msg_heap_remaining(hp);
    if (currfree < len)
        return (NULL);
    rp = hp->free;
    hp->free += len;
    return (rp);
}