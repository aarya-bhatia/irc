#include "include/common.h"
#include "include/server.h"
#include "include/replies.h"
#include "include/register.h"
#include <time.h>
#include <sys/stat.h>

#define MOTD_FILENAME "./motd.txt"
#define NICKS_FILENAME "./nicks.txt"

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
			else if(!strcmp(message->command, "PRIVMSG"))
			{
				Server_reply_to_PRIVMSG(serv, usr, message);
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

/**
 * Copy nicks to file and free the memory associated with nick_map
 */
void save_and_destroy_nicks(Server *serv, char *filename)
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

		if (nicks)
		{
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
		}

		fputc('\n', nick_file);

		// cc_hashtable_iter_remove(&itr, (void **) &nicks);
		// cc_array_destroy_cb(nicks, free);
	}

	cc_hashtable_destroy(serv->user_to_nicks_map);

	fclose(nick_file);

	log_info("Wrote nicks to file: %s", NICKS_FILENAME);
}

void Server_destroy(Server *serv)
{
	assert(serv);

	save_and_destroy_nicks(serv, NICKS_FILENAME);

	CC_HashTableIter itr;
	TableEntry *elem;

	cc_hashtable_iter_init(&itr, serv->connections);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END)
	{
		int *key = elem->key;
		User *usr = (User *)elem->value;

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

char *get_motd(char *fname)
{
	FILE *file = fopen(fname, "r");
	char *res = NULL;
	size_t res_len = 0;

	if (!file)
	{
		log_warn("failed to open %s", fname);
		return NULL;
	}
	else
	{
		size_t num_lines = 1;

		// count number of lines
		for (int c = fgetc(file); c != EOF; c = fgetc(file))
		{
			if (c == '\n')
			{
				num_lines = num_lines + 1;
			}
		}

		fseek(file, 0, SEEK_SET); // go to beginning

		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		char *mday = make_string("%d", tm.tm_mday); // day of month
		int mday_int = atoi(mday);
		free(mday);

		size_t line_no = mday_int % num_lines; // select a line from file to use

		for (size_t i = 0; i < line_no + 1; i++)
		{
			if (getline(&res, &res_len, file) == -1)
			{
				perror("getline");
				break;
			}
		}
	}

	if (res && res[res_len - 1] == '\n')
	{
		res[res_len - 1] = 0;
	}

	fclose(file);

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
	serv->motd_file = MOTD_FILENAME;

	serv->user_to_nicks_map = load_nicks(NICKS_FILENAME);
	assert(serv->user_to_nicks_map);

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

void Server_reply_to_NICK(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);

    assert(!strcmp(msg->command, "NICK"));

    if (msg->n_params != 1)
    {
        User_add_msg(usr, make_reply(ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
        return;
    }

    assert(msg->params[0]);

    char *new_nick = msg->params[0];

    if (!check_nick_available(serv, usr, new_nick))
    {
        User_add_msg(usr, make_reply(ERR_NICKNAMEINUSE_MSG, msg->params[0]));
        return;
    }

    free(usr->nick);

    usr->nick = strdup(new_nick);
    usr->nick_changed = true;

    log_info("user %s updated nick", usr->nick);

    check_registration_complete(serv, usr);
    update_nick_map(serv, usr);
}

void Server_reply_to_USER(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);

    assert(!strcmp(msg->command, "USER"));

    if (msg->n_params != 3 || !msg->body)
    {
        User_add_msg(usr, make_reply(ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
        return;
    }

    if (usr->registered)
    {
        User_add_msg(usr, make_reply(ERR_ALREADYREGISTRED_MSG, usr->nick));
        return;
    }

    assert(msg->params[0]);

    char *username = msg->params[0];
    char *realname = msg->body;

    free(usr->username);
    free(usr->realname);

    usr->username = strdup(username);
    usr->realname = strdup(realname);

    log_debug("user %s set username to %s and realname to %s", usr->nick, username, realname);

    check_registration_complete(serv, usr);
}

void Server_reply_to_MOTD(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "MOTD"));

    char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

    if (motd)
    {
        User_add_msg(usr, make_reply(RPL_MOTD_MSG, usr->nick, motd));
    }
    else
    {
        User_add_msg(usr, make_reply(ERR_NOMOTD_MSG, usr->nick));
    }
}

void Server_reply_to_PING(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "PING"));
    User_add_msg(usr, make_reply("PONG %s", serv->hostname));
}

void Server_reply_to_PRIVMSG(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
		assert(!strcmp(msg->command, "PRIVMSG"));

	if(!usr->registered)
	{
		User_add_msg(usr, make_reply(ERR_NOTREGISTERED_MSG, usr->nick));
		return;
	}

	assert(usr->username);

	if(msg->n_params == 0)
	{
		User_add_msg(usr, make_reply(ERR_NORECIPIENT_MSG, usr->nick));
		return;
	}

	assert(msg->params[0]);

	char *target_nick = msg->params[0];

	if(msg->n_params > 1)
	{
		User_add_msg(usr, make_reply(ERR_TOOMANYTARGETS_MSG, usr->nick));
		return;
	}
	
	// Check if nick exists
	CC_HashTableIter itr;
	cc_hashtable_iter_init(&itr, serv->connections);

	TableEntry *entry = NULL;
	User *found_user = NULL;

	while(!found_user && cc_hashtable_iter_next(&itr, &entry) != CC_ITER_END)
	{
		User *user_data = entry->value;
		CC_Array *nicks = NULL;

		if(user_data && 
				user_data->registered && 
				cc_hashtable_get(serv->user_to_nicks_map, user_data->username, (void**) &nicks) == CC_OK && 
				nicks)
		{
			for(size_t i = 0; i < cc_array_size(nicks); i++)
			{
				char *value = NULL;
				if(cc_array_get_at(nicks, i, (void **) &value) == CC_OK && !strcmp(value, target_nick))
				{
					found_user = user_data;
					break;
				}
			}
		}
	}

	if(!found_user)
	{
		User_add_msg(usr, make_reply(ERR_NOSUCHNICK_MSG, usr->nick, target_nick));
		return;
	}
	
	// TODO: Need to check if nick exists at all for RPL_AWAY
	//
	// TODO: Respond to current user for success?
	
	// Add message to target user's queue
	User_add_msg(found_user, make_reply(":%s PRIVMSG %s :%s", usr->nick, target_nick, msg->body));
	return;
}

void Server_reply_to_QUIT(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "QUIT"));

    char *reason = (msg->body ? msg->body : "Client Quit");

    User_add_msg(usr, make_reply("ERROR :Closing Link: %s (%s)", usr->hostname, reason));
}

void Server_reply_to_WHO(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_WHOIS(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_JOIN(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_LIST(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_SERVER(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_PASS(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}
