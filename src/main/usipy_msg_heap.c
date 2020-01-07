#include <errno.h>
#include <stddef.h>

#include "usipy_msg_heap.h"

void *
usipy_msg_heap_alloc(struct usipy_msg_heap *hp, size_t len)
{
    size_t currfree, alen;
    void *rp;

    alen = len & ~((1 << USIPY_MEM_ALIGNOF) - 1);
    if (alen != len) {
        len = alen + (1 << USIPY_MEM_ALIGNOF);
    }
    currfree = hp->size - (hp->free - hp->first);
    if (currfree < len)
        return (NULL);
    rp = hp->free;
    hp->free += len;
    return (rp);
}
