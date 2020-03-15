struct usipy_msg;
struct usipy_sip_status;

struct usipy_msg *usipy_sip_res_ctor_fromreq(const struct usipy_msg *,
  const struct usipy_sip_status *);
