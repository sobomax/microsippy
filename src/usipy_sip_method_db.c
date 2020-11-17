#include <stdlib.h>
#include <string.h>

#include "bits/turbocompare.h"

#include "public/usipy_str.h"
#include "public/usipy_sip_method_types.h"
#include "usipy_sip_method_db.h"

static const struct usipy_method_db_entr usipy_method_db[USIPY_SIP_METHOD_max + 1] = {
    {.cantype = USIPY_SIP_METHOD_generic},
    {.cantype = USIPY_SIP_METHOD_ACK, .name = USIPY_2STR("ACK")},
    {.cantype = USIPY_SIP_METHOD_BYE, .name = USIPY_2STR("BYE")},
    {.cantype = USIPY_SIP_METHOD_CANCEL, .name = USIPY_2STR("CANCEL")},
    {.cantype = USIPY_SIP_METHOD_INFO, .name = USIPY_2STR("INFO")},
    {.cantype = USIPY_SIP_METHOD_INVITE, .name = USIPY_2STR("INVITE")},
    {.cantype = USIPY_SIP_METHOD_OPTIONS, .name = USIPY_2STR("OPTIONS")},
    {.cantype = USIPY_SIP_METHOD_PRACK, .name = USIPY_2STR("PRACK")},
    {.cantype = USIPY_SIP_METHOD_REFER, .name = USIPY_2STR("REFER")},
    {.cantype = USIPY_SIP_METHOD_REGISTER, .name = USIPY_2STR("REGISTER")},
    {.cantype = USIPY_SIP_METHOD_SUBSCRIBE, .name = USIPY_2STR("SUBSCRIBE")},
};

#define TOTAL_KEYWORDS 10
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 9
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 13
/* maximum key range = 11, duplicates = 0 */

static unsigned int
method_hash(const struct usipy_str *sp)
{
    static unsigned char asso_values[] = {
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 10,  0,  5, 14, 14,
      14, 14, 14,  0, 14, 14, 14, 14, 14,  0,
       5, 14,  0,  0, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 10,  0,  5,
      14, 14, 14, 14, 14,  0, 14, 14, 14, 14,
      14,  0,  5, 14,  0,  0, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      14, 14, 14, 14, 14, 14
    };
    return (sp->l + asso_values[(unsigned char)sp->s.ro[0]]);
}

const struct usipy_method_db_entr *
usipy_method_db_lookup(const struct usipy_str *tp)
{
    static const struct usipy_method_db_entr *wordlist[] = {
      &usipy_method_db[USIPY_SIP_METHOD_generic],
      &usipy_method_db[USIPY_SIP_METHOD_generic],
      &usipy_method_db[USIPY_SIP_METHOD_generic],
      &usipy_method_db[USIPY_SIP_METHOD_BYE],
      &usipy_method_db[USIPY_SIP_METHOD_INFO],
      &usipy_method_db[USIPY_SIP_METHOD_REFER],
      &usipy_method_db[USIPY_SIP_METHOD_INVITE],
      &usipy_method_db[USIPY_SIP_METHOD_OPTIONS],
      &usipy_method_db[USIPY_SIP_METHOD_REGISTER],
      &usipy_method_db[USIPY_SIP_METHOD_SUBSCRIBE],
      &usipy_method_db[USIPY_SIP_METHOD_PRACK],
      &usipy_method_db[USIPY_SIP_METHOD_CANCEL],
      &usipy_method_db[USIPY_SIP_METHOD_generic],
      &usipy_method_db[USIPY_SIP_METHOD_ACK],
    };

    if (tp->l <= MAX_WORD_LENGTH && tp->l >= MIN_WORD_LENGTH) {
        int key = method_hash(tp);

        if (key <= MAX_HASH_VALUE && key >= 0) {
            const struct usipy_method_db_entr *wp = wordlist[key];

            if (wp->name.l == tp->l && turbo_casebcmp(tp->s.ro, wp->name.s.ro, wp->name.l) == 0)
                return (wp);
        }
    }
    return (&usipy_method_db[USIPY_SIP_METHOD_generic]);
}
