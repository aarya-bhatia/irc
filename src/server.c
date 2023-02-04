#include "server.h"
#include <sys/epoll.h>

static Server *server = NULL;

void do_NICK(Server *serv, User *usr, Message *msg, char **response)
{
	assert(serv);
	assert(usr);
	assert(msg);

	if(msg->n_params != 1)
	{
		asprintf(response, "%s INVALID_PARAMS\r\n", serv->hostname);
		return;
	}

	char *p0 = msg->params[0];
	assert(p0);

	usr->nick = strdup(p0);
	log_debug("user %d nick set to %s", usr->fd, usr->nick);

	asprintf(response, "%s OK\r\n", serv->hostname);
}

void do_USER(Server *serv, User *usr, Message *msg, char **response)
{
	assert(serv);
	assert(usr);
	assert(msg);

	if(msg->n_params != 3)
	{
		asprintf(response, "%s INVALID_PARAMS\r\n", serv->hostname);
		return;
	}

	if(!msg->body)
	{
		asprintf(response, "%s ERR_NO_NAME_FOUND\r\n", serv->hostname);
		return;
	}

	char *p0 = msg->params[0];
	assert(p0);

	if(!usr->nick || strcmp(usr->nick, p0))
	{
		free(usr->nick);
		usr->nick = strdup(p0);
	}

	usr->name = strdup(msg->body);

	log_debug("user %d nick=%s name=%s", usr->fd, usr->nick, usr->name);
	asprintf(response, "%s OK\r\n", serv->hostname);
	
}

void do_PRIVMSG(Server *serv, User *usr, Message *msg, char **response);

void user_destroy(User *usr)
{
	if (!usr)
	{
		return;
	}

	free(usr->nick);
	free(usr->name);

	cc_array_destroy(usr->inbox);
	cc_array_destroy(usr->outbox);

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
	close(serv->epollfd);
	free(serv->hostname);
	free(serv->port);
	free(serv);

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

	serv->hostname = strdup(addr_to_string((struct sockaddr *) &serv->servaddr, sizeof(serv->servaddr)));
	asprintf(&serv->port, "%d", port);

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

void accept_new_connections(Server *serv)
{
	while (1)
	{
		int conn_sock = accept(serv->fd, NULL, NULL);

		if (conn_sock == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return;
			}

			die("accept");
		}

		User *user = calloc(1, sizeof(User));
		user->fd = conn_sock;

		if (cc_array_new(&user->inbox) != CC_OK)
		{
			log_error("Failed to create CC_Array");
			user_destroy(user);
			continue;
		}

		if (cc_array_new(&user->outbox) != CC_OK)
		{
			log_error("Failed to create CC_Array");
			user_destroy(user);
			continue;
		}

		if (fcntl(conn_sock, F_SETFL, fcntl(conn_sock, F_GETFL) | O_NONBLOCK) != 0)
		{
			perror("fcntl");
			user_destroy(user);
			continue;
		}

		struct epoll_event ev = {.data.fd = conn_sock, .events = EPOLLIN | EPOLLOUT};

		if (epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, conn_sock, &ev) != 0)
		{
			perror("epoll_ctl");
			user_destroy(user);
			continue;
		}

		int *key = calloc(1, sizeof(int));

		if (!key)
		{
			perror("calloc");
			user_destroy(user);
			continue;
		}

		*key = user->fd;

		if (cc_hashtable_add(serv->connections, (void *)key, user) != CC_OK)
		{
			perror("cc_hashtable_add");
			user_destroy(user);
			continue;
		}

		log_info("Got connection: %d", conn_sock);
	}
}

