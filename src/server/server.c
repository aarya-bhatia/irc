
#include "include/server.h"

#include <sys/epoll.h>
#include <sys/stat.h>
#include <time.h>

#include "include/common.h"
#include "include/hashtable.h"
#include "include/list.h"

/**
 * request handler
 */
typedef struct _UserRequestHandler
{
	const char *name;							  // command name
	void (*handler)(Server *, User *, Message *); // request handler function
} UserRequestHandler;

/**
 * Look up table for user request handlers
 */
static UserRequestHandler user_request_handlers[] = {
	{"NICK", Server_handle_NICK},
	{"USER", Server_handle_USER},
	{"PRIVMSG", Server_handle_PRIVMSG},
	{"NOTICE", Server_handle_NOTICE},
	{"PING", Server_handle_PING},
	{"QUIT", Server_handle_QUIT},
	{"MOTD", Server_handle_MOTD},
	{"INFO", Server_handle_INFO},
	{"LIST", Server_handle_LIST},
	{"WHO", Server_handle_WHO},
	{"JOIN", Server_handle_JOIN},
	{"PART", Server_handle_PART},
	{"NAMES", Server_handle_NAMES},
	{"TOPIC", Server_handle_TOPIC},
	{"LUSERS", Server_handle_LUSERS},
	{"HELP", Server_handle_HELP},
	{"CONNECT", Server_handle_CONNECT},
	{"TEST_LIST_SERVER", Server_handle_TEST_LIST_SERVER},
};

Peer *Peer_alloc(int type)
{
	Peer *this = calloc(1, sizeof *this);
	this->msg_queue = List_alloc(NULL, free);
	this->server_type = type;
	return this;
}

void Peer_free(Peer *this)
{
	List_free(this->msg_queue);
	free(this->name);
	free(this->passwd);
	free(this);
}

User *User_alloc()
{
	User *this = calloc(1, sizeof *this);
	this->nick = make_string("user%05d", (rand() % (int)1e5)); // temporary nick
	this->channels = Vector_alloc(4, (elem_copy_type)strdup, free);
	this->msg_queue = List_alloc(NULL, free);
	return this;
}

void User_free(User *usr)
{
	free(usr->nick);
	free(usr->username);
	free(usr->realname);
	free(usr->hostname);

	Vector_free(usr->channels);
	List_free(usr->msg_queue);

	free(usr);
}

bool User_is_member(User *usr, const char *channel_name)
{
	for (size_t i = 0; i < Vector_size(usr->channels); i++)
	{
		if (!strcmp(Vector_get_at(usr->channels, i), channel_name))
		{
			return true;
		}
	}

	return false;
}

void User_add_channel(User *usr, const char *channel_name)
{
	if (!User_is_member(usr, channel_name))
	{
		Vector_push(usr->channels, (void *)channel_name);
	}
}

bool User_remove_channel(User *usr, const char *channel_name)
{
	for (size_t i = 0; i < Vector_size(usr->channels); i++)
	{
		if (!strcmp(Vector_get_at(usr->channels, i), channel_name))
		{
			Vector_remove(usr->channels, i, NULL);
			return true;
		}
	}

	return false;
}

void add_message(List *queue, const char *message)
{
	if (strstr(message, "\r\n"))
	{
		List_push_back(queue, strdup(message));
	}
	else
	{
		List_push_back(queue, make_string("%s\r\n", message));
	}
}

/**
 * Create and initialise the server with given name.
 * Reads server info from config file.
 */
