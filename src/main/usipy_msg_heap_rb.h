#ifndef _USIPY_MSG_HEAP_RB_H
#define _USIPY_MSG_HEAP_RB_H

struct usipy_msg_heap_cnt {
    size_t alen;
    USIPY_DCODE(size_t lastalen);
};

void *usipy_msg_heap_alloc_cnt(struct usipy_msg_heap *, size_t,
  struct usipy_msg_heap_cnt *);
int usipy_msg_heap_aextend(struct usipy_msg_heap *, size_t,
  struct usipy_msg_heap_cnt *);

#endif /* _USIPY_MSG_HEAP_RB_H */
