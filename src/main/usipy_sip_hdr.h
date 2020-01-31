struct usipy_hdr_db_entr;
struct usipy_sip_hdr_cseq;
struct usipy_sip_hdr_via;

union usipy_sip_hdr_parsed {
    struct usipy_sip_hdr_cseq *via;
    struct usipy_sip_hdr_cseq *cseq;
    const struct usipy_str *generic;
};

struct usipy_sip_hdr {
    struct {
        struct usipy_str full;
        struct usipy_str name;
        struct usipy_str value;
	const struct usipy_hdr_db_entr *hf_type;
    } onwire;
    const struct usipy_hdr_db_entr *hf_type;
#if 0
    const char *col_offst;
#endif
    union usipy_sip_hdr_parsed parsed;
};

int usipy_sip_hdr_preparse(struct usipy_sip_hdr *, struct usipy_sip_hdr *);
