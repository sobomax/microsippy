#include <stdint.h>
#include <string.h>

#include "usipy_types.h"
#include "usipy_str.h"
#include "usipy_fast_parser.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_tvpair.h"
#include "usipy_sip_hdr_via.h"
#include "usipy_sip_hdr_cseq.h"
#include "usipy_sip_hdr_onetoken.h"
#include "usipy_sip_hdr_nameaddr.h"

#include "usipy_sip_hdr_db_pdata.h"

static const struct usipy_hdr_db_entr usipy_hdr_db[USIPY_HF_max + 1] = {
    {.cantype = USIPY_HF_generic},
    {.cantype = USIPY_HF_ACCEPT, .name = {.s.ro = "Accept", .l = 6},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_ALLOW, .name = {.s.ro = "Allow", .l = 5},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_ALSO, .name = {.s.ro = "Also", .l = 4}},
    {.cantype = USIPY_HF_AUTHORIZATION, .name = {.s.ro = "Authorization", .l = 13}},
    {.cantype = USIPY_HF_CCDIVERSION, .name = {.s.ro = "CC-Diversion", .l = 12}},
    {.cantype = USIPY_HF_CSEQ, .name = {.s.ro = "CSeq", .l = 4}, .dump = usipy_sip_hdr_cseq_dump,
     .parse = usipy_sip_hdr_cseq_parse},
    {.cantype = USIPY_HF_CALLID, .name = {.s.ro = "Call-ID", .l = 7},
     .parse = usipy_sip_hdr_1token_parse},
    {
      .cantype = USIPY_HF_CONTACT,
      .name = {.s.ro = "Contact", .l = 7},
      .parse = usipy_sip_hdr_nameaddr_parse,
      .dump = usipy_sip_hdr_nameaddr_dump,
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_CONTENTLENGTH, .name = {.s.ro = "Content-Length", .l = 14}},
    {.cantype = USIPY_HF_CONTENTTYPE, .name = {.s.ro = "Content-Type", .l = 12}},
    {.cantype = USIPY_HF_DIVERSION, .name = {.s.ro = "Diversion", .l = 9}},
    {.cantype = USIPY_HF_EXPIRES, .name = {.s.ro = "Expires", .l = 7}},
    {
      .cantype = USIPY_HF_FROM,
      .name = {.s.ro = "From", .l = 4},
      .parse = usipy_sip_hdr_nameaddr_parse,
      .dump = usipy_sip_hdr_nameaddr_dump
    },
    {.cantype = USIPY_HF_MAXFORWARDS, .name = {.s.ro = "Max-Forwards", .l = 12}},
    {.cantype = USIPY_HF_PASSERTEDIDENTITY, .name = {.s.ro = "P-Asserted-Identity", .l = 19}},
    {.cantype = USIPY_HF_PROXYAUTHENTICATE, .name = {.s.ro = "Proxy-Authenticate", .l = 18}},
    {.cantype = USIPY_HF_PROXYAUTHORIZATION, .name = {.s.ro = "Proxy-Authorization", .l = 19}},
    {.cantype = USIPY_HF_RACK, .name = {.s.ro = "RAck", .l = 4}},
    {.cantype = USIPY_HF_RSEQ, .name = {.s.ro = "RSeq", .l = 4}},
    {.cantype = USIPY_HF_REASON, .name = {.s.ro = "Reason", .l = 6}},
    {
      .cantype = USIPY_HF_RECORDROUTE,
      .name = {.s.ro = "Record-Route", .l = 12},
      .parse = usipy_sip_hdr_nameaddr_parse,
      .dump = usipy_sip_hdr_nameaddr_dump,
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_REFERTO, .name = {.s.ro = "Refer-To", .l = 8}},
    {.cantype = USIPY_HF_REFERREDBY, .name = {.s.ro = "Referred-By", .l = 11}},
    {.cantype = USIPY_HF_REPLACES, .name = {.s.ro = "Replaces", .l = 8}},
    {
      .cantype = USIPY_HF_ROUTE,
      .name = {.s.ro = "Route", .l = 5},
      .parse = usipy_sip_hdr_nameaddr_parse,
      .dump = usipy_sip_hdr_nameaddr_dump,
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_SERVER, .name = {.s.ro = "Server", .l = 6}},
    {.cantype = USIPY_HF_SUPPORTED, .name = {.s.ro = "Supported", .l = 9},
     .flags.csl_allowed = 1},
    {
      .cantype = USIPY_HF_TO,
      .name = {.s.ro = "To", .l = 2},
      .parse = usipy_sip_hdr_nameaddr_parse,
      .dump = usipy_sip_hdr_nameaddr_dump
    },
    {.cantype = USIPY_HF_USERAGENT, .name = {.s.ro = "User-Agent", .l = 10}},
    {.cantype = USIPY_HF_VIA, .name = {.s.ro = "Via", .l = 3}, .dump = usipy_sip_hdr_via_dump,
     .parse = usipy_sip_hdr_via_parse, .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_WWWAUTHENTICATE, .name = {.s.ro = "WWW-Authenticate", .l = 16}},
    {.cantype = USIPY_HF_WARNING, .name = {.s.ro = "Warning", .l = 7},
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_CALLID, .name = {.s.ro = "i", .l = 1},
     .parse = usipy_sip_hdr_1token_parse},
    {
      .cantype = USIPY_HF_CONTACT,
      .name = {.s.ro = "m", .l = 1},
      .parse = usipy_sip_hdr_nameaddr_parse,
      .dump = usipy_sip_hdr_nameaddr_dump,
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_CONTENTLENGTH, .name = {.s.ro = "l", .l = 1}},
    {.cantype = USIPY_HF_CONTENTTYPE, .name = {.s.ro = "c", .l = 1}},
    {
      .cantype = USIPY_HF_FROM,
      .name = {.s.ro = "f", .l = 1},
      .parse = usipy_sip_hdr_nameaddr_parse,
      .dump = usipy_sip_hdr_nameaddr_dump
    },
    {.cantype = USIPY_HF_REFERTO, .name = {.s.ro = "r", .l = 1}},
    {.cantype = USIPY_HF_SUPPORTED, .name = {.s.ro = "k", .l = 1},
     .flags.csl_allowed = 1},
    {
      .cantype = USIPY_HF_TO,
      .name = {.s.ro = "t", .l = 1},
      .parse = usipy_sip_hdr_nameaddr_parse,
      .dump = usipy_sip_hdr_nameaddr_dump
    },
    {.cantype = USIPY_HF_VIA, .name = {.s.ro = "v", .l = 1}, .dump = usipy_sip_hdr_via_dump,
     .parse = usipy_sip_hdr_via_parse, .flags.csl_allowed = 1 }
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
