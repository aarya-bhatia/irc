#pragma once

#include "common.h"
#include "message.h"

#include "collectc/cc_array.h"
#include "collectc/cc_hashtable.h"
#include "collectc/cc_list.h"

#include <sys/epoll.h>

typedef struct _Server
{
	CC_HashTable *connections; // Map socket fd to User data
	struct sockaddr_in servaddr;
	int fd;
	int epollfd;
	char *hostname;
	char *port;
	char created_at[64];
} Server;

typedef struct _User
{
	int fd; 						// socket connection
	char *nick;
	char *username;
	char *realname;
	char *hostname;
	char req_buf[MAX_MSG_LEN + 1]; // the request message
	char res_buf[MAX_MSG_LEN + 1]; // the response message
	size_t req_len;				   // length of request buffer
	size_t res_len;				   // length of response buffer
	size_t res_off;				   // no of bytes of the response sent
	CC_List *msg_queue;			   // messages to be delivered to user
} User;

Server *Server_create(int port);
void Server_destroy(Server *serv);
void Server_accept_all(Server *serv);
void Server_process_request(Server *serv, User *usr);

void Server_reply_to_Nick(Server *serv, User *usr, Message *msg);
void Server_reply_to_User(Server *serv, User *usr, Message *msg);
void Server_reply_to_Privmsg(Server *serv, User *usr, Message *msg);

ssize_t User_Read_Event(Server *serv, User *usr);
ssize_t User_Write_Event(Server *serv, User *usr);
void User_Disconnect(Server *serv, User *usr);
void User_Destroy(User *usr);

void User_add_msg(User *usr, char *msg);