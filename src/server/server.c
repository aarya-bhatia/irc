#include "include/types.h"
#include "include/K.h"
#include "include/server.h"
#include "include/channel.h"
#include "include/user.h"
#include "include/replies.h"
#include "include/register.h"

#include <time.h>
#include <sys/stat.h>
#include <sys/epoll.h>

typedef void (*free_like)(void *);

void _sanity_check(Server *serv, User *usr, Message *msg)
{
	assert(serv);
	assert(usr);
	assert(msg);
}

void Server_process_request(Server *serv, User *usr)
{
	assert(serv);
	assert(usr);
	assert(strstr(usr->req_buf, "\r\n"));

	// Get array of parsed messages
	CC_Array *messages = parse_all_messages(usr->req_buf);
	assert(messages);

	log_debug("Processing %zu messages from user %s",
			  cc_array_size(messages), usr->nick);

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
			if (!strcmp(message->command, "MOTD"))
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
			else if (!strcmp(message->command, "PRIVMSG"))
			{
				Server_reply_to_PRIVMSG(serv, usr, message);
			}
			else if (!strcmp(message->command, "JOIN"))
			{
				Server_reply_to_JOIN(serv, usr, message);
			}
			else if (!strcmp(message->command, "QUIT"))
			{
				Server_reply_to_QUIT(serv, usr, message);
				assert(cc_list_size(usr->msg_queue) > 0);
				usr->quit = true;
			}
			else if (!usr->registered)
			{
				User_add_msg(usr,
							 make_reply(":%s " ERR_NOTREGISTERED_MSG,
										serv->hostname,
										usr->nick));
			}
			else
			{
				User_add_msg(usr,
							 make_reply(":%s " ERR_UNKNOWNCOMMAND_MSG,
										serv->hostname,
										usr->nick,
										message->command));
			}
		}

		message_destroy(message);
		free(message);
	}

	cc_array_destroy(messages);
}

/**
 * Copy nicks to file
 */
void write_nicks_to_file(Server *serv, char *filename)
{
	FILE *nick_file = fopen(filename, "w");

	if (!nick_file)
	{
		log_error("Failed to open nick file: %s", filename);
		return;
	}

	CC_HashTableIter itr;
	TableEntry *elem;

	cc_hashtable_iter_init(&itr, serv->user_to_nicks_map);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END)
	{
		fwrite(elem->key, 1, strlen(elem->key), nick_file);
		fputc(':', nick_file);

		CC_Array *nicks = elem->value;

		assert(nicks);

		char *nick = NULL;

		for (size_t i = 0; i < cc_array_size(nicks); i++)
		{
			if (cc_array_get_at(nicks, i, (void **)&nick) == CC_OK)
			{
				fwrite(nick, 1, strlen(nick), nick_file);
			}

			if (i + 1 < cc_array_size(nicks))
			{
				fputc(',', nick_file);
			}
		}

		fputc('\n', nick_file);
	}

	fclose(nick_file);

	log_info("Wrote nicks to file: %s", NICKS_FILENAME);
}

