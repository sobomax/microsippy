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

#include "bits/turbocompare.h"

#include "usipy_sip_hdr_db_pdata.h"

static const struct usipy_hdr_db_entr usipy_hdr_db[USIPY_HF_max + 1] = {
    {.cantype = USIPY_HF_generic},
    {.cantype = USIPY_HF_ACCEPT, .name = USIPY_2STR("Accept"),
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_ALLOW, .name = USIPY_2STR("Allow"),
     .flags.csl_allowed = 1},
    {.cantype = USIPY_HF_ALSO, .name = USIPY_2STR("Also")},
    {.cantype = USIPY_HF_AUTHORIZATION, .name = USIPY_2STR("Authorization")},
    {.cantype = USIPY_HF_CCDIVERSION, .name = USIPY_2STR("CC-Diversion")},
    {
     .cantype = USIPY_HF_CSEQ,
     .name = USIPY_2STR("CSeq"),
     .dump = usipy_sip_hdr_cseq_dump,
     .parse = usipy_sip_hdr_cseq_parse,
     .parsed_memb_name = "cseq"
    },
    {
     .cantype = USIPY_HF_CALLID,
     .name = USIPY_2STR("Call-ID"),
     .parse = usipy_sip_hdr_1token_parse,
     .parsed_memb_name = "generic"
    },
    {
      .cantype = USIPY_HF_CONTACT,
      .name = USIPY_2STR("Contact"),
      .parse = usipy_sip_hdr_nameaddr_parse,
      .parsed_memb_name = "contact",
      .dump = usipy_sip_hdr_nameaddr_dump,
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_CONTENTLENGTH, .name = USIPY_2STR("Content-Length")},
    {.cantype = USIPY_HF_CONTENTTYPE, .name = USIPY_2STR("Content-Type")},
    {.cantype = USIPY_HF_DIVERSION, .name = USIPY_2STR("Diversion")},
    {.cantype = USIPY_HF_EXPIRES, .name = USIPY_2STR("Expires")},
    {
      .cantype = USIPY_HF_FROM,
      .name = USIPY_2STR("From"),
      .parse = usipy_sip_hdr_nameaddr_parse,
      .parsed_memb_name = "from",
      .dump = usipy_sip_hdr_nameaddr_dump
    },
    {.cantype = USIPY_HF_MAXFORWARDS, .name = USIPY_2STR("Max-Forwards")},
    {.cantype = USIPY_HF_PASSERTEDIDENTITY, .name = USIPY_2STR("P-Asserted-Identity")},
    {.cantype = USIPY_HF_PROXYAUTHENTICATE, .name = USIPY_2STR("Proxy-Authenticate")},
    {.cantype = USIPY_HF_PROXYAUTHORIZATION, .name = USIPY_2STR("Proxy-Authorization")},
    {.cantype = USIPY_HF_RACK, .name = USIPY_2STR("RAck")},
    {.cantype = USIPY_HF_RSEQ, .name = USIPY_2STR("RSeq")},
    {.cantype = USIPY_HF_REASON, .name = USIPY_2STR("Reason")},
    {
      .cantype = USIPY_HF_RECORDROUTE,
      .name = USIPY_2STR("Record-Route"),
      .parse = usipy_sip_hdr_nameaddr_parse,
      .parsed_memb_name = "recordroute",
      .dump = usipy_sip_hdr_nameaddr_dump,
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_REFERTO, .name = USIPY_2STR("Refer-To")},
    {.cantype = USIPY_HF_REFERREDBY, .name = USIPY_2STR("Referred-By")},
    {.cantype = USIPY_HF_REPLACES, .name = USIPY_2STR("Replaces")},
    {
      .cantype = USIPY_HF_ROUTE,
      .name = USIPY_2STR("Route"),
      .parse = usipy_sip_hdr_nameaddr_parse,
      .parsed_memb_name = "route",
      .dump = usipy_sip_hdr_nameaddr_dump,
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_SERVER, .name = USIPY_2STR("Server")},
    {.cantype = USIPY_HF_SUPPORTED, .name = USIPY_2STR("Supported"),
     .flags.csl_allowed = 1},
    {
      .cantype = USIPY_HF_TO,
      .name = USIPY_2STR("To"),
      .parse = usipy_sip_hdr_nameaddr_parse,
      .parsed_memb_name = "to",
      .dump = usipy_sip_hdr_nameaddr_dump
    },
    {.cantype = USIPY_HF_USERAGENT, .name = USIPY_2STR("User-Agent")},
    {
      .cantype = USIPY_HF_VIA,
      .name = USIPY_2STR("Via"),
      .dump = usipy_sip_hdr_via_dump,
      .parse = usipy_sip_hdr_via_parse,
      .parsed_memb_name = "via",
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_WWWAUTHENTICATE, .name = USIPY_2STR("WWW-Authenticate")},
    {.cantype = USIPY_HF_WARNING, .name = USIPY_2STR("Warning"),
     .flags.csl_allowed = 1},
    {
      .cantype = USIPY_HF_CALLID,
      .name = USIPY_2STR("i"),
      .parse = usipy_sip_hdr_1token_parse,
      .parsed_memb_name = "generic",
    },
    {
      .cantype = USIPY_HF_CONTACT,
      .name = USIPY_2STR("m"),
      .parse = usipy_sip_hdr_nameaddr_parse,
      .parsed_memb_name = "contact",
      .dump = usipy_sip_hdr_nameaddr_dump,
      .flags.csl_allowed = 1
    },
    {.cantype = USIPY_HF_CONTENTLENGTH, .name = USIPY_2STR("l")},
    {.cantype = USIPY_HF_CONTENTTYPE, .name = USIPY_2STR("c")},
    {
      .cantype = USIPY_HF_FROM,
      .name = USIPY_2STR("f"),
      .parse = usipy_sip_hdr_nameaddr_parse,
      .parsed_memb_name = "from",
      .dump = usipy_sip_hdr_nameaddr_dump
    },
    {.cantype = USIPY_HF_REFERTO, .name = USIPY_2STR("r")},
    {.cantype = USIPY_HF_SUPPORTED, .name = USIPY_2STR("k"),
     .flags.csl_allowed = 1},
    {
      .cantype = USIPY_HF_TO,
      .name = USIPY_2STR("t"),
      .parse = usipy_sip_hdr_nameaddr_parse,
      .parsed_memb_name = "to",
      .dump = usipy_sip_hdr_nameaddr_dump
    },
    {
      .cantype = USIPY_HF_VIA,
      .name = USIPY_2STR("v"),
      .dump = usipy_sip_hdr_via_dump,
      .parse = usipy_sip_hdr_via_parse,
      .parsed_memb_name = "via",
      .flags.csl_allowed = 1
    }
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
        if (r->name.l != hname->l || turbo_casebcmp(r->name.s.ro, hname->s.ro, hname->l) != 0) {
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
