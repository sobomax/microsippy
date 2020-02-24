#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "usipy_port/log.h"

#include "usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_cseq.h"
#include "usipy_sip_method_db.h"
#include "usipy_sip_method_types.h"
#include "usipy_sip_tid.h"

static inline uint32_t
ROTL32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> ((sizeof(x) * 8) - r));
}

static inline uint32_t
getblock32(const uint32_t *p, int i)
{
    uint32_t r;

    memcpy(&r, &p[i], sizeof(r));
    return (r);
}

static inline uint32_t
fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

static void
MurmurHash3_32(const void *key, size_t len, uint32_t seed, uint32_t *out)
{
  const uint8_t * data = (const uint8_t*)key;
  const int nblocks = len / 4;

  uint32_t h1 = seed;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  //----------
  // body

  const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

  for(int i = -nblocks; i; i++)
  {
    uint32_t k1 = getblock32(blocks,i);

    k1 *= c1;
    k1 = ROTL32(k1,15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1,13);
    h1 = h1*5+0xe6546b64;
  }

  //----------
  // tail

  const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

  uint32_t k1 = 0;

  switch(len & 3)
  {
  case 3: k1 ^= tail[2] << 16;
  case 2: k1 ^= tail[1] << 8;
  case 1: k1 ^= tail[0];
          k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len;

  h1 = fmix32(h1);

  *out = h1;
}

#define DUMP_STR(sname) \
    USIPY_LOGI(log_tag, "%s" #sname " = \"%.*s\"", log_pref, \
      USIPY_SFMT(tp->sname))
#define DUMP_UINT(sname) \
    USIPY_LOGI(log_tag, "%s" #sname " = 0x%.8x", log_pref, tp->sname)

void
usipy_sip_tid_dump(const struct usipy_sip_tid *tp, const char *log_tag,
  const char *log_pref)
{
    const union usipy_sip_hdr_parsed up = {.cseq = tp->cseq};

    DUMP_STR(call_id);
    DUMP_STR(from_tag);
    DUMP_STR(vbranch);
    usipy_sip_hdr_cseq_dump(&up, log_tag, log_pref, "cseq");
    DUMP_UINT(hash);
}

#define HASH_STR(sp, sd, ot) MurmurHash3_32((sp)->s.ro, (sp)->l, sd, ot)

uint32_t
usipy_sip_tid_hash(const struct usipy_sip_tid *tp)
{
    uint32_t rval;

    HASH_STR(tp->call_id, 0, &rval);
    USIPY_LOGI("foobar1", "%u", rval);
    HASH_STR(tp->from_tag, rval, &rval);
    USIPY_LOGI("foobar2", "%u", rval);
    HASH_STR(tp->vbranch, rval, &rval);
    USIPY_LOGI("foobar3", "%u", rval);
    if (tp->cseq->method->cantype == USIPY_SIP_METHOD_generic) {
        HASH_STR(&tp->cseq->onwire.method, rval, &rval);
    } else {
        MurmurHash3_32(&tp->cseq->method->cantype, sizeof(tp->cseq->method->cantype),
          rval, &rval);
    }
    USIPY_LOGI("foobar4", "%u", rval);
    MurmurHash3_32(&tp->cseq->val, sizeof(&tp->cseq->val), rval, &rval);
    USIPY_LOGI("foobar5", "%u", rval);
    return (rval);
}
