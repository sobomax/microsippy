struct usipy_sip_tm_conf {
    uint16_t sip_port;
    int sip_af;
    const char *log_tag;
};

void usipy_sip_tm_task(void *pvParameters);
