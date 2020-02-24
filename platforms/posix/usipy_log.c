#define _WITH_DPRINTF

#include <stdarg.h>
#include <stdio.h>
#include <stdatomic.h>
#include <unistd.h>

#include "usipy_port/log.h"

static struct {
    atomic_uint next_ticket;
    atomic_uint now_serving;
} _log_lock = {ATOMIC_VAR_INIT(0), ATOMIC_VAR_INIT(0)};

static void
_usipy_log_lock(void)
{
    unsigned int my_ticket;

    my_ticket = atomic_fetch_add_explicit(&(_log_lock.next_ticket), 1,
      memory_order_acq_rel);
    while (atomic_load_explicit(&(_log_lock.now_serving),
      memory_order_acquire) != my_ticket) {
        usleep(1);
    }
}

static void
_usipy_log_unlock(void)
{

    atomic_fetch_add_explicit(&(_log_lock.now_serving), 1,
      memory_order_release);
}

void
usipy_log_write(int lvl, const char *tag, const char *fmt , ...)
{
    va_list ap;

    _usipy_log_lock();
    fprintf(stderr, "%s: ", tag);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    _usipy_log_unlock();
    va_end(ap);
    return;
}

