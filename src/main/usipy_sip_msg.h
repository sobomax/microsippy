struct usipy_sip_hdr;

enum usipy_sip_msg_kind {
  USIPY_SIP_MSG_UNKN = -1,
  USIPY_SIP_MSG_REQ = 0,
  USIPY_SIP_MSG_RES = 1
};

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

struct usipy_codeptr {
    const char *fname;
    int linen;
    const char *funcn;
};

struct usipy_msg_parse_err {
    int erRNo;
    struct usipy_codeptr loc;
    const char *reason;
};

struct usipy_sip_msg_iterator {
    struct usipy_str msg_onwire;
    int i;
    int over;
    int last;
    uint32_t oword;
    char cshift;
};

#define USIPY_MSG_PARSE_ERR_init { \
  .erRNo = 0, .loc.fname = NULL, .loc.linen = 0, .loc.funcn = NULL \
}

struct usipy_msg *usipy_sip_msg_ctor_fromwire(const char *, size_t,
  struct usipy_msg_parse_err *);
void usipy_sip_msg_dtor(struct usipy_msg *);
void usipy_sip_msg_dump(const struct usipy_msg *, const char *);
int usipy_sip_msg_parse_hdrs(struct usipy_msg *, uint64_t);
int usipy_sip_msg_break_down(struct usipy_sip_msg_iterator *);

#define USIPY_HFT_MASK(hft) ((uint64_t)1 << (hft))
#define USIPY_HF_MASK(shp) (USIPY_HFT_MASK((shp)->hf_type->cantype))
