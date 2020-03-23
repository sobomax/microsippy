#include <stdint.h>
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

void
usipy_msg_heap_init(struct usipy_msg_heap *hp, void *bp, size_t len)
{
    uintptr_t ralgn;

    hp->first = bp;
    ralgn = USIPY_REALIGN((uintptr_t)hp->first);
    if ((void *)ralgn != hp->first) {
        hp->first = (void *)(ralgn + (1 << USIPY_MEM_ALIGNOF));
    }
    hp->tsize = USIPY_REALIGN(len - (hp->first - bp));
}
