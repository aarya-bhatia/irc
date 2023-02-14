#include "include/timer.h"

void Timer_init(Timer *t, int duration_sec)
{
    struct itimerspec tv;

    tv.it_interval.tv_sec = duration_sec;
    tv.it_interval.tv_nsec = 0;
    tv.it_value.tv_sec = duration_sec;
    tv.it_value.tv_nsec = 0;

    t->timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    t->prev = 0;

    if (timerfd_settime(t->timerfd, 0, &tv, NULL) != 0)
        die("timerfd_settime");

    CHECK(t->timerfd, "create_timer");
}

bool Timer_expired(Timer *t)
{
    uint64_t val = 0;
    int ret = read(t->timerfd, &val, 8);

    if (ret == -1)
    {
        perror("read");
        return false;
    }

    log_debug("Timer expired at %d", (int)time(NULL));

    if (ret > 0)
    {
        t->prev = time(NULL);
    }

    return ret > 0;
}

void Timer_stop(Timer *t)
{
    close(t->timerfd);
}