Server *Server_create(const char *name)
{
	assert(name);

	Server *serv = calloc(1, sizeof *serv);
	assert(serv);

	serv->motd_file = MOTD_FILENAME;
	serv->config_file = CONFIG_FILENAME;

	struct peer_info_t serv_info;
	memset(&serv_info, 0, sizeof serv_info);

	if (!get_peer_info(CONFIG_FILENAME, name, &serv_info))
	{
		log_error("server %s not found in config file %s", name,
				  serv->config_file);
		exit(1);
	}

	serv->name = serv_info.peer_name;
	serv->port = serv_info.peer_port;
	serv->passwd = serv_info.peer_passwd;
	serv->hostname = serv_info.peer_host;

	serv->info = strdup(DEFAULT_INFO);

	serv->connections = ht_alloc_type(INT_TYPE, SHALLOW_TYPE);			   /* Map<int, Connection *> */
	serv->name_to_peer_map = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE);	   /* Map<string,  Peer *> */
	serv->nick_to_serv_name_map = ht_alloc(STRING_TYPE, STRING_TYPE);	   /* Map<string, string> */
	serv->nick_to_user_map = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE);	   /* Map<string, User*> */
	serv->name_to_channel_map = load_channels(CHANNELS_FILENAME);		   /* Map<string, Channel *> */
	serv->test_list_server_map = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE); /* Map<string, struct ListCommand *> */

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	size_t n = strftime(serv->created_at, sizeof(serv->created_at), "%c", tm);
	assert(n > 0);

	// Create epoll fd for listen socket and clients
	serv->epollfd = epoll_create(1 + MAX_EVENTS);
	CHECK(serv->epollfd, "epoll_create");

	// TCP Socket non-blocking
	serv->fd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	CHECK(serv->fd, "socket");

	// Server Address
	serv->servaddr.sin_family = AF_INET;
	serv->servaddr.sin_port = htons(atoi(serv->port));
	serv->servaddr.sin_addr.s_addr = INADDR_ANY;

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

	log_info("Server \"%s\" is running on port %s at %s", serv->name, serv->port, serv->hostname);
	return serv;
}

/**
 * Remove any open connections from server.
 */
void Server_remove_all_connections(Server *serv)
{
	HashtableIter conn_itr;
	ht_iter_init(&conn_itr, serv->connections);
	Connection *conn = NULL;

	while (ht_iter_next(&conn_itr, NULL, (void **)&conn))
	{
		Server_remove_connection(serv, conn);
	}
}

/**
 * Destroy server and close all connections.
 */
void Server_destroy(Server *serv)
{
	assert(serv);

	save_channels(serv->name_to_channel_map, CHANNELS_FILENAME);
	Server_remove_all_connections(serv);

	ht_free(serv->name_to_channel_map);
	ht_free(serv->name_to_peer_map);
	ht_free(serv->nick_to_user_map);
	ht_free(serv->connections);
	ht_free(serv->test_list_server_map);

	close(serv->fd);
	close(serv->epollfd);
	free(serv->hostname);
	free(serv->port);
	free(serv->passwd);
	free(serv->info);
	free(serv->name);
	free(serv);

	log_debug("Server stopped");
	exit(0);
}

/**
 * There are new connections available.
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

		Connection *conn =
			Connection_alloc(conn_sock, (struct sockaddr *)&client_addr,
							 addrlen);

		if (!Server_add_connection(serv, conn))
		{
			Server_remove_connection(serv, conn);
		}
	}
}

/**
 * Process request from unknown connection and promote it to either
 * user or peer connection based on the initial messages.
 */
void Server_process_request_from_unknown(Server *serv, Connection *conn)
{
	assert(conn->conn_type == UNKNOWN_CONNECTION);
	assert(conn->data == NULL);

	char *message = List_peek_front(conn->incoming_messages);

	if (strncmp(message, "QUIT", 4) == 0)
	{
		char *reason = strstr(message, ":");
		if (!reason)
		{
			reason = "";
		}
		List_push_back(conn->outgoing_messages, Server_create_message(serv, "ERROR :Closing Link: %s (%s)", conn->hostname, reason));
		conn->quit = true;
	}
	else if (strncmp(message, "NICK", 4) == 0 || strncmp(message, "USER", 4) == 0)
	{
		conn->conn_type = USER_CONNECTION;
		conn->data = User_alloc();
		((User *)conn->data)->hostname = strdup(conn->hostname);
		((User *)conn->data)->fd = conn->fd;
	}
	else if (strncmp(message, "PASS", 4) == 0 || strncmp(message, "SERVER", 6) == 0)
	{
		conn->conn_type = PEER_CONNECTION;
		conn->data = Peer_alloc(ACTIVE_SERVER);
		((Peer *)conn->data)->fd = conn->fd;
	}
	else
	{
		log_warn("Invalid message: %s", List_peek_front(conn->incoming_messages));
		List_pop_front(conn->incoming_messages);
		return;
	}
}

/**
 * Process request from user connection
 */
