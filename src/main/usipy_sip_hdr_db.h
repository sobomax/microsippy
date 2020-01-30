struct usipy_hdr_db_entr {
    struct usipy_str name;
    unsigned char cantype:7;
    /*
     * Set if multiple header fields of the same field name can be combined
     * into one comma-separated list for this type of header field.
     */
    unsigned char csl_allowed:1;
};

const struct usipy_hdr_db_entr *usipy_hdr_db_lookup(const struct usipy_str *);
const struct usipy_hdr_db_entr *usipy_hdr_db_byid(int);

#define usipy_hdr_iscmpct(hdep) ((hdep)->name.l == 1)
