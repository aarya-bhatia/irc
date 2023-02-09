#include "server.h"
#include <sys/epoll.h>

static Server *g_server = NULL;

void sighandler(int sig);

/**
 * To start IRC server on given port
*/
int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s port", *argv);
		return 1;
	}

	int port = atoi(argv[1]);

	Server *serv = Server_create(port);

	g_server = serv;

	struct epoll_event events[MAX_EVENTS] = {0};

	struct epoll_event ev = {.events = EPOLLIN, .data.fd = serv->fd};

	CHECK(epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, serv->fd, &ev), "epoll_ctl");

	// signal(SIGINT, sighandler);

	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sighandler;
	sa.sa_flags = SA_RESTART;

	if (sigaction(SIGINT, &sa, NULL) == -1)
		die("sigaction");

	while (1)
	{
		int num = epoll_wait(serv->epollfd, events, MAX_EVENTS, -1);
		CHECK(num, "epoll_wait");

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

				User *usr;

				// Fetch user data from hashtable
				if (cc_hashtable_get(serv->connections, (void *)&fd, (void **)&usr) != CC_OK)
				{
					epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, fd, NULL);
					continue;
				}

				if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
				{
					log_debug("user%d disconnected", usr->fd);
					User_Disconnect(serv, usr);
					continue;
				}

				int ret;

				if (e & EPOLLIN)
				{
					if((ret = User_Read_Event(serv, usr)) == -1) { log_error("Error %d", ret); continue; }
				}

				if (e & EPOLLOUT)
				{
					if((ret = User_Write_Event(serv, usr)) == -1) { log_error("Error %d", ret); continue; }
				}
			}
		}
	}

	cc_hashtable_destroy(serv->connections);

	return 0;
}


/**
 * Close connection with user and free all memory associated with them.
 */
void User_Destroy(User *usr)
{
	if (!usr)
	{
		return;
	}

	free(usr->nick);
	free(usr->username);
	free(usr->realname);

	CC_ListIter iter;
	cc_list_iter_init(&iter, usr->msg_queue);
	char *value;
	while (cc_list_iter_next(&iter, (void **) &value) != CC_ITER_END)
	{
		free(value);
	}
	cc_list_destroy(usr->msg_queue);

	shutdown(usr->fd, SHUT_RDWR);
	close(usr->fd);
}

void Server_destroy(Server *serv)
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
		User_Destroy(usr);
	}

	cc_hashtable_destroy(serv->connections);

	close(serv->fd);
	close(serv->epollfd);
	free(serv->hostname);
	free(serv->port);
	free(serv);

	log_debug("Server stopped");
	exit(0);
}

/**
 * Create and initialise the server. Bind socket to given port.
 */
Server *Server_create(int port)
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

	serv->hostname = strdup(addr_to_string((struct sockaddr *)&serv->servaddr, sizeof(serv->servaddr)));
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

/**
 * There are new connections available
 */
void Server_accept_all(Server *serv)
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

		if (cc_list_new(&user->msg_queue) != CC_OK)
		{
			perror("CC_LIST");
			User_Destroy(user);
			continue;
		}

		if (fcntl(conn_sock, F_SETFL, fcntl(conn_sock, F_GETFL) | O_NONBLOCK) != 0)
		{
			perror("fcntl");
			User_Destroy(user);
			continue;
		}

		struct epoll_event ev = {.data.fd = conn_sock, .events = EPOLLIN | EPOLLOUT};

		if (epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, conn_sock, &ev) != 0)
		{
			perror("epoll_ctl");
			User_Destroy(user);
			continue;
		}

		int *key = calloc(1, sizeof(int));

		if (!key)
		{
			perror("calloc");
			User_Destroy(user);
			continue;
		}

		*key = user->fd;

		if (cc_hashtable_add(serv->connections, (void *)key, user) != CC_OK)
		{
			perror("cc_hashtable_add");
			User_Destroy(user);
			continue;
		}

		log_info("Got connection: %d", conn_sock);
	}
}

