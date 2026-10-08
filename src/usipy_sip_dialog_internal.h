#pragma once

#include <stddef.h>

struct usipy_sip_dialog;

/* Told when an accepted call ends for want of the ACK of its 2xx, after the
 * dialog has sent the BYE (its transaction index, or none if it couldn't) */
void usipy_sip_dialog_set_no_ack_handler(struct usipy_sip_dialog *,
  void (*)(void *, size_t), void *);
