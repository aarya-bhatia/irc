
#include <signal.h>
#include <sys/epoll.h>

#include "include/server.h"

static volatile bool g_alive = true;

void sighandler(int sig);

/**
 * To start IRC server on given port
 */
int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <name>", *argv);
		return 1;
	}
	// Create and start an IRC server on given port
	Server *serv = Server_create(argv[1]);

	// Array for events returned from epoll
	struct epoll_event events[MAX_EVENTS] = {0};

	// Event for listener socket
	struct epoll_event ev = {.events = EPOLLIN, .data.fd = serv->fd};

	CHECK(epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, serv->fd, &ev), "epoll_ctl");

	// Setup signal handler to stop server
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sighandler;
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGINT, &sa, NULL) == -1)
		die("sigaction");

	if (sigaction(SIGPIPE, &sa, NULL) == -1)
		die("sigaction");

	// Run while g_alive flag is set
	while (g_alive)
	{
		int num = epoll_wait(serv->epollfd, events, MAX_EVENTS, -1);

		if (num == -1)
		{
			perror("epoll_wait");
			g_alive = false;
			break;
		}

		for (int i = 0; i < num; i++)
		{
			if (events[i].data.fd == serv->fd)
			{
				Server_accept_all(serv);
			}
			else
			{
				int e = events[i].events;
				int fd = events[i].data.fd;

				Connection *connection = ht_get(serv->connections, &fd);

				if (!connection)
				{
					epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, fd, NULL);
					continue;
				}

				if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
				{
					Server_remove_connection(serv, connection);
					continue;
				}

				if (e & EPOLLIN)
				{
					if (Connection_read(connection) == -1)
					{
						Server_remove_connection(serv, connection);
						continue;
					}

					Server_process_request(serv, connection);
				}

				if (e & EPOLLOUT)
				{
					if (Connection_write(connection) == -1)
					{
						Server_remove_connection(serv, connection);
						continue;
					}
				}

				if (connection->quit && List_size(connection->outgoing_messages) == 0 && connection->res_len == 0)
				{
					Server_remove_connection(serv, connection);
					continue;
				}
			}
		}
	}

	Server_destroy(serv);

	return 0;
}

void sighandler(int sig)
{
	if (sig == SIGINT)
	{
		g_alive = false;
	}
}
