#pragma once

#include "include/common.h"
#include "include/hashtable.h"
#include "include/list.h"
#include "include/message.h"
#include "include/vector.h"

#define MOTD_FILENAME "./data/motd.txt"
#define CONFIG_FILENAME "./config.csv"
#define MAX_CHANNEL_COUNT 8
#define MAX_CHANNEL_USERS 8
#define CHANNELS_FILENAME "./data/channels.txt"
#define DEFAULT_INFO "development irc server"

/* Add server prefix and \r\n suffix to messages */
#define Server_create_message(serv, format, ...) \
  make_string(":%s " format "\r\n", serv->name, __VA_ARGS__)

/* Add user prefix and \r\n suffix to messages */
#define User_create_message(usr, format, ...)                       \
  make_string(":%s!%s@%s " format "\r\n", usr->nick, usr->username, \
              usr->hostname, __VA_ARGS__)

typedef enum _conn_type_t {
	UNKNOWN_CONNECTION,
	USER_CONNECTION,
	PEER_CONNECTION
} conn_type_t;

typedef struct _Connection {
	int fd;
	conn_type_t conn_type;
	char *hostname;
	int port;
	bool quit;
	size_t req_len;		// request buffer length
	size_t res_len;		// response buffer length
	size_t res_off;		// num bytes sent from response buffer
	char req_buf[MAX_MSG_LEN + 1];	// request buffer
	char res_buf[MAX_MSG_LEN + 1];	// response buffer
	List *incoming_messages;	// queue of received messages
	List *outgoing_messages;	// queue of messages to deliver
	void *data;		// additional data for users and peers
} Connection;

typedef struct _Server {
	struct sockaddr_in servaddr;	// address info for server
	int fd;			// listen socket
	int epollfd;		// epoll fd
	char *name;		// name of this server
	char *hostname;		// server hostname
	char *port;		// server port
	char created_at[64];	// server time created at as string
	char *motd_file;	// file to use for message of the day greetings
	char *config_file;	// name of config file with irc server address and passwords
	char *passwd;
	char *info;

	Hashtable *connections;	// map sock to Connection struct
	Hashtable *nick_to_user_map;	// Map nick to user struct on this server
	Hashtable *name_to_peer_map;	// Map server name to peer struct
	Hashtable *name_to_channel_map;	// Map channel name to channel struct
	Hashtable *nick_to_serv_name_map;	// Map nick to name of server which has user

} Server;

typedef struct _Peer {
	char *name;
	bool registered;
	bool quit;		// flag to indicate server leaving
	List *msg_queue;
	Vector *nicks;		// nick of users behind this server

	enum { ACTIVE_SERVER, PASSIVE_SERVER } server_type;
} Peer;

typedef struct peer_info_t {
	char *peer_name;
	char *peer_host;
	char *peer_port;
	char *peer_passwd;
} peer_info_t;

enum {
	USER_ONLINE,
	USER_OFFLINE
};

typedef struct _User {
	char *nick;		// display name
	char *username;		// unique identifier
	char *realname;		// full name
	char *hostname;		// client ip
	Vector *channels;	// list of channels joined by user
	bool registered;	// flag to indicate user has registered with username, realname and nick
	bool nick_changed;	// flag to indicate user has set a nick
	bool quit;		// flag to indicate user is leaving server
	int status;		// to indicate if user online or offline
	List *msg_queue;
} User;

typedef struct _Channel {
	char *name;		// name of channel
	char *topic;		// channel topic
	int mode;		// channel mode
	time_t time_created;	// time channel was created
	Hashtable *members;	// map username to User struct

	// time_t topic_changed_at;
	// char *topic_changed_by;
} Channel;

struct help_t {
	const char *subject;
	const char *title;
	const char *body;
};

Server *Server_create(const char *name);
void Server_destroy(Server * serv);
void Server_accept_all(Server * serv);
void Server_process_request(Server * serv, Connection * usr);

void Server_flush_message_queues(Server * serv);

void Server_process_request_from_unknown(Server * serv, Connection * conn);
void Server_process_request_from_user(Server * serv, Connection * conn);
void Server_process_request_from_peer(Server * serv, Connection * conn);

void Server_message_channel(Server * serv, const char *origin, const char *target, const char *message);
void Server_message_user(Server * serv, const char *origin, const char *target, const char *message);
void Server_relay_message(Server * serv, const char *origin, const char *message);

bool Server_add_connection(Server * serv, Connection * connection);
void Server_remove_connection(Server * serv, Connection * connection);

void Server_handle_NICK(Server * serv, User * usr, Message * msg);
void Server_handle_USER(Server * serv, User * usr, Message * msg);
void Server_handle_PRIVMSG(Server * serv, User * usr, Message * msg);
void Server_handle_NOTICE(Server * serv, User * usr, Message * msg);
void Server_handle_PING(Server * serv, User * usr, Message * msg);
void Server_handle_QUIT(Server * serv, User * usr, Message * msg);
void Server_handle_MOTD(Server * serv, User * usr, Message * msg);
void Server_handle_INFO(Server * serv, User * usr, Message * msg);
void Server_handle_WHO(Server * serv, User * usr, Message * msg);
void Server_handle_JOIN(Server * serv, User * usr, Message * msg);
void Server_handle_PART(Server * serv, User * usr, Message * msg);
void Server_handle_TOPIC(Server * serv, User * usr, Message * msg);
void Server_handle_LIST(Server * serv, User * usr, Message * msg);
void Server_handle_NAMES(Server * serv, User * usr, Message * msg);
void Server_handle_CONNECT(Server * serv, User * usr, Message * msg);
void Server_handle_LUSERS(Server * serv, User * usr, Message * msg);
void Server_handle_HELP(Server * serv, User * usr, Message * msg);

void Server_handle_SERVER(Server * serv, Peer * peer, Message * msg);
void Server_handle_PASS(Server * serv, Peer * peer, Message * msg);

User *User_alloc();
void User_free(User * this);
bool User_is_member(User * usr, const char *channel_name);
void User_add_channel(User * usr, const char *channel_name);
bool User_remove_channel(User * usr, const char *channel_name);

Peer *Peer_alloc();
void Peer_free(Peer *);
Hashtable *load_peers(const char *config_filename);
char *get_server_passwd(const char *config_filename, const char *name);
bool get_peer_info(const char *filename, const char *name, struct peer_info_t *info);

Connection *Connection_alloc(int fd, struct sockaddr *addr, socklen_t addrlen);
void Connection_free(Connection * this);
ssize_t Connection_read(Connection *);
ssize_t Connection_write(Connection *);

Hashtable *load_channels(const char *filename);
void save_channels(Hashtable * hashtable, const char *filename);
Channel *Channel_alloc(const char *name);
void Channel_free(Channel * this);
void Channel_add_member(Channel * this, User *);
bool Channel_remove_member(Channel * this, User *);
bool Channel_has_member(Channel * this, User *);

const struct help_t *get_help_text(const char *subject);
char *get_motd(char *fname);
