struct usipy_msg_heap;
struct usipy_msg;
struct usipy_sip_status;
struct usipy_str;

struct usipy_msg *usipy_sip_res_ctor_fromreq(const struct usipy_msg *,
  const struct usipy_sip_status *);
struct usipy_msg *usipy_sip_res_build_fromreq_tagged(struct usipy_msg_heap *,
  const struct usipy_msg *, const struct usipy_sip_status *,
  const struct usipy_str *);
