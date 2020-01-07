#include <errno.h>
#include <stddef.h>

#include "usipy_msg_heap.h"

void *
usipy_msg_heap_alloc(struct usipy_msg_heap *hp, size_t len)
{
    size_t currfree;
    void *rp;

    currfree = hp->size - (hp->free - hp->first);
    if (currfree < len)
        return (NULL);
    rp = hp->free;
    hp->free += len;
    return (rp);
}
