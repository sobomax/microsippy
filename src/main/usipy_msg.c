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
    struct usipy_str cp;

    alloc_len = sizeof(struct usipy_msg) + (len * 2);
    rp = malloc(alloc_len);
    if (rp == NULL) {
        goto e0;
    }
    memset(rp, '\0', alloc_len);
    memcpy(rp->_storage, buf, len);
    rp->onwire.s.rw = rp->_storage;
    rp->onwire.l = len;
    rp->heap.free = rp->heap.first = rp->_storage + len;
    rp->heap.size = alloc_len - offsetof(struct usipy_msg, _storage);
    for (struct usipy_str cp = rp->onwire; cp.l > 0;) {
        const char *chp = strnstr(cp.s.ro, "\r\n", cp.l);
        if (chp == NULL)
            break;
        struct usipy_sip_hdr *shp = usipy_msg_heap_alloc(&rp->heap,
          sizeof(struct usipy_sip_hdr));
        if (shp == NULL)
            goto e1;
        if (rp->nhdrs == 0)
            rp->hdrs = shp;
        rp->nhdrs += 1;
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
