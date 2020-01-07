#include <stddef.h>
#include <stdlib.h>

#include "usipy_msg.h"

struct usipy_msg *
usipy_msg_ctor_fromwire(const char *buf, size_t len, int *err)
{
    struct usipy_msg *rp;
    size_t heap_len;

    heap_len = sizeof(struct usipy_msg) + (len * 2);
    rp = malloc(heap_len);
    if (rp == NULL) {
        if (err != NULL)
            *err = ENOMEM;
        return (NULL);
    }
    memset(rp, '\0', heap_len);
    memcpy(rp->_storage, buf, len);
    rp->onwire.rw.s = rp->_storage;
    rp->onwire.len = len;
    rp->heap.free = rp->heap.first = rp->_storage + len;
    rp->heap.size = heap_len - offsetof(struct usipy_msg, _storage);
    return (rp);
}

void
usipy_msg_dtor(struct usipy_msg *msg)
{

    free(msg);
}
