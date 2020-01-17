#include "usipy_str.h"

static const char *sip_version = "SIP/2.0";
#define CHLOWER(ch) ((ch) | 32)
#define CHCASECMP(ch1, ch2) ((ch1) == (ch2) || (ch1) == CHLOWER(ch2))

#define chk_sip_version(ch) ( \
  CHCASECMP((ch)[0], sip_version[0]) && \
  CHCASECMP((ch)[1], sip_version[1]) && \
  CHCASECMP((ch)[2], sip_version[2]) && \
  (ch)[3] == sip_version[3] && \
  (ch)[4] == sip_version[4] && \
  (ch)[5] == sip_version[5] && \
  (ch)[6] == sip_version[6] \
)

int
usipy_verify_sip_version(const usipy_str *vp)
{

    return (vp->l == 7 && chk_sip_version(vp->s.ro));
}
