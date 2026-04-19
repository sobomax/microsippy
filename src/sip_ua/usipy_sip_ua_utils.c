#include <stddef.h>

#include "public/usipy_sip_msg.h"
#include "public/usipy_sip_ua_utils.h"
#include "public/usipy_str.h"
#include "usipy_tvpair.h"
#include "usipy_sip_uri.h"

bool
usipy_sip_tm_addr_same(const struct usipy_sip_tm_addr *ap,
  const struct usipy_sip_tm_addr *bp)
{

    if (ap == NULL || bp == NULL) {
        return (false);
    }
    return (ap->af == bp->af &&
      ap->port == bp->port &&
      ap->transport == bp->transport &&
      usipy_str_eq(&ap->host, &bp->host));
}

bool
usipy_sip_ua_request_targets_user(const struct usipy_msg *msg,
  const struct usipy_str *usernamep)
{
    const struct usipy_sip_uri *urip;

    if (msg == NULL || usernamep == NULL || msg->kind != USIPY_SIP_MSG_REQ) {
        return (false);
    }
    urip = msg->sline.parsed.rl.ruri;
    if (urip == NULL) {
        return (false);
    }
    return (usipy_str_eq(&urip->user, usernamep));
}

int
usipy_sip_ua_schedule_refresh(unsigned int expires, uint64_t now_ms,
  uint64_t *next_refresh_at_msp)
{
    uint64_t refresh_delay_s;

    if (next_refresh_at_msp == NULL || expires == 0) {
        return (-1);
    }
    if (expires > 90) {
        refresh_delay_s = expires - 30u;
    } else {
        refresh_delay_s = expires / 2u;
    }
    if (refresh_delay_s == 0) {
        refresh_delay_s = 1;
    }
    *next_refresh_at_msp = now_ms + (refresh_delay_s * 1000u);
    return (0);
}

int
usipy_sip_ua_reset(struct usipy_sip_ua **uapp,
  const struct usipy_sip_ua_ctor_params *ctorp)
{
    struct usipy_sip_ua *uap;

    if (uapp == NULL || ctorp == NULL) {
        return (USIPY_SIP_TM_ERR_INVAL);
    }
    if (*uapp != NULL) {
        usipy_sip_ua_dtor(*uapp);
    }
    uap = usipy_sip_ua_ctor(ctorp);
    if (uap == NULL) {
        *uapp = NULL;
        return (USIPY_SIP_TM_ERR_NOMEM);
    }
    *uapp = uap;
    return (USIPY_SIP_TM_OK);
}
