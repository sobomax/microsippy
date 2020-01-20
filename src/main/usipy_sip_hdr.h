struct usipy_hdr_db_entr;
struct usipy_sip_hdr_cseq;

union usipy_sip_hdr_parsed {
    struct usipy_sip_hdr_cseq *cseq;
};

struct usipy_sip_hdr {
    struct {
        struct usipy_str full;
        struct usipy_str name;
        struct usipy_str value;
	const struct usipy_hdr_db_entr *hf_type;
    } onwire;
    const struct usipy_hdr_db_entr *hf_type;
    union usipy_sip_hdr_parsed parsed;
};

int usipy_sip_hdr_preparse(struct usipy_sip_hdr *);
