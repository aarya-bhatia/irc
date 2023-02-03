#pragma once

#include "common.h"
#include "message.h"

typedef struct _Server {
	CC_HashTable *connections; // Map socket fd to data of type User*.
	struct sockaddr_in servaddr;
	int fd;
	int epollfd;
} Server;

typedef struct _User 
{
	int fd; // socket connection
	char *nick; // nickname
	char *name; // username
	char req_buf[MAX_MSG_LEN + 1]; // the request message
	char res_buf[MAX_MSG_LEN + 1]; // the response message
	size_t req_len; // length of request buffer
	size_t res_len; // length of response buffer
	size_t nsent; // number of bytes in request buffer recieved from user
	size_t nrecv; // number of bytes in response buffer sent from server
} User;
