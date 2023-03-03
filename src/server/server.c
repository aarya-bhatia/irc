#include "include/common.h"
#include "include/server.h"
#include "include/replies.h"
#include "include/register.h"
#include <time.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <dirent.h>

typedef void (*free_like)(void *);

void _sanity_check(Server * serv, User * usr, Message * msg)
{
	assert(serv);
	assert(usr);
	assert(msg);
}

void Server_process_request(Server * serv, User * usr)
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
	while (cc_array_iter_next(&itr, (void **)&message) != CC_ITER_END) {
		assert(message);

		if (message->command) {
			if (!strcmp(message->command, "MOTD")) {
				Server_reply_to_MOTD(serv, usr, message);
			} else if (!strcmp(message->command, "NICK")) {
				Server_reply_to_NICK(serv, usr, message);
			} else if (!strcmp(message->command, "USER")) {
				Server_reply_to_USER(serv, usr, message);
			} else if (!strcmp(message->command, "PING")) {
				Server_reply_to_PING(serv, usr, message);
			} else if (!strcmp(message->command, "PRIVMSG")) {
				Server_reply_to_PRIVMSG(serv, usr, message);
			} else if (!strcmp(message->command, "JOIN")) {
				Server_reply_to_JOIN(serv, usr, message);
			} else if (!strcmp(message->command, "QUIT")) {
				Server_reply_to_QUIT(serv, usr, message);
				assert(cc_list_size(usr->msg_queue) > 0);
				usr->quit = true;
			} else if (!usr->registered) {
				User_add_msg(usr,
					     make_reply(":%s "
							ERR_NOTREGISTERED_MSG,
							serv->hostname,
							usr->nick));
			} else {
				User_add_msg(usr,
					     make_reply(":%s "
							ERR_UNKNOWNCOMMAND_MSG,
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
void write_nicks_to_file(Server * serv, char *filename)
{
	FILE *nick_file = fopen(filename, "w");

	if (!nick_file) {
		log_error("Failed to open nick file: %s", filename);
		return;
	}

	CC_HashTableIter itr;
	TableEntry *elem;

	cc_hashtable_iter_init(&itr, serv->user_to_nicks_map);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END) {
		fwrite(elem->key, 1, strlen(elem->key), nick_file);
		fputc(':', nick_file);

		CC_Array *nicks = elem->value;

		assert(nicks);

		char *nick = NULL;

		for (size_t i = 0; i < cc_array_size(nicks); i++) {
			if (cc_array_get_at(nicks, i, (void **)&nick) == CC_OK) {
				fwrite(nick, 1, strlen(nick), nick_file);
			}

			if (i + 1 < cc_array_size(nicks)) {
				fputc(',', nick_file);
			}
		}

		fputc('\n', nick_file);
	}

	fclose(nick_file);

	log_info("Wrote nicks to file: %s", NICKS_FILENAME);
}

void Server_destroy(Server * serv)
{
	assert(serv);
	write_nicks_to_file(serv, NICKS_FILENAME);

	CC_HashTableIter itr;
	TableEntry *elem = NULL;

	/* Destroy server user_to_nicks_map */
	cc_hashtable_iter_init(&itr, serv->user_to_nicks_map);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END) {
		free(elem->key);
		elem->key = NULL;

		CC_Array *nicks = elem->value;
		cc_array_destroy_cb(nicks, free);
		elem->value = NULL;
	}

	cc_hashtable_destroy(serv->user_to_nicks_map);

	/* Destroy server user_to_sock_map */
	cc_hashtable_iter_init(&itr, serv->user_to_sock_map);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END) {
		// Do not destroy the username key as it is owned by user

		int *fd = elem->value;
		free(fd);
		elem->value = NULL;
	}

	cc_hashtable_destroy(serv->user_to_sock_map);

	/* Destroy server connections ma */
	cc_hashtable_iter_init(&itr, serv->connections);

	while (cc_hashtable_iter_next(&itr, &elem) != CC_ITER_END) {
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
	for (size_t i = 0; i < cc_array_size(serv->channels); i++) {
		Channel *channel = NULL;
		if (cc_array_get_at(serv->channels, i, (void **)&channel) ==
		    CC_OK) {
			assert(channel);
			assert(channel->name);

			char *filename = NULL;
			asprintf(&filename, "channels/%s", channel->name);
			Channel_save_to_file(channel, filename);

			free(filename);
		}
	}

	cc_array_destroy_cb(serv->channels, (free_like) Channel_destroy);

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

char *get_motd(char *fname)
{
	FILE *file = fopen(fname, "r");

	if (!file) {
		log_warn("failed to open %s", fname);
		return NULL;
	}

	char *res = NULL;
	size_t res_len = 0;
	size_t num_lines = 1;

	// count number of lines
	for (int c = fgetc(file); c != EOF; c = fgetc(file)) {
		if (c == '\n') {
			num_lines = num_lines + 1;
		}
	}

	fseek(file, 0, SEEK_SET);	// go to beginning

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	size_t line_no = tm.tm_yday % num_lines;	// select a line from file to use

	for (size_t i = 0; i < line_no + 1; i++) {
		if (getline(&res, &res_len, file) == -1) {
			perror("getline");
			break;
		}
	}

	if (res && res[res_len - 1] == '\n') {
		res[res_len - 1] = 0;
	}

	fclose(file);

	return res;
}

void load_channels(CC_Array * channels, char *dirname)
{
	DIR *dir = opendir(dirname);

	if (!dir) {
		perror("opendir");
		log_error("Failed to load channels from dir %s", dirname);
		return;
	}

	struct dirent *d = NULL;

	while ((d = readdir(dir)) != NULL) {
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) {
			continue;
		}

		if (d->d_type == DT_REG) {
			Channel *channel = Channel_load_from_file(d->d_name);

			if (channel) {
				if (cc_array_add(channels, channel) != CC_OK) {
					log_error("cc_array_add");
				}
			}
		}
	}

	closedir(dir);

	log_info("Loaded all channels from dir %s", dirname);
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
	if (cc_hashtable_new_conf(&htc, &serv->connections) != CC_OK) {
		log_error("Failed to create hashtable");
		exit(1);
	}

	log_info("Hashtable initialized with capacity %zu",
		 cc_hashtable_capacity(serv->connections));

	serv->motd_file = MOTD_FILENAME;

	serv->user_to_nicks_map = load_nicks(NICKS_FILENAME);
	assert(serv->user_to_nicks_map);

	if (cc_hashtable_new(&serv->user_to_sock_map) != CC_OK) {
		log_error("Failed to create hashtable");
		exit(1);
	}

	if (cc_array_new(&serv->channels) != CC_OK) {
		log_error("Failed to create array");
		exit(1);
	}
	// Load channels from file
	load_channels(serv->channels, CHANNELS_DIRNAME);

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
	    strdup(addr_to_string
		   ((struct sockaddr *)&serv->servaddr,
		    sizeof(serv->servaddr)));
	serv->port = make_string("%d", port);

	int yes = 1;

	// Set socket options
	CHECK(setsockopt(serv->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes),
	      "setsockopt");

	// Bind
	CHECK(bind
	      (serv->fd, (struct sockaddr *)&serv->servaddr,
	       sizeof(struct sockaddr_in)), "bind");

	// Listen
	CHECK(listen(serv->fd, MAX_EVENTS), "listen");

	log_info("server running on port %d", port);

	return serv;
}

/**
 * There are new connections available
 */
void Server_accept_all(Server * serv)
{
	struct sockaddr_storage client_addr;
	socklen_t addrlen = sizeof(client_addr);

	while (1) {
		int conn_sock =
		    accept(serv->fd, (struct sockaddr *)&client_addr, &addrlen);

		if (conn_sock == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}

			die("accept");
		}

		User *user = calloc(1, sizeof(User));

		user->fd = conn_sock;
		user->hostname =
		    strdup(addr_to_string
			   ((struct sockaddr *)&client_addr, addrlen));
		user->nick = make_string("user%05d", (rand() % (int)1e5));
		user->registered = false;
		user->nick_changed = false;
		user->quit = false;

		if (cc_list_new(&user->msg_queue) != CC_OK) {
			perror("cc_list_new");
			User_Destroy(user);
			continue;
		}

		if (cc_array_new(&user->memberships) != CC_OK) {
			perror("cc_array_new");
			User_Destroy(user);
			continue;
		}

		if (fcntl
		    (conn_sock, F_SETFL,
		     fcntl(conn_sock, F_GETFL) | O_NONBLOCK) != 0) {
			perror("fcntl");
			User_Destroy(user);
			continue;
		}

		struct epoll_event ev = {.data.fd = conn_sock,.events =
			    EPOLLIN | EPOLLOUT };

		if (epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, conn_sock, &ev) !=
		    0) {
			perror("epoll_ctl");
			User_Destroy(user);
			continue;
		}

		int *key = calloc(1, sizeof(int));

		if (!key) {
			perror("calloc");
			User_Destroy(user);
			continue;
		}

		*key = user->fd;

		if (cc_hashtable_add(serv->connections, (void *)key, user) !=
		    CC_OK) {
			perror("cc_hashtable_add");
			User_Destroy(user);
			continue;
		}

		log_info("Got connection %d from %s", conn_sock,
			 user->hostname);
	}
}

/**
 * The USER and NICK command should be the first messages sent by a new client to complete registration.
 *
 * Syntax: `NICK <nickname> [<hopcount>]`
 * Example: `NICK aarya\r\n`
 *
 * Replies:
 *
 * - RPL_WELCOME
 * - ERR_NONICKNAMEGIVEN
 * - ERR_NICKNAMEINUSE
 */
void Server_reply_to_NICK(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);

	assert(!strcmp(msg->command, "NICK"));

	if (msg->n_params != 1) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NEEDMOREPARAMS_MSG,
					serv->hostname, usr->nick,
					msg->command));
		return;
	}

	assert(msg->params[0]);

	char *new_nick = msg->params[0];

	if (!check_nick_available(serv, usr, new_nick)) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NICKNAMEINUSE_MSG,
					serv->hostname, msg->params[0]));
		return;
	}

	free(usr->nick);

	usr->nick = strdup(new_nick);
	usr->nick_changed = true;

	log_info("user %s updated nick", usr->nick);

	check_registration_complete(serv, usr);
	update_nick_map(serv, usr);
}

