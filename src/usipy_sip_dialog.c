#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "usipy_debug.h"
#include "public/usipy_msg_heap.h"
#include "public/usipy_sip_dialog.h"
#include "public/usipy_sip_msg.h"
#include "public/usipy_sip_hdr_types.h"
#include "public/usipy_str.h"
#include "usipy_tvpair.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_hdr_nameaddr.h"
#include "usipy_sip_tm_internal.h"
#include "usipy_sip_uri.h"

#define USIPY_SIP_DIALOG_HEAP_SIZE 512u

struct usipy_sip_dialog {
    struct usipy_sip_tm *tm;
    struct usipy_msg_heap heap;
    struct usipy_sip_tm_new_in_dialog_transaction_params request_template;
    int ended;
    unsigned char _storage[USIPY_SIP_DIALOG_HEAP_SIZE];
};

struct usipy_sip_dialog *
usipy_sip_dialog_uac_ctor(struct usipy_sip_tm *tm, size_t invite_index,
  const struct usipy_msg *msg)
{
    struct usipy_sip_dialog *dp;

    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(msg != NULL);
    dp = calloc(1, sizeof(*dp));
    if (dp == NULL) {
        return (NULL);
    }
    usipy_msg_heap_init(&dp->heap, dp->_storage, sizeof(dp->_storage), NULL, 0);
    dp->tm = tm;
    if (usipy_sip_tm_init_uac_dialog_request_params(tm, invite_index, msg,
      USIPY_SIP_METHOD_BYE, &dp->heap, &dp->request_template) != USIPY_SIP_TM_OK) {
        free(dp);
        return (NULL);
    }
    return (dp);
}

struct usipy_sip_dialog *
usipy_sip_dialog_uas_ctor(struct usipy_sip_tm *tm, size_t invite_index,
  const struct usipy_sip_tm_uas_response_params *rpp)
{
    struct usipy_sip_dialog *dp;

    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(rpp != NULL);
    if (rpp->status.code < 200 || rpp->status.code > 299) {
        return (NULL);
    }
    dp = calloc(1, sizeof(*dp));
    if (dp == NULL) {
        return (NULL);
    }
    usipy_msg_heap_init(&dp->heap, dp->_storage, sizeof(dp->_storage), NULL, 0);
    dp->tm = tm;
    if (usipy_sip_tm_init_uas_dialog_request_params(tm, invite_index,
      USIPY_SIP_METHOD_BYE, &dp->heap, &dp->request_template) != USIPY_SIP_TM_OK ||
      usipy_sip_tm_send_uas_response(tm, invite_index, rpp) != USIPY_SIP_TM_OK) {
        free(dp);
        return (NULL);
    }
    return (dp);
}

void
usipy_sip_dialog_dtor(struct usipy_sip_dialog *dp)
{
    USIPY_DASSERT(dp != NULL);
    free(dp);
}

int
usipy_sip_dialog_end(struct usipy_sip_dialog *dp,
  const struct usipy_sip_tm_uac_callbacks *cbp, size_t *indexp)
{
    struct usipy_sip_tm_new_in_dialog_transaction_params tp = {0};
    struct usipy_sip_tm_uac_callbacks callbacks = {0};
    int rval;

    USIPY_DASSERT(dp != NULL);
    USIPY_DASSERT(indexp != NULL);
    if (dp->ended) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    if (cbp != NULL) {
        callbacks = *cbp;
    }
    tp = dp->request_template;
    tp.request_id.cseq += 1;
    tp.callbacks = callbacks;
    rval = usipy_sip_tm_new_in_dialog_transaction(dp->tm, &tp, indexp);

    if (rval != USIPY_SIP_TM_OK) {
        return (rval);
    }
    dp->request_template.request_id.cseq = tp.request_id.cseq;
    dp->ended = 1;
    return (USIPY_SIP_TM_OK);
}
