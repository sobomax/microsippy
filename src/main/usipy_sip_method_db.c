#include <stdlib.h>

#include "usipy_sip_method_types.h"
#include "usipy_sip_method_db.h"

static const struct usipy_method_db_entr usipy_method_db[USIPY_SIP_METHOD_max + 1] = {
    {.cantype = USIPY_SIP_METHOD_generic},
    {.cantype = USIPY_SIP_METHOD_ACK, .name = {.s.ro = "ACK", .l = 3}},
    {.cantype = USIPY_SIP_METHOD_BYE, .name = {.s.ro = "BYE", .l = 3}},
    {.cantype = USIPY_SIP_METHOD_CANCEL, .name = {.s.ro = "CANCEL", .l = 6}},
    {.cantype = USIPY_SIP_METHOD_INFO, .name = {.s.ro = "INFO", .l = 4}},
    {.cantype = USIPY_SIP_METHOD_INVITE, .name = {.s.ro = "INVITE", .l = 6}},
    {.cantype = USIPY_SIP_METHOD_OPTIONS, .name = {.s.ro = "OPTIONS", .l = 7}},
    {.cantype = USIPY_SIP_METHOD_PRACK, .name = {.s.ro = "PRACK", .l = 4}},
    {.cantype = USIPY_SIP_METHOD_REFER, .name = {.s.ro = "REFER", .l = 5}},
    {.cantype = USIPY_SIP_METHOD_REGISTER, .name = {.s.ro = "REGISTER", .l = 8}},
    {.cantype = USIPY_SIP_METHOD_SUBSCRIBE, .name = {.s.ro = "SUBSCRIBE", .l = 9}},
};

#define TOTAL_KEYWORDS 10
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 9
#define MIN_HASH_VALUE 3
#define MAX_HASH_VALUE 13
/* maximum key range = 11, duplicates = 0 */

static int
method_case_strcmp(const struct usipy_str *sp1, const struct usipy_str *sp2)
{
    size_t i;

    for (i = 0; i < sp1->l; i++) {
        unsigned char c1 = ((unsigned char)(sp1->s.ro[i])) & (~32);
        unsigned char c2 = (unsigned char)(sp2->s.ro[i]);
        if (c1 == c2)
            continue;
        return (int)c1 - (int)c2;
    }
    return (0);
}

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

            if (wp->name.l == tp->l && !method_case_strcmp(tp, &wp->name))
                return (wp);
        }
    }
    return (&usipy_method_db[USIPY_SIP_METHOD_generic]);
}
