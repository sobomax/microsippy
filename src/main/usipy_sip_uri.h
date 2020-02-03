struct usipy_msg_heap;

struct usipy_sip_uri {
    struct usipy_str proto;
    struct usipy_str user;
    struct usipy_str password;
    struct usipy_str host;
    unsigned int port;
    struct usipy_str parameters;
    struct usipy_str headers;
};

struct usipy_sip_uri *usipy_sip_uri_parse(struct usipy_msg_heap *,
  const struct usipy_str *);

