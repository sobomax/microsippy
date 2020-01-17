struct usipy_str {
    union {
        const char *ro;
        char *rw;
    } s;
    size_t l;
};

struct usipy_str_ro {
    union {
        const char *ro;
    } s;
    size_t l;
};

int usipy_str_split(const struct usipy_str *, unsigned char,
  struct usipy_str *, struct usipy_str *);
int usipy_str_atoui_range(const struct usipy_str *, unsigned int *,
  unsigned int, unsigned int);

#define USIPY_SP       ' '
#define USIPY_CRLF     "\r\n"
#define USIPY_CRLF_LEN 2
#define USIPY_ISWS(ch) ((ch) == USIPY_SP || (ch) == '\t')
#define USIPY_ISLWS(ch) (USIPY_ISWS(ch) || (ch) == '\r' || (ch) == '\n')

#define usipy_str_trm_e(sp) \
    while ((sp)->l > 0 && USIPY_ISWS((sp)->s.ro[(sp)->l - 1])) { \
        (sp)->l -= 1; \
    }

#define usipy_str_trm_b(sp) \
    while ((sp)->l > 0 && USIPY_ISWS((sp)->s.ro[0])) { \
        (sp)->s.ro += 1; \
        (sp)->l -= 1; \
    }

#define usipy_str_ltrm_e(sp) \
    while ((sp)->l > 0 && USIPY_ISLWS((sp)->s.ro[(sp)->l - 1])) { \
        (sp)->l -= 1; \
    }

#define usipy_str_ltrm_b(sp) \
    while ((sp)->l > 0 && USIPY_ISLWS((sp)->s.ro[0])) { \
        (sp)->s.ro += 1; \
        (sp)->l -= 1; \
    }