void Server_process_request_from_user(Server *serv, Connection *conn)
{
	assert(conn->conn_type == USER_CONNECTION);

	User *usr = conn->data;
	assert(usr);

	// Get array of parsed messages
	Vector *messages = parse_message_list(conn->incoming_messages);
	assert(messages);

	// log_debug("Processing %zu messages from connection %d", Vector_size(messages), conn->fd);

	// Iterate over the request messages and add response message(s) to user's message queue in the same order.
	for (size_t i = 0; i < Vector_size(messages); i++)
	{
		Message *message = Vector_get_at(messages, i);
		assert(message);
		log_debug("Message from user %s: %s", usr->nick, message->message);

		if (!message->command)
		{
			log_error("invalid message");
			continue;
		}

		// Find a handler to execute for given request

		bool found = false;

		for (size_t j = 0; j < sizeof(user_request_handlers) / sizeof(user_request_handlers[0]); j++)
		{
			UserRequestHandler handle = user_request_handlers[j];

			if (!strcmp(handle.name, message->command))
			{
				handle.handler(serv, usr, message);
				found = true;
				break;
			}
		}

		if (found)
		{
			continue;
		}

		// Handle every other command

		if (!usr->registered)
		{
			char *reply = Server_create_message(serv, "451 %s :Connection not registered", usr->nick);
			List_push_back(conn->outgoing_messages, reply);
		}
		else
		{
			char *reply = Server_create_message(serv, "421 %s %s :Unknown command", usr->nick, message->command);
			List_push_back(conn->outgoing_messages, reply);
		}

		conn->quit = usr->quit;
	}

	Vector_free(messages);
}

/**
 * This will send a message to all known members of channel on given server and
 * forward the message to its peers to reach other channel members in the
 * network.
 */
void Server_message_channel(Server *serv, const char *origin, const char *target, const char *message)
{
	Channel *channel = ht_get(serv->name_to_channel_map, target);

	if (channel)
	{
		HashtableIter itr;
		ht_iter_init(&itr, channel->members);
		User *member = NULL;

		// Send message to all users on channel
		while (ht_iter_next(&itr, NULL, (void **)&member))
		{
			if (member->registered && !member->quit)
			{
				log_debug("Sent message to user %s in channel %s on server %s", member->nick, channel->name, serv->name);
				add_message(member->msg_queue, message);
			}
		}
	}

	Server_relay_message(serv, origin, message);
}

/**
 * Attemps to send a message to user with given nick.
 * Relays message to peers if user is not on current server.
 * If user is not on this server, message is relayed to its peers.
 */
void Server_message_user(Server *serv, const char *origin, const char *target, const char *message)
{
	// TODO: check if user nick exists in nick_to_serv_name map

	User *user = ht_get(serv->nick_to_user_map, target);

	if (user)
	{
		// found user
		if (user->registered && !user->quit)
		{
			log_debug("Sent message to user %s on server %s", user->nick, serv->name);
			add_message(user->msg_queue, message);
		}
	}
	else
	{
		Server_relay_message(serv, origin, message);
	}
}

/**
 * Send message to every peer connected to server.
 * NOTE: This should be only called if the message is originating from this server.
 */
void Server_broadcast_message(Server *serv, const char *message)
{
	HashtableIter itr;
	ht_iter_init(&itr, serv->name_to_peer_map);
	Peer *peer = NULL;
	int c = 0;

	Hashtable *visited = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE);

	while (ht_iter_next(&itr, NULL, (void **)&peer))
	{
		if (ht_contains(visited, peer->name))
		{
			continue;
		}

		if (peer->registered && !peer->quit)
		{
			add_message(peer->msg_queue, message);
			c++;
		}

		ht_set(visited, peer->name, NULL);
	}

	if (c)
	{
		log_debug("Sent message to %d peers", c);
	}

	ht_free(visited);
}

/**
 * Send given message to all known peers on this server except origin server.
 */
void Server_relay_message(Server *serv, const char *origin, const char *message)
{
	HashtableIter itr;
	ht_iter_init(&itr, serv->name_to_peer_map);
	Peer *peer = NULL;
	int c = 0;

	Hashtable *visited = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE);

	while (ht_iter_next(&itr, NULL, (void **)&peer))
	{
		if (ht_contains(visited, peer->name))
		{
			continue;
		}

		if (strcmp(peer->name, origin) != 0 && peer->registered && !peer->quit)
		{
			add_message(peer->msg_queue, message);
			c++;
		}

		ht_set(visited, peer->name, NULL);
	}

	if (c)
	{
		log_debug("Relayed message to %d peers on server %s", c, serv->name);
	}

	ht_free(visited);
}

/**
 * Process request from peer connection
 */
