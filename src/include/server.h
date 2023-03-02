#pragma once

#include "common.h"
#include "message.h"

#include "collectc/cc_array.h"
#include "collectc/cc_hashtable.h"
#include "collectc/cc_list.h"

#include <sys/epoll.h>

#define MAX_CHANNEL_COUNT 10

typedef struct _Server
{
	CC_HashTable *connections;		 // Map socket fd to User data object
	CC_HashTable *user_to_nicks_map; // Map username to array of nicks owned by user
	CC_HashTable *user_to_sock_map;	 // Map username to client socket
	CC_Array *channels;				 // List of channels
	struct sockaddr_in servaddr;	 // address info for server
	int fd;							 // listen socket
	int epollfd;					 // epoll fd
	char *hostname;					 // server hostname
	char *port;						 // server port
	char created_at[64];			 // server time created at as string
	char *motd_file;				 // file to use for message of the day greetings
} Server;

typedef struct _User
{
	int fd;						   // socket connection
	char *nick;					   // display name
	char *username;				   // unique identifier
	char *realname;				   // full name
	char *hostname;				   // client ip
	CC_List *msg_queue;			   // messages to be delivered to user
	CC_Array *memberships;		   // info about channels joined
	size_t req_len;				   // length of request buffer
	size_t res_len;				   // length of response buffer
	size_t res_off;				   // no of bytes of the response sent
	char req_buf[MAX_MSG_LEN + 1]; // the request message
	char res_buf[MAX_MSG_LEN + 1]; // the response message
	bool registered;			   // flag to indicate user has registered with username, realname and nick
	bool nick_changed;			   // flag to indicate user has set a nick
	bool quit;					   // flag to indicate user is leaving server
} User;

enum MODES
{
	MODE_NORMAL,
	MODE_AWAY,
	MODE_OPERATOR
};

typedef struct _Membership
{
	char *channel_name;
	int channel_mode;
	time_t date_joined;
} Membership;

typedef struct _Channel
{
	char *name; // name of channel
	char *topic;
	time_t time_created; // time channel was created
	int private;		 // by default channels are public
	CC_Array *members;	 // usernames of members in the channel
	int mode;			 // unused
	int user_limit;		 // unused
} Channel;

// Implemented in server.c
Server *Server_create(int port);
void Server_destroy(Server *serv);
void Server_accept_all(Server *serv);
void Server_process_request(Server *serv, User *usr);

void Server_reply_to_NICK(Server *serv, User *usr, Message *msg);
void Server_reply_to_USER(Server *serv, User *usr, Message *msg);
void Server_reply_to_PRIVMSG(Server *serv, User *usr, Message *msg);
void Server_reply_to_PING(Server *serv, User *usr, Message *msg);
void Server_reply_to_QUIT(Server *serv, User *usr, Message *msg);
void Server_reply_to_MOTD(Server *serv, User *usr, Message *msg);

void Server_reply_to_WHO(Server *serv, User *usr, Message *msg);
void Server_reply_to_WHOIS(Server *serv, User *usr, Message *msg);

void Server_reply_to_JOIN(Server *serv, User *usr, Message *msg);
void Server_reply_to_LIST(Server *serv, User *usr, Message *msg);
void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg);

void Server_reply_to_SERVER(Server *serv, User *usr, Message *msg);
void Server_reply_to_PASS(Server *serv, User *usr, Message *msg);
void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg);

CC_HashTable *load_nicks(const char *filename);
char *get_motd(char *fname);

// Implemented in user.c
ssize_t User_Read_Event(Server *serv, User *usr);
ssize_t User_Write_Event(Server *serv, User *usr);
void User_Disconnect(Server *serv, User *usr);
void User_Destroy(User *usr);
void User_add_msg(User *usr, char *msg);
void User_save_to_file(User *usr, const char *filename);
User *User_load_from_file(const char *filename);

// Implemented in channel.c
void Channel_destroy(Channel *this);

Channel *Server_get_channel(Server *serv, const char *name);
Channel *Server_add_channel(Server *serv, const char *name);
bool Server_remove_channel(Server *serv, const char *name);
bool Channel_has_member(Channel *this, const char *username);

void Channel_save_to_file(Channel *this, const char *filename);
Channel *Channel_load_from_file(const char *filename);
bool Channel_add_member(Channel *this, const char *username);
bool Channel_remove_member(Channel *this, const char *username);

// Implemented in register.c
bool check_registration_complete(Server *serv, User *usr);
CC_Array *get_current_nicks(Server *serv, User *usr);
bool update_nick_map(Server *serv, User *usr);
bool check_nick_available(Server *serv, User *usr, char *nick);
