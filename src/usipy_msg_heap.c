#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "usipy_debug.h"
#include "public/usipy_msg_heap.h"
#include "public/usipy_str.h"
#include "usipy_msg_heap_rb.h"

void *
usipy_msg_heap_alloc(struct usipy_msg_heap *hp, size_t len)
{
    size_t currfree, alen;
    void *rp;

    alen = USIPY_ALIGNED_SIZE(len);
    currfree = usipy_msg_heap_remaining(hp);
    if (currfree < alen)
        return (NULL);
    rp = hp->first + hp->alen;
    hp->alen += alen;
    return (rp);
}

void
usipy_msg_heap_init(struct usipy_msg_heap *hp, void *bp, size_t len, size_t *checkpoints,
  size_t ncheckpoints)
{
    uintptr_t ralgn;

    hp->alen = 0;
    hp->first = bp;
    hp->checkpoints = checkpoints;
    hp->ncheckpoints = ncheckpoints;
    hp->checkpoint_top = 0;
    ralgn = USIPY_REALIGN((uintptr_t)hp->first);
    if ((void *)ralgn != hp->first) {
        hp->first = (void *)(ralgn + (1 << USIPY_MEM_ALIGNOF));
    }
    hp->tsize = USIPY_REALIGN(len - (hp->first - bp));
}

size_t
usipy_msg_heap_checkpoint(struct usipy_msg_heap *hp)
{
    size_t cpi;

    USIPY_DASSERT(hp != NULL);
    USIPY_DASSERT(hp->checkpoint_top < hp->ncheckpoints);
    USIPY_DASSERT(hp->checkpoints != NULL);
    cpi = hp->checkpoint_top;
    hp->checkpoints[cpi] = hp->alen;
    hp->checkpoint_top += 1;
    return (cpi);
}

void
usipy_msg_heap_rollback(struct usipy_msg_heap *hp, size_t checkpoint_index)
{
    size_t alen;

    USIPY_DASSERT(hp != NULL);
    USIPY_DASSERT(hp->checkpoint_top > 0);
    USIPY_DASSERT(hp->checkpoints != NULL);
    USIPY_DASSERT(checkpoint_index != USIPY_MSG_HEAP_CHECKPOINT_NONE);
    USIPY_DASSERT(checkpoint_index == hp->checkpoint_top - 1);
    alen = hp->checkpoints[checkpoint_index];
    USIPY_DASSERT(alen <= hp->alen);
    memset((char *)hp->first + alen, '\0', hp->alen - alen);
    hp->alen = alen;
}

int
usipy_msg_heap_build(struct usipy_msg_heap *hp, struct usipy_str *sp, void *arg,
  usipy_msg_heap_build_cb cb)
{
    const size_t currfree = usipy_msg_heap_remaining(hp);
    char *bp;
    size_t alen;
    int blen;

    USIPY_DASSERT(hp != NULL);
    USIPY_DASSERT(sp != NULL);
    USIPY_DASSERT(cb != NULL);

    if (currfree == 0) {
        return (-1);
    }
    bp = (char *)hp->first + hp->alen;
    blen = cb(arg, bp, currfree);
    if (blen < 0) {
        return (-1);
    }
    alen = USIPY_ALIGNED_SIZE((size_t)blen);
    if (alen > currfree) {
        return (-1);
    }
    hp->alen += alen;
    sp->s.ro = bp;
    sp->l = (size_t)blen;
    return (0);
}

static int
usipy_msg_heap_vsprintf(struct usipy_msg_heap *hp, struct usipy_str *sp,
  const char *fmt, va_list ap)
{
    const size_t currfree = usipy_msg_heap_remaining(hp);
    const char *bp;
    size_t alen;
    int plen;

    USIPY_DASSERT(hp != NULL);
    USIPY_DASSERT(sp != NULL);
    USIPY_DASSERT(fmt != NULL);

    if (currfree == 0) {
        return (-1);
    }
    bp = (char *)hp->first + hp->alen;
    plen = vsnprintf((char *)bp, currfree, fmt, ap);
    if (plen < 0 || (size_t)plen >= currfree) {
        return (-1);
    }
    alen = USIPY_ALIGNED_SIZE((size_t)plen + 1);
    if (alen > currfree) {
        return (-1);
    }
    hp->alen += alen;
    sp->s.ro = bp;
    sp->l = (size_t)plen;
    return (0);
}

int
usipy_msg_heap_sprintf(struct usipy_msg_heap *hp, struct usipy_str *sp,
  const char *fmt, ...)
{
    va_list ap;
    int rval;

    va_start(ap, fmt);
    rval = usipy_msg_heap_vsprintf(hp, sp, fmt, ap);
    va_end(ap);
    return (rval);
}
