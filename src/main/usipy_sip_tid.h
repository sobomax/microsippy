struct usipy_str;
struct usipy_sip_hdr_cseq;

struct usipy_sip_tid {
    const struct usipy_str *call_id;
    const struct usipy_str *from_tag;
    const struct usipy_sip_hdr_cseq *cseq;
    const struct usipy_str *vbranch;
};

#define USIPY_HF_TID_MASK ( \
  USIPY_HFT_MASK(USIPY_HF_CSEQ) | \
  USIPY_HFT_MASK(USIPY_HF_CALLID) | \
  USIPY_HFT_MASK(USIPY_HF_VIA) | \
  USIPY_HFT_MASK(USIPY_HF_FROM) \
)
