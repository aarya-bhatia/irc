
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
};

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

	serv->connections = ht_alloc_type(INT_TYPE, SHALLOW_TYPE);		   /* Map<int, Connection *> */
	serv->name_to_peer_map = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE); /* Map<string,  Peer *> */
	serv->nick_to_serv_name_map = ht_alloc(STRING_TYPE, STRING_TYPE);  /* Map<string, string> */
	serv->nick_to_user_map = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE); /* Map<string, User*> */
	serv->name_to_channel_map = load_channels(CHANNELS_FILENAME);	   /* Map<string, Channel *> */

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

	log_info("Server %s is running on port %s at %s", serv->name, serv->port, serv->hostname);
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
		List_push_back(conn->outgoing_messages,
					   Server_create_message(serv,
											 "ERROR :Closing Link: %s (%s)",
											 conn->hostname, reason));
		conn->quit = true;
	}
	else if (strncmp(message, "NICK", 4) == 0 || strncmp(message, "USER", 4) == 0)
	{
		conn->conn_type = USER_CONNECTION;
		conn->data = User_alloc();
		((User *)conn->data)->hostname = strdup(conn->hostname);
	}
	else if (strncmp(message, "PASS", 4) == 0 || strncmp(message, "SERVER", 6) == 0)
	{
		conn->conn_type = PEER_CONNECTION;
		conn->data = Peer_alloc(ACTIVE_SERVER);
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

	log_debug("Processing %zu messages from connection %d", Vector_size(messages), conn->fd);

	// Iterate over the request messages and add response message(s) to user's message queue in the same order.
	for (size_t i = 0; i < Vector_size(messages); i++)
	{
		Message *message = Vector_get_at(messages, i);
		assert(message);

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
				List_push_back(member->msg_queue, strdup(message));
			}
		}
	}

	Server_relay_message(serv, origin, message);
}

/**
 * Attemps to send a message to user with given nick.
 * If user is not on this server, message is relayed to its peers.
 */
void Server_message_user(Server *serv, const char *origin, const char *target, const char *message)
{
	User *user = ht_get(serv->nick_to_user_map, target);

	if (user)
	{
		if (user->registered && !user->quit)
		{
			log_debug("Sent message to user %s on server %s", user->nick, serv->name);
			List_push_back(user->msg_queue, strdup(message));
		}
	}
	else
	{
		Server_relay_message(serv, origin, message);
	}
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
	while (ht_iter_next(&itr, NULL, (void **)&peer))
	{
		if (strcmp(peer->name, origin) != 0 && peer->registered && !peer->quit)
		{
			List_push_back(peer->msg_queue, strdup(message));
		}
		c++;
	}
	log_debug("Relayed message to %d peers on server %s", c, serv->name);
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
		else if (!strcmp(message->command, "SERVER"))
		{
			if(!peer->registered) {
				Server_handle_SERVER(serv, peer, message);
			} else {
				Server_relay_message(serv, peer->name, message->message);
			}
		}
		else if (!strcmp(message->command, "PASS"))
		{
			Server_handle_PASS(serv, peer, message);
		}
		else if (!strcmp(message->command, "NICK")) // A new user was registered on peer server
		{
			ht_set(serv->nick_to_serv_name_map, message->params[0], peer->name);
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
		else if (!strcmp(message->command, "JOIN"))
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
		Server_relay_message(serv, serv->name, Server_create_message(serv, "QUIT :%s", "closing link"));
		User_free(usr);
	}
	else if (connection->conn_type == PEER_CONNECTION)
	{
		Peer *peer = connection->data;
		log_info("Closing connection with peer %d", connection->fd);
		if (peer->name)
		{
			ht_remove(serv->name_to_peer_map, peer->name, NULL, NULL);
		}

		for (size_t i = 0; i < Vector_size(peer->nicks); i++)
		{
			char *nick = Vector_get_at(peer->nicks, i);
			ht_remove(serv->nick_to_serv_name_map, nick, NULL, NULL);
		}
		Server_relay_message(serv, serv->name, Server_create_message(serv, "SQUIT %s :%s", peer->name, "closing link"));
		Peer_free(peer);
	}

	Connection_free(connection);
}
