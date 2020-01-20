struct usipy_method_db_entr {
    struct usipy_str name;
    int cantype;
};

const struct usipy_method_db_entr *usipy_method_db_lookup(const struct usipy_str *);
