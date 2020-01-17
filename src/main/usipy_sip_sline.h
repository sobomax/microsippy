struct usipy_sip_status_line {
    struct usipy_str version;
    unsigned int status_code;
    struct usipy_str reason_phrase;
};

struct usipy_sip_request_line {
    struct usipy_str method;
    struct usipy_str ruri;
    struct usipy_str version;
};

struct usipy_sip_sline {
    struct usipy_str onwire;
    union {
        struct usipy_sip_status_line sl;
        struct usipy_sip_request_line rl;
    } parsed;
};

int usipy_sip_sline_parse(struct usipy_sip_sline *);
