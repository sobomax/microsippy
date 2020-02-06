struct usipy_msg_heap;
struct usipy_method_db_entr;

struct usipy_sip_hdr_cseq {
    uint32_t val;
    const struct usipy_method_db_entr *method;
};

union usipy_sip_hdr_parsed usipy_sip_hdr_cseq_parse(struct usipy_msg_heap *,
  const struct usipy_str *);
void usipy_sip_hdr_cseq_dump(const union usipy_sip_hdr_parsed *, const char *,
  const char *);