void Server_process_request_from_peer(Server *serv, Connection *conn)
{
	assert(conn->conn_type == PEER_CONNECTION);

	Peer *peer = conn->data;
	assert(peer);

	Vector *messages = parse_message_list(conn->incoming_messages);

	for (size_t i = 0; i < Vector_size(messages); i++)
	{
		Message *message = Vector_get_at(messages, i);

		if (!strcmp(message->command, "ERROR"))
		{
			peer->quit = true;
			break;
		}
		else if (!strcmp(message->command, "SQUIT"))
		{
			List_push_back(peer->msg_queue, Server_create_message(serv, "ERROR :Closing Link: %s", conn->hostname));
			peer->quit = true;
			break;
		}
		else if (!strcmp(message->command, "TEST_LIST_SERVER"))
		{
			Server_handle_peer_TEST_LIST_SERVER(serv, peer, message);
		}
		else if (!strcmp(message->command, "901"))
		{
			if (message->n_params > 0 && message->body)
			{
				char *target = message->params[0];
				assert(target);
				struct ListCommand *list_data = ht_get(serv->test_list_server_map, target);
				if (list_data)
				{
					log_debug("Sent TEST_LIST_SERVER reply for %s to fd %d", target, list_data->conn->fd);
					List_push_back(list_data->conn->outgoing_messages, strdup(message->message));
				}
			}
		}
		else if (!strcmp(message->command, "902"))
		{
			if (message->n_params > 0 && message->body)
			{
				char *target = message->params[0];
				assert(target);
				struct ListCommand *list_data = ht_get(serv->test_list_server_map, target);
				if (list_data)
				{
					ht_remove(list_data->pending, peer->name, NULL, NULL);
					if (ht_size(list_data->pending) == 0)
					{
						log_debug("Sent TEST_LIST_SERVER_END reply for %s to %d", target, list_data->conn->fd);
						List_push_back(list_data->conn->outgoing_messages, Server_create_message(serv, "902 %s :End of TEST_LIST_SERVER", target));
						ht_remove(serv->test_list_server_map, target, NULL, NULL);
						ht_free(list_data->pending);
						free(list_data);
					}
				}
			}
		}
		else if (!strcmp(message->command, "SERVER"))
		{
			if (!peer->registered)
			{
				Server_handle_SERVER(serv, peer, message);
			}
			else
			{
				char *server_name = message->params[0];
				assert(server_name);

				if (ht_contains(serv->name_to_peer_map, server_name))
				{
					log_error("Cycle detected: Remove peer %s", server_name);
					Peer *other_peer = ht_get(serv->name_to_peer_map, server_name);
					Connection *other_conn = ht_get(serv->connections, &other_peer->fd);
					assert(other_conn);
					Server_remove_connection(serv, other_conn);
					continue;
				}

				ht_set(serv->name_to_peer_map, server_name, peer);
				Server_relay_message(serv, peer->name, message->message);
			}
		}
		else if (!strcmp(message->command, "PASS"))
		{
			Server_handle_PASS(serv, peer, message);
		}
		else if (!strcmp(message->command, "NICK")) // A new user was registered on peer server
		{
			char *nick = message->params[0];
			assert(nick);

			if (ht_contains(serv->nick_to_serv_name_map, nick))
			{
				log_error("NICK collision: %s", nick);
				User *user = ht_get(serv->nick_to_user_map, nick);
				assert(user);
				Connection *other_conn = ht_get(serv->connections, &user->fd);
				assert(other_conn);
				Server_remove_connection(serv, other_conn);
				continue;
			}

			log_info("== user %s registered with server %s == ", nick, peer->name);
			ht_set(serv->nick_to_serv_name_map, nick, peer->name);
			Server_relay_message(serv, peer->name, message->message);
		}
		else if (!strcmp(message->command, "PRIVMSG")) // To send message to a user or channel
		{
			assert(message->n_params > 0);

			if (*message->params[0] == '#')
			{
				Server_message_channel(serv, peer->name, message->params[0] + 1, message->message);
			}
			else
			{
				Server_message_user(serv, peer->name, message->params[0], message->message);
			}
		}
		else if (!strcmp(message->command, "JOIN")) // TODO: Update channel map
		{
			Server_message_channel(serv, peer->name, message->params[0] + 1, message->message);
		}
		else if (!strcmp(message->command, "PART"))
		{
			Server_message_channel(serv, peer->name, message->params[0] + 1, message->message);
		}
		else
		{
			Server_relay_message(serv, peer->name, message->message);
		}
	}

	Vector_free(messages);
	conn->quit = peer->quit;
}

