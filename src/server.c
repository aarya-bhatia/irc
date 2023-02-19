#include "include/server.h"
#include "include/replies.h"
#include <time.h>
#include <sys/stat.h>

void Server_process_request(Server *serv, User *usr)
{
	assert(serv);
	assert(usr);
	assert(strstr(usr->req_buf, "\r\n"));

	// Get array of parsed messages
	CC_Array *messages = parse_all_messages(usr->req_buf);
	assert(messages);

	log_debug("Processing %zu messages from user %s", cc_array_size(messages), usr->nick);

	usr->req_buf[0] = 0;
	usr->req_len = 0;

	CC_ArrayIter itr;
	cc_array_iter_init(&itr, messages);

	Message *message = NULL;

	// Iterate over the request messages and add response message(s)
	// to user's message queue in the same order.
	while (cc_array_iter_next(&itr, (void **)&message) != CC_ITER_END)
	{
		assert(message);

		if (message->command)
		{
			if(!strcmp(message->command, "MOTD"))
			{
				Server_reply_to_MOTD(serv, usr, message);
			}
			else if (!strcmp(message->command, "NICK"))
			{
				Server_reply_to_NICK(serv, usr, message);
			}
			else if (!strcmp(message->command, "USER"))
			{
				Server_reply_to_USER(serv, usr, message);
			}
			else if (!strcmp(message->command, "PING"))
			{
				Server_reply_to_PING(serv, usr, message);
			}
			else if (!strcmp(message->command, "QUIT"))
			{
				Server_reply_to_QUIT(serv, usr, message);
				assert(cc_list_size(usr->msg_queue) > 0);
				usr->quit = true;
			}
			else if (!usr->registered)
			{
				User_add_msg(usr, make_reply(ERR_NOTREGISTERED_MSG, usr->nick));
			}
			else
			{
				User_add_msg(usr, make_reply(ERR_UNKNOWNCOMMAND_MSG, usr->nick, message->command));
			}
		}

		message_destroy(message);
		free(message);
	}

	cc_array_destroy(messages);
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

		cc_hashtable_iter_remove(&itr, (void **)&usr);

		free(key);
		User_Destroy(usr);
	}

	cc_hashtable_destroy(serv->connections);

	free(serv->motd);
	close(serv->fd);
	close(serv->epollfd);
	free(serv->hostname);
	free(serv->port);
	free(serv);

	log_debug("Server stopped");
	exit(0);
}

char *_get_motd(char *fname)
{
	FILE *motd_file = fopen(fname, "r");
	char *res = NULL;
	size_t res_len = 0;

	if (!motd_file)
	{
		log_warn("failed to open %s", fname);
	}
	else
	{
		size_t num_lines = 1;

		// count number of lines
		for (int c = fgetc(motd_file); c != EOF; c = fgetc(motd_file))
		{
			if (c == '\n')
			{
				num_lines = num_lines + 1;
			}
		}

		fseek(motd_file, 0, SEEK_SET); // go to beginning

		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		char *mday = make_string("%d", tm.tm_mday); // day of month
		int mday_int = atoi(mday);
		free(mday);

		size_t line_no = mday_int % num_lines; // select a line from file to use

		for (size_t i = 0; i < line_no + 1; i++)
		{
			if (getline(&res, &res_len, motd_file) == -1)
			{
				perror("getline");
				break;
			}
		}
	}

	if(res && res[res_len-1] == '\n')
	{
		res[res_len-1] = 0;
	}

	fclose(motd_file);

	return res;
}

/**
 * Create and initialise the server. Bind socket to given port.
 */
Server *Server_create(int port)
{
	Server *serv = calloc(1, sizeof *serv);
	assert(serv);

	// TCP Socket non-blocking
	serv->fd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	CHECK(serv->fd, "socket");

	// Server Address
	serv->servaddr.sin_family = AF_INET;
	serv->servaddr.sin_port = htons(port);
	serv->servaddr.sin_addr.s_addr = INADDR_ANY;
	serv->hostname = strdup(addr_to_string((struct sockaddr *)&serv->servaddr, sizeof(serv->servaddr)));
	serv->port = make_string("%d", port);
	serv->motd = _get_motd("motd.txt");

	if(serv->motd){
		log_info("MOTD: %s", serv->motd);
	}

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	size_t n = strftime(serv->created_at, sizeof(serv->created_at), "%c", tm);
	assert(n > 0);

	int yes = 1;

	// Set socket options
	CHECK(setsockopt(serv->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes), "setsockopt");

	// Bind
	CHECK(bind(serv->fd, (struct sockaddr *)&serv->servaddr, sizeof(struct sockaddr_in)), "bind");

	// Listen
	CHECK(listen(serv->fd, MAX_EVENTS), "listen");

	// Create epoll fd for listen socket and clients
	serv->epollfd = epoll_create(1 + MAX_EVENTS);
	CHECK(serv->epollfd, "epoll_create");

	// Hashtable settings
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
	struct sockaddr_storage client_addr;
	socklen_t addrlen = sizeof(client_addr);

	while (1)
	{
		int conn_sock = accept(serv->fd, (struct sockaddr *)&client_addr, &addrlen);

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
		user->hostname = strdup(addr_to_string((struct sockaddr *)&client_addr, addrlen));
		user->nick = make_string("user%05d", (rand() % (int)1e5));
		user->registered = false;
		user->nick_changed = false;
		user->quit = false;

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

		log_info("Got connection %d from %s", conn_sock, user->hostname);
	}
}
