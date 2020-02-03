struct usipy_msg_heap;
union usipy_sip_hdr_parsed;

struct usipy_sip_param {
    struct usipy_str token;
    struct usipy_str value;
};

struct usipy_sip_hdr_via {
    struct {
        struct usipy_str name;      /* "SIP" / token */
        struct usipy_str version;   /* token */
        struct usipy_str transport; /* "UDP" / "TCP" / "TLS" / "SCTP" / other-transport */
    }  sent_protocol;
    struct {
        struct usipy_str host;      /* host [ COLON port ] */
        unsigned int port;
    } sent_by;
    int nparams;
    struct usipy_sip_param params[0];
};

struct usipy_sip_hdr_via *usipy_sip_hdr_via_parse(struct usipy_msg_heap *,
  const struct usipy_str *);
void usipy_sip_hdr_via_dump(const union usipy_sip_hdr_parsed *, const char *,
  const char *);
