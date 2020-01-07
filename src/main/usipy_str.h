struct usipy_str {
    union {
        const char *ro;
        char *rw;
    } s;
    size_t l;
};

struct usipy_str_ro {
    union {
        const char *ro;
    } s;
    size_t l;
};
