#include <stdint.h>
#include <string.h>

#include "usipy_types.h"
#include "usipy_str.h"
#include "usipy_fast_parser.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_hdr_via.h"
#include "usipy_sip_hdr_cseq.h"

static const struct usipy_fast_parser hdr_pdata = {
    .magic = 0x46,
    .toid = {
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_SUPPORTED,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_FROM_c, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_REFERTO, USIPY_HF_generic, USIPY_HF_CONTENTTYPE_c, USIPY_HF_RACK,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_CALLID, USIPY_HF_CONTENTLENGTH_c,
      USIPY_HF_CONTACT_c, USIPY_HF_generic, USIPY_HF_SUPPORTED_c, USIPY_HF_generic,
      USIPY_HF_CALLID_c, USIPY_HF_VIA_c, USIPY_HF_generic, USIPY_HF_TO_c,
      USIPY_HF_generic, USIPY_HF_REFERTO_c, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_WARNING, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_PROXYAUTHORIZATION,
      USIPY_HF_FROM, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_CCDIVERSION, USIPY_HF_CONTACT, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_VIA, USIPY_HF_generic, USIPY_HF_REFERREDBY, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_CSEQ, USIPY_HF_generic, USIPY_HF_RSEQ,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_CONTENTTYPE, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_EXPIRES, USIPY_HF_generic, USIPY_HF_AUTHORIZATION,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_PASSERTEDIDENTITY,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_ALSO, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_CONTENTLENGTH,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_REPLACES, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_TO, USIPY_HF_REASON, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_USERAGENT, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_RECORDROUTE, USIPY_HF_generic,
      USIPY_HF_WWWAUTHENTICATE, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_SERVER, USIPY_HF_ACCEPT,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_ALLOW, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_PROXYAUTHENTICATE, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_MAXFORWARDS, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_ROUTE, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic, USIPY_HF_DIVERSION, USIPY_HF_generic, USIPY_HF_generic,
      USIPY_HF_generic
    }
};

static const struct usipy_hdr_db_entr usipy_hdr_db[USIPY_HF_max + 1] = {
    {.cantype = USIPY_HF_generic},
    {.cantype = USIPY_HF_ACCEPT, .name = {.s.ro = "Accept", .l = 6},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_ALLOW, .name = {.s.ro = "Allow", .l = 5},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_ALSO, .name = {.s.ro = "Also", .l = 4}},
    {.cantype = USIPY_HF_AUTHORIZATION, .name = {.s.ro = "Authorization", .l = 13}},
    {.cantype = USIPY_HF_CCDIVERSION, .name = {.s.ro = "CC-Diversion", .l = 12}},
    {.cantype = USIPY_HF_CSEQ, .name = {.s.ro = "CSeq", .l = 4}, .dump = usipy_sip_hdr_cseq_dump,}
    {.cantype = USIPY_HF_CALLID, .name = {.s.ro = "Call-ID", .l = 7}},
    {.cantype = USIPY_HF_CONTACT, .name = {.s.ro = "Contact", .l = 7},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_CONTENTLENGTH, .name = {.s.ro = "Content-Length", .l = 14}},
    {.cantype = USIPY_HF_CONTENTTYPE, .name = {.s.ro = "Content-Type", .l = 12}},
    {.cantype = USIPY_HF_DIVERSION, .name = {.s.ro = "Diversion", .l = 9}},
    {.cantype = USIPY_HF_EXPIRES, .name = {.s.ro = "Expires", .l = 7}},
    {.cantype = USIPY_HF_FROM, .name = {.s.ro = "From", .l = 4}},
    {.cantype = USIPY_HF_MAXFORWARDS, .name = {.s.ro = "Max-Forwards", .l = 12}},
    {.cantype = USIPY_HF_PASSERTEDIDENTITY, .name = {.s.ro = "P-Asserted-Identity", .l = 19}},
    {.cantype = USIPY_HF_PROXYAUTHENTICATE, .name = {.s.ro = "Proxy-Authenticate", .l = 18}},
    {.cantype = USIPY_HF_PROXYAUTHORIZATION, .name = {.s.ro = "Proxy-Authorization", .l = 19}},
    {.cantype = USIPY_HF_RACK, .name = {.s.ro = "RAck", .l = 4}},
    {.cantype = USIPY_HF_RSEQ, .name = {.s.ro = "RSeq", .l = 4}},
    {.cantype = USIPY_HF_REASON, .name = {.s.ro = "Reason", .l = 6}},
    {.cantype = USIPY_HF_RECORDROUTE, .name = {.s.ro = "Record-Route", .l = 12},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_REFERTO, .name = {.s.ro = "Refer-To", .l = 8}},
    {.cantype = USIPY_HF_REFERREDBY, .name = {.s.ro = "Referred-By", .l = 11}},
    {.cantype = USIPY_HF_REPLACES, .name = {.s.ro = "Replaces", .l = 8}},
    {.cantype = USIPY_HF_ROUTE, .name = {.s.ro = "Route", .l = 5},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_SERVER, .name = {.s.ro = "Server", .l = 6}},
    {.cantype = USIPY_HF_SUPPORTED, .name = {.s.ro = "Supported", .l = 9},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_TO, .name = {.s.ro = "To", .l = 2}},
    {.cantype = USIPY_HF_USERAGENT, .name = {.s.ro = "User-Agent", .l = 10}},
    {.cantype = USIPY_HF_VIA, .name = {.s.ro = "Via", .l = 3}, .dump = usipy_sip_hdr_via_dump,
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_WWWAUTHENTICATE, .name = {.s.ro = "WWW-Authenticate", .l = 16}},
    {.cantype = USIPY_HF_WARNING, .name = {.s.ro = "Warning", .l = 7},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_CALLID, .name = {.s.ro = "i", .l = 1}},
    {.cantype = USIPY_HF_CONTACT, .name = {.s.ro = "m", .l = 1},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_CONTENTLENGTH, .name = {.s.ro = "l", .l = 1}},
    {.cantype = USIPY_HF_CONTENTTYPE, .name = {.s.ro = "c", .l = 1}},
    {.cantype = USIPY_HF_FROM, .name = {.s.ro = "f", .l = 1}},
    {.cantype = USIPY_HF_REFERTO, .name = {.s.ro = "r", .l = 1}},
    {.cantype = USIPY_HF_SUPPORTED, .name = {.s.ro = "k", .l = 1},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_TO, .name = {.s.ro = "t", .l = 1}},
    {.cantype = USIPY_HF_VIA, .name = {.s.ro = "v", .l = 1}, .dump = usipy_sip_hdr_via_dump,
     .flags.csl_allowed = 1 }
};

const struct usipy_hdr_db_entr *
usipy_hdr_db_lookup(const struct usipy_str *hname)
{
    int hid;
    const struct usipy_hdr_db_entr *r;

    hid = usipy_fp_classify(&hdr_pdata, hname);
    if (hid == -1)
        return (NULL);
    if (hid != USIPY_HF_generic) {
        r = &usipy_hdr_db[hid];
        if (r->name.l != hname->l || strncasecmp(r->name.s.ro, hname->s.ro, hname->l) != 0) {
            hid = USIPY_HF_generic;
        }
    }
    return (&usipy_hdr_db[hid]);
}

const struct usipy_hdr_db_entr *
usipy_hdr_db_byid(int hid)
{

    return (&usipy_hdr_db[hid]);
}
