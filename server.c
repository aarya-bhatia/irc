#include "server.h"

/*
void do_NICK(int fd, Message *msg);
void do_USER(int fd, Message *msg);
void do_PRIVMSG(int fd, Message *msg);
*/

static struct epoll_event events[MAX_EVENTS];

Server *start_server(int port);
{
	Server *serv = calloc(1, sizeof *serv);
	assert(serv);

	// TCP Socket
	serv->fd = socket(PF_INET, SOCK_STREAM, 0);
	CHECK(serv->fd, "socket");

	// Server Address
	serv->servaddr.sin_family = AF_INET;
	serv->servaddr.sin_port = htons(port);
	serv->servaddr.sin_addr.s_addr = INADDR_ANY;

	// Bind
	CHECK(bind(serv->fd, (struct sockaddr *) &serv->servaddr, sizeof(struct sockaddr_in)),
			"bind");

	// Listen
	CHECK(listen(server_fd, MAX_EVENTS), "listen");

	int yes = 1;

	// Set socket options
	CHECK(setsockopt(serv->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes), "setsockopt");
	CHECK(setsockopt(serv->fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof yes), "setsockopt");

	// Create epoll fd
	serv->epollfd = epoll_create(0);
	CHECK(epollfd, "epoll_create");

	// Make server non blocking
	CHECK(fcntl(serv->fd, F_SETFL, fcntl(serv->fd, F_GETFL) | O_NONBLOCK), "fcntl");

	log_info("server running on port %d", port);
}

int main(int argc, char *argv[])
{
	if(argc != 2) 
	{
		fprintf(stderr, "Usage: %s port", *argv);
		return 1;
	}

	int port = atoi(argv[1]);

	Server *serv = start_server(port);

	struct epoll_event ev = { .events = EPOLLIN, .data.fd = serv->fd };
	CHECK(epoll_ctl(epollfd, EPOLL_CTL_ADD, serv->fd, &ev), "epoll_ctl");

	while(1)
	{
		int num = epoll_wait(epollfd, serv->events, MAX_EVENTS, -1);
		CHECK(num, "epoll_wait");

		for(size_t i = 0; i < num; i++)
		{
			// new connections are available
			if(events[i].data.fd == serv->fd)
			{
				while(1)
				{
					int conn_sock = accept(serv->fd, NULL, NULL);
					
					if(conn_sock == -1)
					{
						if(errno == EAGAIN || errno == EWOULDBLOCK) {
							break;
						}

						perror("accept");
						break;
					}

					User *user = calloc(1, sizeof *user);
					user->fd = conn_sock;

					// TODO: Do error handling

					fcntl(conn_sock, F_SETFL, fcntl(conn_sock, F_GETFL) | O_NONBLOCK);
					ev.events = EPOLLIN | EPOLLOUT;
					ev.data.fd = conn_sock;

					epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, conn_sock, &ev);

					cc_hashtable_add(serv->connections, conn_sock, user);

					log_info("Got connection: %d", conn_sock);

				}
			}
			else
			{
				int fd = events[i].data.fd;
				log_info("Event on %d", fd);
			}
		}
	}

	return 0;
}
