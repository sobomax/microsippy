struct usipy_msg_heap;

struct usipy_sip_uri {
    struct usipy_str proto;
    struct usipy_str user;
    struct usipy_str password;
    struct usipy_str host;
    unsigned int port;
    int nparams;
    struct usipy_tvpair *parameters;
    struct usipy_str headers;
    struct usipy_tvpair params[0];
};

struct usipy_sip_uri *usipy_sip_uri_parse(struct usipy_msg_heap *,
  const struct usipy_str *);
void usipy_sip_uri_dump(const struct usipy_sip_uri *, const char *,
  const char *);
