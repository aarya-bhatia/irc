#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>



int main()
{
	int epollfd = epoll_create1(0);

	struct epoll_event events[10];

	struct itimerspec tv = {
		.it_interval.tv_sec = 3,
		.it_interval.tv_nsec = 0,
		.it_value.tv_sec = 3,
		.it_value.tv_nsec = 0
	};

	int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);

	struct epoll_event ev = { .data.fd = timerfd, .events = EPOLLIN };
	epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &ev); 

	if(timerfd_settime(timerfd, 0, &tv, NULL) != 0) {
		printf("Failed to start timer!");
		exit(1);
	}

	printf("Start time: %d\n", (int)time(NULL));

	while(1)
	{
		int n = epoll_wait(epollfd, events, sizeof events, 2000);
		if(n == 0) { continue; }

		uint64_t t = 0;
		read(timerfd, &t, 8);
		printf("Expired at %d: %d\n", (int)time(NULL), t);
	}
	
	return 0;
}
