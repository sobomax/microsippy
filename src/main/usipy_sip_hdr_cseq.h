struct usipy_msg_heap;

struct usipy_sip_hdr_cseq {
    uint32_t val;
    int method;
};

struct usipy_sip_hdr_cseq *usipy_sip_hdr_cseq_parse(struct usipy_msg_heap *,
  const struct usipy_str *);