/**
 * The USER and NICK command should be the first messages sent by a new client to complete registration.
 * - Syntax: `USER <username> * * :<realname>`
 *
 * - Description: USER message is used at the beginning to specify the username and realname of new user.
 * - It is used in communication between servers to indicate new user
 * - A client will become registered after both USER and NICK have been received.
 * - Realname can contain spaces.
 */
void Server_reply_to_USER(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
	assert(!strcmp(msg->command, "USER"));

	if (msg->n_params != 3 || !msg->body) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NEEDMOREPARAMS_MSG,
					serv->hostname, usr->nick,
					msg->command));
		return;
	}
	// User cannot set username/realname twice
	if (usr->username && usr->realname) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_ALREADYREGISTRED_MSG,
					serv->hostname, usr->nick));
		return;
	}

	assert(msg->params[0]);
	assert(msg->body);

	char *username = msg->params[0];
	char *realname = msg->body;

	// Set the username and realname
	usr->username = strdup(username);
	usr->realname = strdup(realname);

	log_debug("user %s set username to %s and realname to %s", usr->nick,
		  username, realname);

	// To store the value of socket in hashmap
	int *userfd = malloc(sizeof *userfd);
	*userfd = usr->fd;

	// Register user's username to their socket in map
	cc_hashtable_add(serv->user_to_sock_map, usr->username, userfd);

	// Complete registration and prepare greeting messages
	check_registration_complete(serv, usr);
}