/**
 * Process incoming messages sent by connection.
 */
void Server_process_request(Server *serv, Connection *conn)
{
	assert(serv);
	assert(conn);

	if (List_size(conn->incoming_messages) == 0)
	{
		return;
	}

	if (!conn->quit && conn->conn_type == UNKNOWN_CONNECTION)
	{
		Server_process_request_from_unknown(serv, conn);
	}

	if (!conn->quit && conn->conn_type == PEER_CONNECTION)
	{
		Server_process_request_from_peer(serv, conn);
	}

	if (!conn->quit && conn->conn_type == USER_CONNECTION)
	{
		Server_process_request_from_user(serv, conn);
	}
}

/**
 * Add new connection to server and listen for read/write events on that
 * connection.
 */
bool Server_add_connection(Server *serv, Connection *connection)
{
	assert(serv);
	assert(connection);
	assert(connection->conn_type == UNKNOWN_CONNECTION);

	ht_set(serv->connections, &connection->fd, connection);

	// Make user socket non-blocking
	if (fcntl(connection->fd, F_SETFL, fcntl(connection->fd, F_GETFL) | O_NONBLOCK) != 0)
	{
		perror("fcntl");
		return false;
	}
	// Add event
	struct epoll_event ev = {.data.fd = connection->fd, .events = EPOLLIN | EPOLLOUT};

	// Add user socket to epoll set
	if (epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, connection->fd, &ev) != 0)
	{
		perror("epoll_ctl");
		return false;
	}

	log_info("Got connection %d from %s on port %d", connection->fd, connection->hostname, connection->port);

	return true;
}

/**
 * Remove connection from server and free all its memory.
 *
 * TODO: Remove nicks and channels behind peer
 */
void Server_remove_connection(Server *serv, Connection *connection)
{
	assert(serv);
	assert(connection);

	ht_remove(serv->connections, &connection->fd, NULL, NULL);
	epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, connection->fd, NULL);

	if (connection->conn_type == USER_CONNECTION)
	{
		User *usr = connection->data;
		log_info("Closing connection with user %s", usr->nick);
		ht_remove(serv->nick_to_user_map, usr->nick, NULL, NULL);
		ht_remove(serv->nick_to_serv_name_map, usr->nick, NULL, NULL);

		// Remove user from channels
		for (size_t i = 0; i < Vector_size(usr->channels); i++)
		{
			char *name = Vector_get_at(usr->channels, i);
			Channel *channel = ht_get(serv->name_to_channel_map, name);
			if (channel)
			{
				Channel_remove_member(channel, usr);
			}
		}
		Server_broadcast_message(serv, Server_create_message(serv, "QUIT :%s", "closing link"));
		User_free(usr);
	}
	else if (connection->conn_type == PEER_CONNECTION)
	{
		Peer *peer = connection->data;
		log_info("Closing connection with peer %d", connection->fd);

		// Remove all servers behind quitting server
		if (peer->name)
		{
			// Remove all users behind quitting server
			HashtableIter itr;
			ht_iter_init(&itr, serv->nick_to_serv_name_map);
			char *nick = NULL;
			char *name = NULL;

			while (ht_iter_next(&itr, (void **)&nick, (void **)&name))
			{
				if (!strcmp(name, peer->name))
				{
					// TODO: Send quit message for user
					// Need to save user info
					// Server_broadcast_message(serv, User_create_message(serv, "QUIT :%s", "closing link"));
					if (!ht_iter_remove(&itr, NULL, NULL))
					{
						break;
					}
				}
			}

			ht_remove(serv->name_to_peer_map, peer->name, NULL, NULL);
			ht_iter_init(&itr, serv->name_to_peer_map);
			Peer *other_peer = NULL;
			name = NULL;
			while (ht_iter_next(&itr, (void **)&name, (void **)&other_peer))
			{
				if (!strcmp(other_peer->name, peer->name))
				{
					Server_broadcast_message(serv, Server_create_message(serv, "SQUIT %s :closing link", name));

					if (!ht_iter_remove(&itr, NULL, NULL))
					{
						break;
					}
				}
			}
		}

		Peer_free(peer);
	}

	Connection_free(connection);
}
