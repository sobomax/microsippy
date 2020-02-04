#ifndef _USIPY_DEBUG_H
#define _USIPY_DEBUG_H

#if 0
# include <assert.h>

# define USIPY_DASSERT(x) assert(x)
# define USIPY_DCODE(code) code
#else
# define USIPY_DASSERT(x)
# define USIPY_DCODE(code)
#endif

#endif /* _USIPY_DEBUG_H */
