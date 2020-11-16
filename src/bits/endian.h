#if !defined(__FreeBSD__)
#define _BSD_SOURCE             /* See feature_test_macros(7) */
#include <endian.h>
#else
#include <sys/endian.h>
#endif

#if USIPY_BIGENDIAN
#  warning Platform is big-endian.
#  define HTOLE32(sp) htole32(sp)
#else
#  warning Platform is little-endian.
#  define HTOLE32(sp) (sp)/* Nop */
#endif
