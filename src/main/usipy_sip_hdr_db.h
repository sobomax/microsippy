struct usipy_hdr_db_entr {
    struct usipy_str name;
    int cantype;
};

const struct usipy_hdr_db_entr *usipy_hdr_db_lookup(const struct usipy_str *);
const struct usipy_hdr_db_entr *usipy_hdr_db_byid(int);

#define usipy_hdr_iscmpct(hdep) ((hdep)->name.l == 1)
