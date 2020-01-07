struct usipy_msg_heap {
    void *first;
    size_t size;
    void *free;
};

void *usipy_msg_heap_alloc(struct usipy_msg_heap *, size_t);
