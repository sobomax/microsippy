#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>

#include "usipy_stdout_watch.h"

void *
usipy_stdout_watch(void *tap)
{
    struct pollfd pfds[1];
    int r;

    pfds[0].fd = fileno(stdout);
    pfds[0].events = POLLHUP | POLLIN;
    do {
        r = poll(pfds, 1, INFTIM);
    } while (r == 0 || (r < 0 && errno == EINTR));
    exit(0);
}
