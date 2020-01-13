#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "usipy_str.h"
#include "usipy_msg_heap.h"
#include "usipy_sip_sline.h"
#include "usipy_sip_msg.h"
#include "usipy_sip_hdr.h"

#define USIPY_CRLF     "\r\n"
#define USIPY_CRLF_LEN 2
#define USIPY_ISWS(ch) ((ch) == ' ' || (ch) == '\t')
#define USIPY_ISLWS(ch) (USIPY_ISWS(ch) || (ch) == '\r' || (ch) == '\n')

static int
header_parse(struct usipy_sip_hdr *shp)
{

     if (usipy_str_split(&shp->onwire.full, ':', &shp->onwire.name,
       &shp->onwire.value) != 0)
         return (-1);
     usipy_str_trm_e(&shp->onwire.name);
     usipy_str_ltrm_b(&shp->onwire.value);
     usipy_str_ltrm_e(&shp->onwire.value);
     return (0);
}

struct usipy_msg *
usipy_sip_msg_ctor_fromwire(const char *buf, size_t len, int *err)
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
    ralgn = USIPY_REALIGN((uintptr_t)rp->heap.first);
    if ((void *)ralgn != rp->heap.first) {
        rp->heap.first = (void *)(ralgn + (1 << USIPY_MEM_ALIGNOF));
    }
    rp->heap.free = rp->heap.first;
    rp->heap.size = USIPY_REALIGN(alloc_len - (rp->heap.first - (void *)rp));
    int nempty = 0;
    struct usipy_sip_hdr *shp = NULL;
    for (struct usipy_str cp = rp->onwire; cp.l > 0;) {
        const char *chp = memmem(cp.s.ro, cp.l, USIPY_CRLF, USIPY_CRLF_LEN);
        if (chp == NULL)
            break;
        if (rp->nhdrs > 0) {
            if (chp == cp.s.ro) {
                /* End of headers reached */
                if (nempty > 0)
                    break;
                nempty += 1;
                continue;
            }
            if (USIPY_ISWS(cp.s.ro[0])) {
                /* Continuation */
                goto multi_line;
            }
        } else if (rp->sline.onwire.l == 0) {
            rp->sline.onwire.s.ro = cp.s.ro;
            rp->sline.onwire.l = chp - cp.s.ro;
            goto next_line;
        }
        if (shp != NULL) {
            if (header_parse(shp) != 0)
                goto e1;
        }
        shp = usipy_msg_heap_alloc(&rp->heap, sizeof(struct usipy_sip_hdr));
        if (shp == NULL)
            goto e1;
        if (rp->nhdrs == 0)
            rp->hdrs = shp;
        rp->nhdrs += 1;
        shp->onwire.full.s.ro = cp.s.ro;
multi_line:
        shp->onwire.full.l = chp - shp->onwire.full.s.ro;
next_line:
        chp += 2;
        cp.l -= chp - cp.s.ro;
        cp.s.ro = chp;
    }
    if (shp != NULL) {
        if (header_parse(shp) != 0)
            goto e1;
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
usipy_sip_msg_dtor(struct usipy_msg *msg)
{

    free(msg);
}
