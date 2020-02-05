#include <errno.h>
#include <stddef.h>

#include "usipy_debug.h"
#include "usipy_msg_heap.h"
#include "usipy_msg_heap_rb.h"

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
