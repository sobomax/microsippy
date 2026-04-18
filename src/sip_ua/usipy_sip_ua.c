#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "usipy_debug.h"
#include "sip_ua/usipy_sip_ua_internal.h"

static int
usipy_sip_ua_unsupported_transaction(struct usipy_sip_ua *uap, size_t tx_index,
  const struct usipy_msg *msg)
{
    (void)uap;
    (void)tx_index;
    (void)msg;
    return (USIPY_SIP_TM_ERR_UNSUPPORTED);
}

static int
usipy_sip_ua_unsupported_event(struct usipy_sip_ua *uap,
  const struct usipy_sip_ua_event *eventp, size_t *indexp)
{
    (void)uap;
    (void)eventp;
    if (indexp != NULL) {
        *indexp = USIPY_SIP_TM_TX_INDEX_NONE;
    }
    return (USIPY_SIP_TM_ERR_UNSUPPORTED);
}

static int
usipy_sip_ua_unsupported_response(struct usipy_sip_ua *uap, size_t tx_index,
  const struct usipy_msg *msg)
{
    (void)uap;
    (void)tx_index;
    (void)msg;
    return (USIPY_SIP_TM_ERR_UNSUPPORTED);
}

const struct usipy_sip_ua_state_ops *
usipy_sip_ua_state_ops_get(enum usipy_sip_ua_state state)
{
    switch (state) {
    case USIPY_SIP_UA_STATE_IDLE:
        return (&usipy_sip_ua_idle_ops);

    case USIPY_SIP_UA_STATE_TRYING:
        return (&usipy_sip_ua_trying_ops);

    case USIPY_SIP_UA_STATE_DIALING:
        return (&usipy_sip_ua_dialing_ops);

    case USIPY_SIP_UA_STATE_CONNECTED:
        return (&usipy_sip_ua_connected_ops);

    case USIPY_SIP_UA_STATE_DISCONNECTED:
        return (&usipy_sip_ua_disconnected_ops);
    }
    USIPY_DABORT();
}

void
usipy_sip_ua_transition(struct usipy_sip_ua *uap, enum usipy_sip_ua_state state)
{
    USIPY_DASSERT(uap != NULL);
    uap->state = state;
}

void
usipy_sip_ua_emit_event(struct usipy_sip_ua *uap, enum usipy_sip_ua_emit_type type,
  size_t tx_index, const struct usipy_msg *msg)
{
    struct usipy_sip_ua_emit emitp;

    USIPY_DASSERT(uap != NULL);
    if (uap->emit == NULL) {
        return;
    }
    emitp = (struct usipy_sip_ua_emit){
      .type = type,
      .state = uap->state,
      .role = uap->role,
      .transaction_index = tx_index,
      .message = msg,
      .body = (msg != NULL ? msg->body : USIPY_STR_NULL),
    };
    uap->emit(uap->emit_arg, &emitp);
}

int
usipy_sip_ua_expect_transaction(const struct usipy_sip_ua *uap, size_t tx_index,
  enum usipy_sip_tm_role role, uint8_t method_type, const struct usipy_sip_tm_tx **txpp)
{
    const struct usipy_sip_tm_tx *txp;

    if (uap == NULL || txpp == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    txp = usipy_sip_tm_get_transaction(uap->tm, tx_index);
    if (txp == NULL) {
        return (USIPY_SIP_TM_ERR_NOT_FOUND);
    }
    if (txp->role != role || txp->common.id.method_type != method_type) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    *txpp = txp;
    return (USIPY_SIP_TM_OK);
}

struct usipy_sip_ua *
usipy_sip_ua_ctor(const struct usipy_sip_ua_ctor_params *ipp)
{
    struct usipy_sip_ua *uap;

    if (ipp == NULL || ipp->tm == NULL) {
        return (NULL);
    }
    uap = calloc(1, sizeof(*uap));
    if (uap == NULL) {
        return (NULL);
    }
    uap->tm = ipp->tm;
    uap->state = USIPY_SIP_UA_STATE_IDLE;
    uap->role = USIPY_SIP_TM_ROLE_UAC;
    uap->tx_index = USIPY_SIP_TM_TX_INDEX_NONE;
    uap->emit = ipp->emit;
    uap->emit_arg = ipp->emit_arg;
    return (uap);
}

void
usipy_sip_ua_dtor(struct usipy_sip_ua *uap)
{
    if (uap == NULL) {
        return;
    }
    if (uap->dialogp != NULL) {
        usipy_sip_dialog_dtor(uap->dialogp);
    }
    free(uap);
}

enum usipy_sip_ua_state
usipy_sip_ua_get_state(const struct usipy_sip_ua *uap)
{
    if (uap == NULL) {
        return (USIPY_SIP_UA_STATE_DISCONNECTED);
    }
    return (uap->state);
}

int
usipy_sip_ua_on_event(struct usipy_sip_ua *uap, const struct usipy_sip_ua_event *eventp,
  size_t *indexp)
{
    const struct usipy_sip_ua_state_ops *opp;
    size_t tx_index;

    if (uap == NULL || eventp == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    opp = usipy_sip_ua_state_ops_get(uap->state);
    tx_index = USIPY_SIP_TM_TX_INDEX_NONE;
    const int rval = opp->on_event(uap, eventp, &tx_index);

    if (indexp != NULL) {
        *indexp = tx_index;
    }
    return (rval);
}

int
usipy_sip_ua_on_transaction(struct usipy_sip_ua *uap, size_t tx_index,
  const struct usipy_msg *msg)
{
    const struct usipy_sip_ua_state_ops *opp;

    if (uap == NULL || msg == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    opp = usipy_sip_ua_state_ops_get(uap->state);
    return (opp->on_transaction(uap, tx_index, msg));
}

int
usipy_sip_ua_on_tx_response(struct usipy_sip_ua *uap, size_t tx_index,
  const struct usipy_msg *msg)
{
    const struct usipy_sip_ua_state_ops *opp;

    if (uap == NULL || msg == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    opp = usipy_sip_ua_state_ops_get(uap->state);
    return (opp->on_tx_response(uap, tx_index, msg));
}
