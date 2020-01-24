struct usipy_msg_heap {
    void *first;
    size_t size;
    void *free;
};

void *usipy_msg_heap_alloc(struct usipy_msg_heap *, size_t);

#define USIPY_MEM_ALIGNOF  (3) /* alignof(max_align_t) ? */
#define USIPY_REALIGN(val) ((val) & ~((1 << USIPY_MEM_ALIGNOF) - 1))
#define USIPY_ALIGNED_SIZE(val) ( \
  (val & ~((1 << USIPY_MEM_ALIGNOF) - 1)) + \
  ((val & ((1 << USIPY_MEM_ALIGNOF) - 1)) != 0 ? (1 << USIPY_MEM_ALIGNOF) : 0) \
)

#define usipy_msg_heap_remaining(hp) \
  ((hp)->size - ((hp)->free - (hp)->first))
