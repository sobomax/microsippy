#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t (*usipy_platform_mono_ms_cb)(void *);
typedef void (*usipy_platform_sleep_until_ms_cb)(void *, uint64_t);
typedef int (*usipy_platform_random_fill_cb)(void *, void *, size_t);
typedef void (*usipy_platform_log_vwrite_cb)(void *, int, const char *,
  const char *, va_list);

struct usipy_platform_handlers {
    void *arg;
    usipy_platform_mono_ms_cb mono_ms;
    usipy_platform_sleep_until_ms_cb sleep_until_ms;
    usipy_platform_random_fill_cb random_fill;
    usipy_platform_log_vwrite_cb log_vwrite;
};

/*
 * Platforms provide a weak default definition of this symbol. Applications can
 * override it by defining their own strong usipy_platform_handlers object.
 */
extern const struct usipy_platform_handlers usipy_platform_handlers;

uint64_t usipy_platform_mono_ms(void);
void usipy_platform_sleep_until_ms(uint64_t);
int usipy_platform_random_fill(void *, size_t);
void usipy_platform_log_vwrite(int, const char *, const char *, va_list);
void usipy_log_write(int, const char *, const char *, ...) \
  __attribute__ ((format (printf, 3, 4)));
