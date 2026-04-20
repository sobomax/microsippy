#include <assert.h>
#include <stdarg.h>

#include "public/usipy_platform.h"

uint64_t
usipy_platform_mono_ms(void)
{

    assert(usipy_platform_handlers.mono_ms != NULL);
    return (usipy_platform_handlers.mono_ms(usipy_platform_handlers.arg));
}

void
usipy_platform_sleep_until_ms(uint64_t when_ms)
{

    assert(usipy_platform_handlers.sleep_until_ms != NULL);
    usipy_platform_handlers.sleep_until_ms(usipy_platform_handlers.arg,
      when_ms);
}

int
usipy_platform_random_fill(void *buf, size_t len)
{

    assert(usipy_platform_handlers.random_fill != NULL);
    return (usipy_platform_handlers.random_fill(usipy_platform_handlers.arg,
      buf, len));
}

void
usipy_platform_log_vwrite(int lvl, const char *tag, const char *fmt, va_list ap)
{

    assert(usipy_platform_handlers.log_vwrite != NULL);
    usipy_platform_handlers.log_vwrite(usipy_platform_handlers.arg, lvl, tag,
      fmt, ap);
}

void
usipy_log_write(int lvl, const char *tag, const char *fmt , ...)
{
    va_list ap;

    va_start(ap, fmt);
    usipy_platform_log_vwrite(lvl, tag, fmt, ap);
    va_end(ap);
}
