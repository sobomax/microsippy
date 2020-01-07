#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "usipy_str.h"
#include "usipy_msg_heap.h"
#include "usipy_msg.h"
#include "usipy_sip_hdr.h"

struct usipy_msg *
usipy_msg_ctor_fromwire(const char *buf, size_t len, int *err)
{
    struct usipy_msg *rp;
    size_t alloc_len;
    uintptr_t ralgn;

    alloc_len = sizeof(struct usipy_msg) + (len * 2);
    rp = malloc(alloc_len);
    if (rp == NULL) {
        goto e0;
    }
    memset(rp, '\0', alloc_len);
    memcpy(rp->_storage, buf, len);
    rp->onwire.s.rw = rp->_storage;
    rp->onwire.l = len;
    rp->heap.first = rp->_storage + len;
    ralgn = (uintptr_t)rp->heap.first & ~((1 << USIPY_MEM_ALIGNOF) - 1);
    if ((void *)ralgn != rp->heap.first) {
        rp->heap.first = (void *)(ralgn + (1 << USIPY_MEM_ALIGNOF));
    }
    rp->heap.free = rp->heap.first;
    rp->heap.size = alloc_len - (rp->heap.first - (void *)rp);
    for (struct usipy_str cp = rp->onwire; cp.l > 0;) {
        const char *chp = memmem(cp.s.ro, cp.l, "\r\n", 2);
        if (chp == NULL)
            break;
        struct usipy_sip_hdr *shp = usipy_msg_heap_alloc(&rp->heap,
          sizeof(struct usipy_sip_hdr));
        if (shp == NULL)
            goto e1;
        shp->onwire.s.ro = cp.s.ro;
        shp->onwire.l = chp - cp.s.ro;
        if (rp->nhdrs == 0)
            rp->hdrs = shp;
        rp->nhdrs += 1;
        chp += 2;
        cp.l -= chp - cp.s.ro;
        cp.s.ro = chp;
    }
    return (rp);
e1:
    free(rp);
e0:
    if (err != NULL)
        *err = ENOMEM;
    return (NULL);
}

void
usipy_msg_dtor(struct usipy_msg *msg)
{

    free(msg);
}
