struct usipy_hdr_db_entr;

struct usipy_sip_hdr {
    struct {
        struct usipy_str full;
        struct usipy_str name;
        struct usipy_str value;
	const struct usipy_hdr_db_entr *hf_type;
    } onwire;
    const struct usipy_hdr_db_entr *hf_type;
};

int usipy_sip_hdr_preparse(struct usipy_sip_hdr *);