void Server_destroy(Server *serv)
{
	assert(serv);
	write_nicks_to_file(serv, NICKS_FILENAME);

	CC_HashTableIter itr;
	TableEntry *elem = NULL;

	/* Destroy server user_to_nicks_map */
	cc_hashtable_iter_init(&itr, serv->user_to_nicks_map);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END)
	{
		free(elem->key);
		elem->key = NULL;

		CC_Array *nicks = elem->value;
		cc_array_destroy_cb(nicks, free);
		elem->value = NULL;
	}

	cc_hashtable_destroy(serv->user_to_nicks_map);

	/* Destroy server user_to_sock_map */
	cc_hashtable_iter_init(&itr, serv->user_to_sock_map);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END)
	{
		// Do not destroy the username key as it is owned by user

		int *fd = elem->value;
		free(fd);
		elem->value = NULL;
	}

	cc_hashtable_destroy(serv->user_to_sock_map);

	/* Destroy server connections ma */
	cc_hashtable_iter_init(&itr, serv->connections);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END)
	{
		int *key = elem->key;
		User *usr = elem->value;

		free(key);

		/* close connection and free user memory */
		close(usr->fd);
		User_Destroy(usr);

		elem->key = elem->value = NULL;
	}

	cc_hashtable_destroy(serv->connections);

	/* Destroy channels and save data to file */
	for (size_t i = 0; i < cc_array_size(serv->channels); i++)
	{
		Channel *channel = NULL;
		if (cc_array_get_at(serv->channels, i, (void **)&channel) ==
			CC_OK)
		{
			assert(channel);
			assert(channel->name);

			char *filename = NULL;
			asprintf(&filename, "channels/%s", channel->name);
			Channel_save_to_file(channel, filename);

			free(filename);
		}
	}

	cc_array_destroy_cb(serv->channels, (free_like)Channel_destroy);

	/* close fds */
	close(serv->fd);
	close(serv->epollfd);

	/* free server memory */
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

	log_info("Hashtable initialized with capacity %zu",
			 cc_hashtable_capacity(serv->connections));

	serv->motd_file = MOTD_FILENAME;

	serv->user_to_nicks_map = load_nicks(NICKS_FILENAME);
	assert(serv->user_to_nicks_map);

	if (cc_hashtable_new(&serv->user_to_sock_map) != CC_OK)
	{
		log_error("Failed to create hashtable");
		exit(1);
	}

	if (cc_array_new(&serv->channels) != CC_OK)
	{
		log_error("Failed to create array");
		exit(1);
	}

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	size_t n =
		strftime(serv->created_at, sizeof(serv->created_at), "%c", tm);
	assert(n > 0);

	// Create epoll fd for listen socket and clients
	serv->epollfd = epoll_create(1 + MAX_EVENTS);
	CHECK(serv->epollfd, "epoll_create");

	// TCP Socket non-blocking
	serv->fd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	CHECK(serv->fd, "socket");

	// Server Address
	serv->servaddr.sin_family = AF_INET;
	serv->servaddr.sin_port = htons(port);
	serv->servaddr.sin_addr.s_addr = INADDR_ANY;
	serv->hostname =
		strdup(addr_to_string((struct sockaddr *)&serv->servaddr,
							  sizeof(serv->servaddr)));
	serv->port = make_string("%d", port);

	int yes = 1;

	// Set socket options
	CHECK(setsockopt(serv->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes),
		  "setsockopt");

	// Bind
	CHECK(bind(serv->fd, (struct sockaddr *)&serv->servaddr,
			   sizeof(struct sockaddr_in)),
		  "bind");

	// Listen
	CHECK(listen(serv->fd, MAX_EVENTS), "listen");

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
		int conn_sock =
			accept(serv->fd, (struct sockaddr *)&client_addr, &addrlen);

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
		user->hostname =
			strdup(addr_to_string((struct sockaddr *)&client_addr, addrlen));
		user->nick = make_string("user%05d", (rand() % (int)1e5));
		user->registered = false;
		user->nick_changed = false;
		user->quit = false;

		if (cc_list_new(&user->msg_queue) != CC_OK)
		{
			perror("cc_list_new");
			User_Destroy(user);
			continue;
		}

		if (fcntl(conn_sock, F_SETFL,
				  fcntl(conn_sock, F_GETFL) | O_NONBLOCK) != 0)
		{
			perror("fcntl");
			User_Destroy(user);
			continue;
		}

		struct epoll_event ev = {.data.fd = conn_sock, .events = EPOLLIN | EPOLLOUT};

		if (epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, conn_sock, &ev) !=
			0)
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

		if (cc_hashtable_add(serv->connections, (void *)key, user) !=
			CC_OK)
		{
			perror("cc_hashtable_add");
			User_Destroy(user);
			continue;
		}

		log_info("Got connection %d from %s", conn_sock,
				 user->hostname);
	}
}

/**
 * Find channel by name if loaded and return it.
 * Loads channel from file into memory if exists.
 * Returns NULL if not found.
 */
Channel *Server_get_channel(Server *serv, const char *name)
{
	// Check if channel exists in memory
	for (size_t i = 0; i < cc_array_size(serv->channels); i++)
	{
		Channel *channel = NULL;
		cc_array_get_at(serv->channels, i, (void **)&channel);
		if (!strcmp(channel->name, name))
		{
			return channel;
		}
	}

	// Check if channel exists in file
	char filename[100];
	sprintf(filename, CHANNELS_DIRNAME "/%s", name);

	if (access(filename, F_OK) == 0)
	{
		Channel *channel = Channel_load_from_file(filename);

		if (channel)
		{
			cc_array_add(serv->channels, channel);
			return channel;
		}
	}

	// Channel does not exist or file is corrupted
	return NULL;
}

/**
 * Removes channel from server array and destroys it.
 * Returns true on success and false on failure.
 */
bool Server_remove_channel(Server *serv, const char *name)
{
	assert(serv);
	assert(name);

	Channel *channel = Server_get_channel(serv, name);

	if (!channel)
	{
		return false;
	}

	// Remove channel from channel list
	for (size_t i = 0; i < cc_array_size(serv->channels); i++)
	{
		Channel *current = NULL;

		cc_array_get_at(serv->channels, i, (void **)&current);

		if (!strcmp(current->name, name))
		{
			cc_array_remove_at(serv->channels, i, NULL);
			break;
		}
	}

	Channel_destroy(channel);

	return true;
}
