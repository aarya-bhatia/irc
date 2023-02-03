#include "server.h"
#include <sys/epoll.h>

/*
void do_NICK(int fd, Message *msg);
void do_USER(int fd, Message *msg);
void do_PRIVMSG(int fd, Message *msg);
*/

void user_destroy(User *usr)
{
	if (!usr)
	{
		return;
	}

	free(usr->nick);
	free(usr->name);

	shutdown(usr->fd, SHUT_RDWR);
	close(usr->fd);
}

void quit(Server *serv)
{
	assert(serv);
	CC_HashTableIter itr;
	cc_hashtable_iter_init(&itr, serv->connections);

	TableEntry *elem;

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END)
	{
		int *key = elem->key;
		User *usr = (User *)elem->value;

		log_debug("Key: %d", *key);

		cc_hashtable_iter_remove(&itr, (void **)&usr);

		free(key);
		user_destroy(usr);
	}

	log_debug("Hashtable destroyed: size=%zu", cc_hashtable_size(serv->connections));

	close(serv->fd);
	log_debug("Server stopped");
	;
	exit(0);
}

Server *start_server(int port)
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
	CHECK(bind(serv->fd, (struct sockaddr *)&serv->servaddr, sizeof(struct sockaddr_in)), "bind");

	// Listen
	CHECK(listen(serv->fd, MAX_EVENTS), "listen");

	int yes = 1;

	// Set socket options
	CHECK(setsockopt(serv->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes), "setsockopt");

	// Create epoll fd
	serv->epollfd = epoll_create(MAX_EVENTS);
	CHECK(serv->epollfd, "epoll_create");

	// Make server non blocking
	CHECK(fcntl(serv->fd, F_SETFL, fcntl(serv->fd, F_GETFL) | O_NONBLOCK), "fcntl");

	CC_HashTableConf htc;

	// Initialize all fields to default values
	cc_hashtable_conf_init(&htc);
	htc.hash = GENERAL_HASH;
	htc.key_length = sizeof(int);
	htc.key_compare = int_compare;

	// Create a new HashTable with integer keys
	if (cc_hashtable_new_conf(&htc, &serv->connections) != CC_OK)
	{
		log_error("Failed to create hashtable");
		exit(1);
	}

	log_info("Hashtable initialized with capacity %zu", cc_hashtable_capacity(serv->connections));

	log_info("server running on port %d", port);

	return serv;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s port", *argv);
		return 1;
	}

	int port = atoi(argv[1]);

	Server *serv = start_server(port);

	struct epoll_event events[MAX_EVENTS] = {0};

	struct epoll_event ev = {.events = EPOLLIN, .data.fd = serv->fd};
	CHECK(epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, serv->fd, &ev), "epoll_ctl");

	while (1)
	{
		int num = epoll_wait(serv->epollfd, events, MAX_EVENTS, -1);
		CHECK(num, "epoll_wait");

		for (int i = 0; i < num; i++)
		{
			// new connections are available
			if (events[i].data.fd == serv->fd)
			{
				while (1)
				{
					int conn_sock = accept(serv->fd, NULL, NULL);

					if (conn_sock == -1)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
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

					int *key = calloc(1, sizeof *key);
					*key = conn_sock;

					cc_hashtable_add(serv->connections, (void *)key, user);

					log_info("Got connection: %d", conn_sock);
				}
			}
			else
			{
				int e = events[i].events;
				int fd = events[i].data.fd;

				if (e & (EPOLLERR | EPOLLHUP))
				{
					User *usr;

					cc_hashtable_remove(serv->connections, (void *)&fd, (void **)&usr);

					assert(usr->fd == fd);

					log_info("Connection closed with user=%s fd=%d", usr->nick, usr->fd);
					user_destroy(usr);
				}

				// TODO

				if (e & EPOLLIN)
				{
				}

				if (e & EPOLLOUT)
				{
				}
			}
		}
	}

	return 0;
}
