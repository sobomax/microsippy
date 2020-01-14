struct usipy_fast_parser {
    unsigned char to5bit[128];
    unsigned char pearson[64];
    char toid[64];
};

int usipy_fp_classify(const struct usipy_fast_parser *, const struct usipy_str *);
