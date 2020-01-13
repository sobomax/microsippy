struct usipy_sip_hdr {
    struct {
        struct usipy_str full;
        struct usipy_str name;
        struct usipy_str value;
    } onwire;
};

static int usipy_sip_hdr_preparse(struct usipy_sip_hdr *);
