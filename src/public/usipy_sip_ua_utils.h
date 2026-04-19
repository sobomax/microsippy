#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "usipy_sip_tm.h"
#include "usipy_sip_ua.h"

struct usipy_msg;
struct usipy_str;

bool usipy_sip_tm_addr_same(const struct usipy_sip_tm_addr *ap,
  const struct usipy_sip_tm_addr *bp);
bool usipy_sip_ua_request_targets_user(const struct usipy_msg *msg,
  const struct usipy_str *usernamep);
int usipy_sip_ua_schedule_refresh(unsigned int expires, uint64_t now_ms,
  uint64_t *next_refresh_at_msp);
int usipy_sip_ua_reset(struct usipy_sip_ua **uapp,
  const struct usipy_sip_ua_ctor_params *ctorp);
