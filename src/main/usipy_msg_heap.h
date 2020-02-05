#ifndef _USIPY_MSG_HEAP_H
#define _USIPY_MSG_HEAP_H

struct usipy_msg_heap {
    size_t tsize;
    size_t alen;
    void *first;
};

struct usipy_msg_heap_cnt {
    size_t alen;
    USIPY_DCODE(size_t lastalen);
};

void *usipy_msg_heap_alloc(struct usipy_msg_heap *, size_t);
void *usipy_msg_heap_alloc_cnt(struct usipy_msg_heap *, size_t,
  struct usipy_msg_heap_cnt *);
int usipy_msg_heap_aextend(struct usipy_msg_heap *, size_t,
  struct usipy_msg_heap_cnt *);

#define USIPY_MEM_ALIGNOF  (3) /* alignof(max_align_t) ? */
#define USIPY_REALIGN(val) ((val) & ~((1 << USIPY_MEM_ALIGNOF) - 1))
#define USIPY_ALIGNED_SIZE(val) ( \
  (val & ~((1 << USIPY_MEM_ALIGNOF) - 1)) + \
  ((val & ((1 << USIPY_MEM_ALIGNOF) - 1)) != 0 ? (1 << USIPY_MEM_ALIGNOF) : 0) \
)

#define usipy_msg_heap_remaining(hp) \
  ((hp)->tsize - (hp)->alen)

inline void
usipy_msg_heap_rollback(struct usipy_msg_heap *hp, struct usipy_msg_heap_cnt *cntp)
{

    USIPY_DASSERT(cntp->lastalen == hp->alen);
    USIPY_DASSERT(cntp->alen <= hp->alen);
    hp->alen -= cntp->alen;
    memset(hp->first + hp->alen, '\0', cntp->alen);
}

#endif /* _USIPY_MSG_HEAP_H */
