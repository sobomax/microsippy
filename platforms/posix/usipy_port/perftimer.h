
struct timer_opduration {
    uint64_t bts;
    uint64_t ets;
    const char *dunit;
};

static inline void
timer_opbegin(struct timer_opduration *odp)
{

#if 0
    odp->bts = timer1_read();
#endif
}

static inline uint32_t
timer_opend(struct timer_opduration *odp)
{
    odp->dunit = "ns";
#if 0
    uint32_t r;

    odp->ets = timer1_read();
    if (odp->ets <= odp->bts) {
        r = odp->bts - odp->ets;
    } else {
        r = (uint32_t)T1VMAX - odp->ets + odp->bts + 1;
    }
    return (r / 80);
#else
    return (0);
#endif
}
