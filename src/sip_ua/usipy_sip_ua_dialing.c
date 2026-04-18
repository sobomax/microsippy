#include <stddef.h>

#include "usipy_debug.h"
#include "sip_ua/usipy_sip_ua_internal.h"

static int
usipy_sip_ua_dialing_on_transaction(struct usipy_sip_ua *uap, size_t tx_index,
  const struct usipy_msg *msg)
{
    (void)uap;
    (void)tx_index;
    (void)msg;
    return (USIPY_SIP_TM_ERR_UNSUPPORTED);
}

static int
usipy_sip_ua_dialing_on_event(struct usipy_sip_ua *uap,
  const struct usipy_sip_ua_event *eventp, size_t *indexp)
{
    int rval;

    USIPY_DASSERT(uap != NULL);
    USIPY_DASSERT(eventp != NULL);
    USIPY_DASSERT(indexp != NULL);

    *indexp = USIPY_SIP_TM_TX_INDEX_NONE;
    if (eventp->type != USIPY_SIP_UA_EVENT_DISCONNECT) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    rval = usipy_sip_tm_cancel(uap->tm, uap->tx_index);
    if (rval != USIPY_SIP_TM_OK) {
        return (rval);
    }
    usipy_sip_ua_transition(uap, USIPY_SIP_UA_STATE_DISCONNECTED);
    usipy_sip_ua_emit_event(uap, USIPY_SIP_UA_EMIT_DISCONNECT, uap->tx_index, NULL);
    *indexp = uap->tx_index;
    return (USIPY_SIP_TM_OK);
}

static int
usipy_sip_ua_dialing_on_tx_response(struct usipy_sip_ua *uap, size_t tx_index,
  const struct usipy_msg *msg)
{
    const struct usipy_sip_tm_tx *txp;
    unsigned int scode;
    int rval;

    USIPY_DASSERT(uap != NULL);
    USIPY_DASSERT(msg != NULL);

    rval = usipy_sip_ua_expect_transaction(uap, tx_index, USIPY_SIP_TM_ROLE_UAC,
      USIPY_SIP_METHOD_INVITE, &txp);
    if (rval != USIPY_SIP_TM_OK) {
        return (rval);
    }
    (void)txp;
    if (tx_index != uap->tx_index || msg->kind != USIPY_SIP_MSG_RES) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    scode = msg->sline.parsed.sl.status.code;
    if (scode < 200) {
        return (USIPY_SIP_TM_OK);
    }
    if (scode < 300) {
        if (uap->dialogp == NULL) {
            uap->dialogp = usipy_sip_dialog_uac_ctor(uap->tm, tx_index, msg);
            if (uap->dialogp == NULL) {
                return (USIPY_SIP_TM_ERR_BADMSG);
            }
        }
        usipy_sip_ua_transition(uap, USIPY_SIP_UA_STATE_CONNECTED);
        usipy_sip_ua_emit_event(uap, USIPY_SIP_UA_EMIT_CONNECT, tx_index, msg);
        return (USIPY_SIP_TM_OK);
    }
    usipy_sip_ua_transition(uap, USIPY_SIP_UA_STATE_DISCONNECTED);
    usipy_sip_ua_emit_event(uap, USIPY_SIP_UA_EMIT_DISCONNECT, tx_index, msg);
    return (USIPY_SIP_TM_OK);
}

const struct usipy_sip_ua_state_ops usipy_sip_ua_dialing_ops = {
    .on_transaction = usipy_sip_ua_dialing_on_transaction,
    .on_event = usipy_sip_ua_dialing_on_event,
    .on_tx_response = usipy_sip_ua_dialing_on_tx_response,
};
