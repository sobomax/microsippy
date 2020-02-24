#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>

#include "usipy_types.h"
#include "usipy_sip_tm.h"

#define EXAMPLE_SIP_PORT 5060

int
main(int argc, const char **argv)
{
    struct usipy_sip_tm_conf stc = {
      .sip_port = EXAMPLE_SIP_PORT,
      .log_tag = "usipy_test",
#ifdef CONFIG_EXAMPLE_IPV6
      .sip_af = AF_INET6
#else
      .sip_af = AF_INET
#endif
    };
    pthread_t tmthread;

    pthread_create(&tmthread, NULL, usipy_sip_tm_task, &stc);
    return (pthread_join(tmthread, NULL));
}
