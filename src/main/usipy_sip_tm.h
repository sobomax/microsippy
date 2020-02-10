struct usipy_sip_tm_conf {
    uint16_t sip_port;
    int sip_af;
    const char *log_tag;
};

void usipy_sip_tm_task(void *pvParameters);

#define USIPY_HF_TID_MASK ( \
  USIPY_HFT_MASK(USIPY_HF_CSEQ) | \
  USIPY_HFT_MASK(USIPY_HF_CALLID) | \
  USIPY_HFT_MASK(USIPY_HF_VIA) | \
  USIPY_HFT_MASK(USIPY_HF_FROM)
)
