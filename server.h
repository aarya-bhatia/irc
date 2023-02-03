#pragma once

#include "common.h"
#include "net.h"
#include <log.h>
#include <pthread.h>
#include <collectc/cc_hashtable.h>
#include <collectc/cc_array.h>

#define MAX_CLIENTS 100
#define MAX_EVENTS 10
#define SERVPORT 5000
#define MAX_MSG_LEN 512
#define AVAIL -1

typedef struct Server {
	CC_HashTable *connections; // Map socket fd to data of type User*.
	struct sockaddr_in servaddr;
	int fd;
	int epollfd;
} Server;


typedef struct User 
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


void stop();
void *routine(void *args);
void main_loop();

