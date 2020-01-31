struct usipy_msg_heap;
union usipy_sip_hdr_parsed;

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
};

struct usipy_sip_hdr_via *usipy_sip_hdr_via_parse(struct usipy_msg_heap *,
  const struct usipy_str *);
void usipy_sip_hdr_via_dump(const char *, const union usipy_sip_hdr_parsed *);
