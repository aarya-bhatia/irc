#include "include/common.h"
#include "include/message.h"
#include "include/iostream.h"
#include "include/timer.h"
#include "include/collectc/cc_list.h"
#include <sys/epoll.h>

#define DEBUG
#define PING_INTERVAL_SEC 3
#define EPOLL_TIMEOUT 1000
#define MAX_EPOLL_EVENTS 3 // stdin, socket, timer

void read_user_input(char**,size_t*);
volatile bool g_is_running = true;

void _signal_handler(int sig)
{
	(void)sig;
	g_is_running = false;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
		return 1;
	}
	char *host = argv[1];
	char *port = argv[2];

	char client_nick[30] = {0};
	char client_username[30] = {0};
	char client_realname[30] = {0};

	char *line = NULL;
	size_t line_len = 0;

	IOStream stream;
	Timer t;

	// Array to store the returned events from epoll_wait
	struct epoll_event events[5];

	IOStream_open(&stream, create_and_bind_socket(host, port));

	// Create fd for epoll
	int epoll_fd = epoll_create1(0);
	CHECK(epoll_fd, "epoll_create1");

	// Create a timer fd to send PING messages
	Timer_init(&t, PING_INTERVAL_SEC);

	// Add all fds to epoll set
	struct epoll_event ev;

	ev.data.fd = 0;
	ev.events = EPOLLIN;
	CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &ev), "epoll_ctl");

	ev.data.fd = t.timerfd;
	ev.events = EPOLLIN;
	CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, t.timerfd, &ev), "epoll_ctl");

	ev.data.fd = stream.fd;
	ev.events = EPOLLIN | EPOLLOUT;
	CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stream.fd, &ev), "epoll_ctl");

	// Register signal handler to quit on CTRL-C
	signal(SIGINT, _signal_handler);

	// Register the client in IRC network
#ifdef DEBUG
	strcpy(client_nick, "aaryab2");
	strcpy(client_username, "aarya.bhatia1678");
	strcpy(client_realname, "Aarya Bhatia");

#else
	char *msg1 = "Enter your nick > ";
	char *msg2 = "Enter your username > ";
	char *msg3 = "Enter your realname > ";

	write(1, msg1, strlen(msg1));
	scanf("%s", client_nick);

	write(1, msg2, strlen(msg2));
	scanf("%s", client_username);

	write(1, msg3, strlen(msg3));
	scanf("%s", client_realname);
#endif

	char *initial_requests = NULL;
	asprintf(&initial_requests, "NICK %s\r\nUSER %s * * :%s\r\n", client_nick, client_username, client_realname);
	IOStream_enqueue(&stream, initial_requests);

	while (g_is_running)
	{
		// Poll event set
		int nfd = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);

		if (nfd == -1)
		{
			perror("epoll_wait");
			exit(1);
		}
		if (nfd == 0)
		{
			log_debug("No events occurred");
			continue;
		}

		// Check for events
		for (size_t i = 0; i < MAX_EPOLL_EVENTS; i++)
		{
			int fd = events[i].data.fd;
			int event = events[i].events;

			// stdin event
			if (fd == 0 && event & EPOLLIN)
			{
				// Read the next line till \n
				read_user_input(&line, &line_len);
				assert(line);
				IOStream_enqueue(&stream, strdup(line));
			}

			// timer event
			else if (fd == t.timerfd && event & EPOLLIN)
			{
				// check time since last message sent
				// Only ping if idle
				if (Timer_expired(&t) && stream.write_buf_len == 0 && difftime(time(NULL), stream.last_write_time) > PING_INTERVAL_SEC)
				{
					// Make ping request
					IOStream_enqueue(&stream, strdup("PING\r\n"));
					sleep(1);
				}
			}

			// socket event
			else if (fd == stream.fd)
			{
				if (event & (EPOLLERR | EPOLLHUP))
				{
					die("Server disconnected");
				}

				// socket is ready to send data i.e. client can read
				if (event & EPOLLIN)
				{
					IOStream_read(&stream);
				}

				// socket is ready to recieve data i.e. client can write data
				if (event & EPOLLOUT)
				{
					IOStream_write(&stream);
				}
			}
		}
	}

	free(line);

	close(epoll_fd);

	Timer_stop(&t);

	return 0;
}

void read_user_input(char **line, size_t *line_len)
{
	ssize_t nread = getline(line, line_len, stdin);

	if (nread == -1)
		die("getline");

	(*line)[nread] = 0;

	strcpy(*line + nread - 1, "\r\n");

	log_debug("%ld bytes read from stdin", nread);
}
