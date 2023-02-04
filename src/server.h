#pragma once

#include "common.h"
#include "message.h"

typedef struct _Server
{
	CC_HashTable *connections; // Map socket fd to data of type User*.
	struct sockaddr_in servaddr;
	int fd;
	int epollfd;
	char *hostname;
	char *port;
} Server;

typedef struct _User
{
	int fd;						   // socket connection
	char *nick;					   // nickname
	char *name;					   // username
	char req_buf[MAX_MSG_LEN + 1]; // the request message
	char res_buf[MAX_MSG_LEN + 1]; // the response message
	size_t req_len;				   // length of request buffer
	size_t res_len;				   // length of response buffer
	size_t res_off;				   // no of bytes of the response sent
	CC_Array *inbox;			   // string array of requests
	CC_Array *outbox;			   // string array of responses

} User;

void quit(Server *serv);

Server *start_server(int port);
void accept_new_connections(Server *serv);

void client_read_event(Server *serv, User *usr);
void client_write_event(Server *serv, User *usr);
void client_disconnect(Server *serv, User *usr);

void user_destroy(User *usr);

void do_NICK(Server *serv, User *usr, Message *msg, char **res);
void do_USER(Server *serv, User *usr, Message *msg, char **res);
void do_PRIVMSG(Server *serv, User *usr, Message *msg, char **res);
