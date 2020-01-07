enum usipy_msg_kind {SIP_REQ, SIP_RES};

struct usipy_msg {
   enum usipy_msg_kind kind;
   struct usipy_str onwire;
};

struct usipy_msg *usipy_msg_ctor_fromwire(struct usipy_str_ro *wirebuf);
void usipy_msg_dtor(struct usipy_msg *);