void client_read_event(Server *serv, User *usr)
{
	assert(usr);
	assert(serv);

	ssize_t nread = read_all(usr->fd, usr->req_buf + usr->req_len, MAX_MSG_LEN - usr->req_len);

	if(nread == 0) 
	{
		return;
	}

	if (nread == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return;
		}

		client_disconnect(serv, usr);
	}

	usr->req_len += nread;
	usr->req_buf[usr->req_len] = 0;

	log_debug("Read %zd bytes from fd %d", nread, usr->fd);

	if (strstr(usr->req_buf, "\r\n"))
	{
		// Process request
		CC_Array *messages = parse_all_messages(usr->req_buf);

		CC_ArrayIter itr;
		cc_array_iter_init(&itr, messages);

		Message *message;

		while(cc_array_iter_next(&itr, (void **) &message) != CC_ITER_END)
		{
			char *response;

			if(message->command) 
			{
				if(!strncmp(message->command, "NICK", strlen("NICK")))
				{
					do_NICK(serv, usr, message, &response);
				}
				else if(!strncmp(message->command, "USER", strlen("USER")))
				{
					do_USER(serv, usr, message, &response);
				}
				else 
				{
					asprintf(&response, "%s INVALID\r\n", serv->hostname);
				}

				cc_array_add(usr->outbox, response);
				log_debug("New message added for user %d", usr->fd);
			}

			message_destroy(message);
			free(message);
		}

		cc_array_destroy(messages);

		usr->req_len = 0;
		usr->req_buf[0] = 0;
	}
}

void client_write_event(Server *serv, User *usr)
{
	assert(usr);
	assert(serv);

	// Message is still being sent
	if(usr->res_off < usr->res_len)
	{
		ssize_t nsent = write_all(usr->fd, usr->res_buf + usr->res_off, usr->res_len - usr->res_off);

		log_debug("Sent %zd bytes to user %d", nsent, usr->fd);

		if(nsent == 0) 
		{
			return;
		}

		if (nsent == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return;
			}

			client_disconnect(serv, usr);
		}

		// Entire message was sent
		if(usr->res_off >= usr->res_len)
		{
			usr->res_off = usr->res_len = 0;
		}
	}

	// Check for pending messages
	
	if(cc_array_size(usr->outbox) > 0)
	{
		char *msg;
		if(cc_array_remove_last(usr->outbox, (void **) &msg) == 0)
		{
			strcpy(usr->res_buf, msg);
			usr->res_len = strlen(msg);
			usr->res_off = 0;
			free(msg);

			client_write_event(serv, usr);
		}
	}
}


void client_disconnect(Server *serv, User *usr)
{
	assert(serv);
	assert(usr);

	log_info("Closing connection with user=%s fd=%d", usr->nick, usr->fd);
	epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, usr->fd, NULL);
	cc_hashtable_remove(serv->connections, (void *)&usr->fd, NULL);
	user_destroy(usr);
}

void sighandler(int sig)
{
	if (sig == SIGINT)
	{
		quit(server);
	}
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

	server = serv;

	struct epoll_event events[MAX_EVENTS] = {0};

	struct epoll_event ev = {.events = EPOLLIN, .data.fd = serv->fd};

	CHECK(epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, serv->fd, &ev), "epoll_ctl");

	signal(SIGINT, sighandler);

	while (1)
	{
		int num = epoll_wait(serv->epollfd, events, MAX_EVENTS, -1);
		CHECK(num, "epoll_wait");

		for (int i = 0; i < num; i++)
		{
			if (events[i].data.fd == serv->fd)
			{
				accept_new_connections(serv);
			}
			else
			{
				int e = events[i].events;
				int fd = events[i].data.fd;

				User *usr;

				if (cc_hashtable_get(serv->connections, (void *)&fd, (void **)&usr) != CC_OK)
				{
					epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, fd, NULL);
					continue;
				}

				if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
				{
					client_disconnect(serv, usr);
					continue;
				}

				if (e & EPOLLIN)
				{
					client_read_event(serv, usr);
				}

				if (e & EPOLLOUT)
				{
					client_write_event(serv, usr);
				}
			}
		}
	}

	cc_hashtable_destroy(serv->connections);

	return 0;
}
