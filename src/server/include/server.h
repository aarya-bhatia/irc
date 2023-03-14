#pragma once

#include "include/common.h"
#include "include/hashtable.h"
#include "include/list.h"
#include "include/message.h"
#include "include/vector.h"

#define MOTD_FILENAME "./data/motd.txt"
#define MAX_CHANNEL_COUNT 10
#define MAX_CHANNEL_USERS 5
#define CHANNELS_FILENAME "./data/channels.txt"

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
    size_t req_len;                 // request buffer length
    size_t res_len;                 // response buffer length
    size_t res_off;                 // num bytes sent from response buffer
    char req_buf[MAX_MSG_LEN + 1];  // request buffer
    char res_buf[MAX_MSG_LEN + 1];  // response buffer
    List *incoming_messages;        // queue of received messages
    List *outgoing_messages;        // queue of messages to deliver
    void *data;                     // additional data for users and peers
} Connection;

typedef struct _Server {
    Hashtable *connections;  // map sock to Connection struct

    Hashtable *name_to_peer_map;              // Map irc server name to IRCNode struct
    Hashtable *username_to_user_map;          // Map username to user struct for registered users
    Hashtable *online_nick_to_username_map;   // Map nick to username of online user
    Hashtable *offline_nick_to_username_map;  // Map nick to username of offline user
    Hashtable *channels_map;                  // Map channel name to channel struct pointer

    struct sockaddr_in servaddr;  // address info for server
    int fd;                       // listen socket
    int epollfd;                  // epoll fd
    char *name;                   // name of this server
    char *hostname;               // server hostname
    char *port;                   // server port
    char created_at[64];          // server time created at as string
    char *motd_file;              // file to use for message of the day greetings
    char *config_file;            // name of config file with irc server address and passwords
} Server;

typedef struct _Peer {
    char *name;
    bool quit;           // flag to indicate server leaving
    bool authenticated;  // flag to check if remote server has authenticated with password
} Peer;

enum { USER_ONLINE,
       USER_OFFLINE };

typedef struct _User {
    char *nick;         // display name
    char *username;     // unique identifier
    char *realname;     // full name
    char *hostname;     // client ip
    Vector *channels;   // list of channels joined by user
    bool registered;    // flag to indicate user has registered with username, realname and nick
    bool nick_changed;  // flag to indicate user has set a nick
    bool quit;          // flag to indicate user is leaving server
    int status;         // to indicate if user online or offline
} User;

enum MODES {
    MODE_NORMAL,
    MODE_AWAY,
    MODE_OPERATOR
};

typedef struct _Membership {
    char *username;  // username of member
    char *channel;   // channel name
    int mode;        // member mode
} Membership;

typedef struct _Channel {
    char *name;           // name of channel
    char *topic;          // channel topic
    int mode;             // channel mode
    time_t time_created;  // time channel was created
    Vector *members;      // usernames of members in the channel
    Vector *connections;

    // time_t topic_changed_at;
    // char *topic_changed_by;
} Channel;

struct rpl_handle_t {
    const char *name;
    void (*function)(Server *, Connection *, Message *);
};

struct help_t {
    const char *subject;
    const char *title;
    const char *body;
};

// help.c
const struct help_t *get_help_text(const char *subject);

// server.c
char *get_motd(char *fname);

Server *Server_create(int port);
void Server_destroy(Server *serv);
void Server_accept_all(Server *serv);
void Server_process_request(Server *serv, User *usr);
void Server_broadcast_message(Server *serv, const char *message);
void Server_broadcast_to_channel(Server *serv, Channel *channel, const char *message);

User *Server_get_user_by_socket(Server *serv, int sock);
User *Server_get_user_by_nick(Server *serv, const char *nick);
User *Server_get_user_by_username(Server *serv, const char *username);

// server_reply.c
bool check_registration_complete(Server *serv, User *usr);

void Server_reply_to_NICK(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_USER(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_PRIVMSG(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_NOTICE(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_PING(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_QUIT(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_MOTD(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_WHO(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_WHOIS(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_JOIN(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_PART(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_TOPIC(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_LIST(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_NAMES(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_SERVER(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_PASS(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_CONNECT(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_LUSERS(Server *serv, Connection *conn, Message *msg);
void Server_reply_to_HELP(Server *serv, Connection *conn, Message *msg);

bool Server_registered_middleware(Server *serv, Connection *conn, Message *msg);
bool Server_channel_middleware(Server *serv, Connection *conn, Message *msg);

bool Server_add_user(Server *, User *);
void Server_remove_user(Server *, User *);

User *User_alloc();
void User_free(User *this);
bool User_is_member(User *usr, const char *channel_name);
void User_add_channel(User *usr, const char *channel_name);
bool User_remove_channel(User *usr, const char *channel_name);

Peer *Peer_alloc();
void Peer_free(Peer *);

Membership *Membership_alloc(const char *channel, const char *username, int mode);
void Membership_free(Membership *membership);

Connection *Connection_alloc(int fd, struct sockaddr *addr, socklen_t addrlen);
void Connection_free(Connection *this);
ssize_t Connection_read(Connection *);
ssize_t Connection_write(Connection *);

Hashtable *load_channels(const char *filename);
void save_channels(Hashtable *hashtable, const char *filename);
Channel *Channel_alloc(const char *name);
void Channel_free(Channel *this);
void Channel_add_member(Channel *this, const char *username);
bool Channel_remove_member(Channel *this, const char *username);
bool Channel_has_member(Channel *this, const char *username);
