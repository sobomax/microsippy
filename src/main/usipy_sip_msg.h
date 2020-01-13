struct usipy_sip_hdr;

enum usipy_sip_msg_kind {SIP_REQ, SIP_RES};

struct usipy_msg {
   struct usipy_str onwire;
   enum usipy_sip_msg_kind kind;
   struct usipy_sip_sline sline;
   struct usipy_sip_hdr *hdrs;
   unsigned int nhdrs;
   struct usipy_msg_heap heap;
   struct {
       uint64_t present;
       uint64_t parsed;
   } hdr_masks;
   char _storage[0];
};

struct usipy_msg *usipy_sip_msg_ctor_fromwire(const char *, size_t, int *);
void usipy_sip_msg_dtor(struct usipy_msg *);
