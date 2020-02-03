#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"

#include "usipy_msg_heap.h"
#include "usipy_str.h"
#include "usipy_sip_uri.h"

struct usipy_sip_uri *
usipy_sip_uri_parse(struct usipy_msg_heap *mhp, const struct usipy_str *up)
{
    struct usipy_str iup, pspace, hspace, pnum;
    struct usipy_sip_uri rval, *rp;

    iup = *up;
    if (usipy_str_split_elem(&iup, ':', &rval.proto) != 0)
        return (NULL);
    if (usipy_str_split_elem(&iup, ';', &pnum) != 0) {
        pspace = USIPY_STR_NULL;
        if (usipy_str_split_elem(&iup, '?', &pnum) != 0) {
            pnum = iup;
            iup = USIPY_STR_NULL;
            hspace = USIPY_STR_NULL;
        } else {
            hspace = iup;
        }
    } else {
        if (usipy_str_split_elem(&iup, '?', &pspace) != 0) {
            pspace = iup;
            hspace = USIPY_STR_NULL;
        } else {
            hspace = iup;
        }
    }
    if (pnum.l == 0) {
        return (NULL);
    }
    if (usipy_str_split_elem(&pnum, '@', &rval.user) == 0) {
        if (usipy_str_split_elem(&rval.user, ':', &rval.password) != 0) {
            rval.password = USIPY_STR_NULL;
        }
    } else {
        rval.user = USIPY_STR_NULL;
        rval.password = USIPY_STR_NULL;
    }
    if (usipy_str_split_elem(&pnum, ':', &rval.host) == 0) {
        if (usipy_str_atoui_range(&pnum, &rval.port, 1, 65535) != 0) {
            return (NULL);
        }
    } else {
        rval.host = pnum;
        rval.port = 0;
    }

    rval.parameters = pspace;
    rval.headers = hspace;
    rp = usipy_msg_heap_alloc(mhp, sizeof(struct usipy_sip_uri));
    if (rp == NULL) {
        return (NULL);
    }
    *rp = rval;
    return (rp);
}

#define DUMP_STR(sname) \
    ESP_LOGI(log_tag, "%s" #sname " = \"%.*s\"", log_pref, up->sname.l, \
      up->sname.s.ro)
#define DUMP_UINT(sname) \
    ESP_LOGI(log_tag, "%s" #sname " = %u", log_pref, up->sname)

void
usipy_sip_uri_dump(const struct usipy_sip_uri *up, const char *log_tag,
  const char *log_pref)
{

    DUMP_STR(proto);
    DUMP_STR(user);
    DUMP_STR(password);
    DUMP_STR(host);
    if (up->port > 0)
        DUMP_UINT(port);
    DUMP_STR(parameters);
    DUMP_STR(headers);
}
