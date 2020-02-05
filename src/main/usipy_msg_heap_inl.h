#ifndef _USIPY_MSG_HEAP_INL_H
#define _USIPY_MSG_HEAP_INL_H

inline void
usipy_msg_heap_rollback(struct usipy_msg_heap *hp, const struct usipy_msg_heap_cnt *cntp)
{

    USIPY_DASSERT(cntp->lastalen == hp->alen);
    USIPY_DASSERT(cntp->alen <= hp->alen);
    hp->alen -= cntp->alen;
    memset(hp->first + hp->alen, '\0', cntp->alen);
}

#endif /* _USIPY_MSG_HEAP_INL_H */
