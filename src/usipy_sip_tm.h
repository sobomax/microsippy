DEFINE_RAW_METHOD(usipy_sip_tm_faterr, void, void *);

struct usipy_sip_tm_conf {
    uint16_t sip_port;
    int sip_af;
    const char *log_tag;
    usipy_sip_tm_faterr_f faterr;
    void *faterr_arg;
};

void usipy_sip_tm_task(void *);