void Server_process_request(Server *serv, User *usr)
{
	assert(serv);
	assert(usr);
	assert(strstr(usr->req_buf, "\r\n"));

	CC_Array *messages = parse_all_messages(usr->req_buf);
	assert(messages);

	// buffer may contain partial messages
	usr->req_len = strlen(usr->req_buf);

	CC_ArrayIter itr;
	cc_array_iter_init(&itr, messages);

	Message *message = NULL;

	while (cc_array_iter_next(&itr, (void **)&message) != CC_ITER_END)
	{
		assert(message);

		char *response = NULL;

		if (message->command)
		{
			if (!strncmp(message->command, "NICK", strlen("NICK")))
			{
				response = Server_Nick_Command(serv, usr, message);
			}
			else if (!strncmp(message->command, "USER", strlen("USER")))
			{
				response = Server_User_Command(serv, usr, message);
			}
			else
			{
				asprintf(&response, "%s INVALID\r\n", serv->hostname);
			}

			cc_list_add_last(usr->msg_queue, response);

			log_debug("New message added for user %d: %s", usr->fd, response);
		}

		message_destroy(message);
		free(message);
	}

	cc_array_destroy(messages);

}

/**
 * User is available to send data to server
 */
ssize_t User_Read_Event(Server *serv, User *usr)
{
	assert(usr);
	assert(serv);

	ssize_t nread = read_all(usr->fd, usr->req_buf + usr->req_len, MAX_MSG_LEN - usr->req_len);

	if(nread == 0 && !strstr(usr->req_buf, "\r\n"))
	{
		User_Disconnect(serv, usr);
		return -1;	
	}

	if (nread == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return 0;
		}

		User_Disconnect(serv, usr);
		return -1;
	}

	usr->req_len += nread;
	usr->req_buf[usr->req_len] = 0;

	log_debug("Read %zd bytes from fd %d", nread, usr->fd);

	if (strstr(usr->req_buf, "\r\n"))
	{
		// Buffer to store partial message
		char tmp[MAX_MSG_LEN + 1];
		tmp[0] = 0;

		// Number of avail messages
		int count = 0;

		char *s = usr->req_buf, *t = NULL;

		// Find last "\r\n" and store in t
		while((s = strstr(s, "\r\n")) != NULL) 
		{
			count++;
			t = s;
			s += 2;
		}

		// Check if there is a partial message
		if(t - usr->req_buf > 2)
		{
			strcpy(tmp, t + 2); // Copy partial message to temp buffer
			t[2] = 0; // Shorten the request buffer to last complete message
		}

		log_info("Processing %d messages from user%d", count, usr->fd);

		// Process all CRLF-terminated messages from request buffer
		Server_process_request(serv, usr);

		// Copy pending message to request buffer
		strcpy(usr->req_buf, tmp);
		usr->req_len = strlen(tmp);

		log_info("Request buffer size for user%d: %zu", usr->fd, usr->req_len);
	}

	return nread;
}

/**
 * User is available to recieve data from server
 */
ssize_t User_Write_Event(Server *serv, User *usr)
{
	assert(usr);
	assert(serv);

	// Message is still being sent
	if (usr->res_len && usr->res_off < usr->res_len)
	{
		// Send remaining message to user
		ssize_t nsent = write_all(usr->fd, usr->res_buf + usr->res_off, usr->res_len - usr->res_off);

		log_debug("Sent %zd bytes to user %d", nsent, usr->fd);

		if (nsent == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return 0;
			}

			User_Disconnect(serv, usr);
			return -1;
		}

		usr->res_off += nsent;

		// Entire message was sent
		if (usr->res_off >= usr->res_len)
		{
			// Mark response buffer as empty
			usr->res_off = usr->res_len = 0;
		}

		return nsent;
	}

	// Check for pending messages
	if (cc_list_size(usr->msg_queue) > 0)
	{
		char *msg = NULL;

		// Dequeue one message and fill response buffer with message contents
		if (cc_list_remove_first(usr->msg_queue, (void **)&msg) == CC_OK)
		{
			strcpy(usr->res_buf, msg);
			usr->res_len = strlen(msg);
			usr->res_off = 0;
			free(msg);

			log_debug("user%d response buffer: %s", usr->fd, usr->res_buf);
		}
	}

	return 0;
}

void User_Disconnect(Server *serv, User *usr)
{
	assert(serv);
	assert(usr);

	log_info("Closing connection with user%d: %s", usr->fd, usr->nick);
	epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, usr->fd, NULL);
	cc_hashtable_remove(serv->connections, (void *)&usr->fd, NULL);
	User_Destroy(usr);
}

void sighandler(int sig)
{
	if (sig == SIGINT)
	{
		if (g_server)
		{
			Server_destroy(g_server);
		}
	}
}
