struct usipy_sip_hdr;

enum usipy_msg_kind {SIP_REQ, SIP_RES};

struct usipy_msg {
   enum usipy_msg_kind kind;
   struct usipy_str onwire;
   struct usipy_sip_hdr *hdrs;
   struct usipy_msg_heap heap;
   struct {
       uint64_t present;
       uint64_t parsed;
   } hdr_masks;
   char _storage[0];
};

struct usipy_msg *usipy_msg_ctor_fromwire(const char *, size_t, int *);
void usipy_msg_dtor(struct usipy_msg *);
