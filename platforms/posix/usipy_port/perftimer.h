#include <time.h>

struct timer_opduration {
    struct timespec bts;
    struct timespec ets;
    const char *dunit;
};

static inline void
timer_opbegin(struct timer_opduration *odp)
{

    clock_gettime(CLOCK_MONOTONIC, &odp->bts);
}

static inline unsigned long
timer_opend(struct timer_opduration *odp)
{
    unsigned long r;

    clock_gettime(CLOCK_MONOTONIC, &odp->ets);
    r = ((odp->ets.tv_sec - odp->bts.tv_sec) * 1000000000) +
      (odp->ets.tv_nsec - odp->bts.tv_nsec);
    odp->dunit = "ns";
    return (r);
}
