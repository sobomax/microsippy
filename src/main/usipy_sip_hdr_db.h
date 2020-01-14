struct usipy_hdr_db_entr {
    struct usipy_str name;
    int cantype;
};

const struct usipy_hdr_db_entr *usipy_hdr_db_lookup(const struct usipy_str *);