void Server_reply_to_MOTD(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
	assert(!strcmp(msg->command, "MOTD"));

	char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

	if (motd) {
		User_add_msg(usr,
			     make_reply(":%s " RPL_MOTD_MSG, serv->hostname,
					usr->nick, motd));
	} else {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NOMOTD_MSG, serv->hostname,
					usr->nick));
	}
}

void Server_reply_to_PING(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
	assert(!strcmp(msg->command, "PING"));
	User_add_msg(usr, make_reply(":%s "
				     "PONG %s",
				     serv->hostname, serv->hostname));
}

/**
 * The PRIVMSG command is used to deliver a message to from one client to another within an IRC network.
 *
 * - Sytnax: `[:prefix] PRIVMSG <receiver> :<text>`
 * - Example: `PRIVMSG Aarya :hello, how are you?`
 *
 * Replies
 *
 * - RPL_AWAY
 * - ERR_NORECIPEINT
 * - ERR_NOSUCHNICK
 * - ERR_TOOMANYTARGETS
 */
void Server_reply_to_PRIVMSG(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
	assert(!strcmp(msg->command, "PRIVMSG"));

	if (!usr->registered) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NOTREGISTERED_MSG,
					serv->hostname, usr->nick));
		return;
	}

	assert(usr->username);

	if (msg->n_params == 0) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NORECIPIENT_MSG,
					serv->hostname, usr->nick));
		return;
	}

	assert(msg->params[0]);

	if (msg->n_params > 1) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_TOOMANYTARGETS_MSG,
					serv->hostname, usr->nick));
		return;
	}
	// The nick to send message to
	const char *target_nick = msg->params[0];
	char *target_username = NULL;

	CC_HashTableIter itr;	// iterate
	TableEntry *entry = NULL;
	cc_hashtable_iter_init(&itr, serv->user_to_nicks_map);

	// For each registered user, check if one of their nicks match the given nick
	while (!target_username
	       && cc_hashtable_iter_next(&itr, &entry) != CC_ITER_END) {
		char *cur_username = entry->key;
		CC_Array *cur_nicks = entry->value;

		assert(cur_username);
		assert(cur_nicks);

		// Iterate the nicks array
		CC_ArrayIter nick_itr;
		cc_array_iter_init(&nick_itr, cur_nicks);
		char *cur_nick = NULL;

		while (cc_array_iter_next(&nick_itr, (void **)&cur_nick) !=
		       CC_ITER_END) {
			assert(cur_nick);

			// match found
			if (!strcmp(cur_nick, target_nick)) {
				target_username = cur_username;
				break;
			}
		}
	}

	// no user found with a matching nick
	if (!target_username) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NOSUCHNICK_MSG,
					serv->hostname, usr->nick,
					target_nick));
		return;
	}
	// Look up the socket for target user through their username
	int *target_sock = NULL;

	cc_hashtable_get(serv->user_to_sock_map, target_username,
			 (void **)&target_sock);

	// user is offline
	if (!target_sock) {
		User_add_msg(usr,
			     make_reply(":%s " RPL_AWAY_MSG, serv->hostname,
					usr->nick, target_nick));
		return;
	}

	assert(target_sock);

	// Look up the user data for the target user through their socket

	User *target_data = NULL;

	if (cc_hashtable_get
	    (serv->connections, (void *)target_sock,
	     (void **)&target_data) == CC_OK) {
		assert(target_data);

		// Add message to target user's queue
		User_add_msg(target_data,
			     make_reply(":%s!%s@%s PRIVMSG %s :%s", usr->nick,
					usr->username, usr->hostname,
					target_nick, msg->body));
		return;
	}

	log_info("PRIVMSG sent from user %s to user %s", usr->nick,
		 target_nick);

	// TODO: Respond to current user for success
}

