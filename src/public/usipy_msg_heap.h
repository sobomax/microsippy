#ifndef _USIPY_MSG_HEAP_H
#define _USIPY_MSG_HEAP_H

struct usipy_msg_heap {
    size_t tsize;
    size_t alen;
    void *first;
};

void *usipy_msg_heap_alloc(struct usipy_msg_heap *, size_t);
void usipy_msg_heap_init(struct usipy_msg_heap *, void *, size_t);

#define USIPY_MEM_ALIGNOF  (3) /* alignof(max_align_t) ? */
#define USIPY_REALIGN(val) ((val) & ~((1 << USIPY_MEM_ALIGNOF) - 1))
#define USIPY_ALIGNED_SIZE(val) ( \
  (val & ~((1 << USIPY_MEM_ALIGNOF) - 1)) + \
  ((val & ((1 << USIPY_MEM_ALIGNOF) - 1)) != 0 ? (1 << USIPY_MEM_ALIGNOF) : 0) \
)

#define usipy_msg_heap_remaining(hp) \
  ((hp)->tsize - (hp)->alen)

#endif /* _USIPY_MSG_HEAP_H */
