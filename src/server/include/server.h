#pragma once

#include "include/common.h"
#include "include/message.h"

#include "collectc/cc_array.h"
#include "collectc/cc_hashtable.h"
#include "collectc/cc_list.h"

#include <sys/epoll.h>

#define MAX_CHANNEL_COUNT 10
#define MAX_CHANNEL_USERS 5

#define MOTD_FILENAME "./data/motd.txt"
#define NICKS_FILENAME "./data/nicks.txt"
#define CHANNELS_DIRNAME "./data/channels"

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

CC_HashTable *load_nicks(const char *filename);
char *get_motd(char *fname);

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