/**
 * The QUIT command should be the final message sent by client to close the connection.
 *
 * - Syntax: `QUIT :<message>`
 * - Example: `QUIT :Gone to have lunch.`
 *
 * Replies:
 *
 * - ERROR
 */
void Server_reply_to_QUIT(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
	assert(!strcmp(msg->command, "QUIT"));

	char *reason = (msg->body ? msg->body : "Client Quit");

	User_add_msg(usr, make_reply(":%s "
				     "ERROR :Closing Link: %s (%s)",
				     serv->hostname, usr->hostname, reason));
}

void Server_reply_to_WHO(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
}

void Server_reply_to_WHOIS(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
}

/**
 * Join a channel
 */
void Server_reply_to_JOIN(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
	assert(!strcmp(msg->command, "JOIN"));

	if (!usr->registered) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NOTREGISTERED_MSG,
					serv->hostname, usr->nick));
		return;
	}

	assert(usr->username);

	if (msg->n_params == 0) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NEEDMOREPARAMS_MSG,
					serv->hostname, usr->nick,
					msg->command));
		return;
	}

	assert(msg->params[0]);

	if (msg->params[0][0] != '#') {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NOSUCHCHANNEL_MSG,
					serv->hostname, usr->nick,
					msg->params[0]));
		return;
	}

	char *channel_name = msg->params[0] + 1;	// skip #

	Channel *channel = Server_get_channel(serv, channel_name);

	if (!channel) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_NOSUCHCHANNEL_MSG,
					serv->hostname, usr->nick,
					channel_name));
		return;
	}

	if (cc_array_size(usr->memberships) > MAX_CHANNEL_COUNT) {
		User_add_msg(usr,
			     make_reply(":%s " ERR_TOOMANYCHANNELS_MSG,
					serv->hostname, usr->nick,
					channel_name));
		return;
	}
	// Check if membership exists

	bool found = false;
	for (size_t i = 0; i < cc_array_size(usr->memberships); i++) {
		Membership *tmp = NULL;
		if (cc_array_get_at(usr->memberships, i, (void **)&tmp) ==
		    CC_OK) {
			assert(tmp);
			if (!strcmp(tmp->channel_name, channel_name)) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		Membership *membership = calloc(1, sizeof *membership);
		membership->channel_name = channel_name;
		membership->channel_mode = 0;
		membership->date_joined = time(NULL);

		if (cc_array_add(usr->memberships, membership) != CC_OK) {
			log_error("cc_array_add");
			return;
		}

		if (!Channel_add_member(channel, usr->username)) {
			log_error("Channel_add_member");
			return;
		}
	}

	if (channel->topic) {
		User_add_msg(usr,
			     make_reply(":%s " RPL_TOPIC_MSG, serv->hostname,
					usr->nick, channel_name,
					channel->topic));
	}
	// Compose names list as multipart message

	char *subject =
	    make_string(":%s " RPL_NAMREPLY_MSG, serv->hostname, usr->nick, "=",
			channel_name);
	char message[MAX_MSG_LEN + 1] = { 0 };
	strcat(message, subject);

	// Get channel members
	for (size_t i = 0; i < cc_array_size(channel->members); i++) {
		char *username = NULL;

		if (cc_array_get_at(channel->members, i, (void **)&username) ==
		    CC_OK) {
			assert(username);

			size_t len = strlen(username) + 1;	// Length for name and space

			if (strlen(message) + len > MAX_MSG_LEN) {
				// End current message
				User_add_msg(usr, make_reply("%s", message));

				// Start new message with subject
				message[0] = 0;
				strcat(message, subject);
			}
			// Append username and space to message
			strcat(message, username);
			strcat(message, " ");
		}
	}

	User_add_msg(usr, make_reply("%s", message));

	free(subject);

	User_add_msg(usr,
		     make_reply(":%s " RPL_ENDOFNAMES_MSG, serv->hostname,
				usr->nick, channel_name));
}

void Server_reply_to_LIST(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
}

void Server_reply_to_NAMES(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
}

void Server_reply_to_SERVER(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
}

void Server_reply_to_PASS(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
}

void Server_reply_to_CONNECT(Server * serv, User * usr, Message * msg)
{
	_sanity_check(serv, usr, msg);
}
