#pragma once
#include "common.h"
#include <sys/timerfd.h>
#include <time.h>

typedef struct Timer
{
    int timerfd;
    time_t prev;
} Timer;

void Timer_init(Timer *t, int duration_sec);
bool Timer_expired(Timer *t);
void Timer_stop(Timer *t);

