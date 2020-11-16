#if USIPY_BIGENDIAN
#  warning Platform is big-endian.
#  define LE32TOH(dp, sp) { \
    *(uint32_t *)(dp) = (uint32_t) \
     ((const char *)(sp))[0] | \
     ((const char *)(sp))[1] << 8 | \
     ((const char *)(sp))[2] << 16 | \
     ((const char *)(sp))[3] << 24; \
  }
#else
#  warning Platform is little-endian.
#  define LE32TOH(dp, sp) /* Nop */
#endif
