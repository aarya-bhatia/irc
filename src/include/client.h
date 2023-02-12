#pragma once

#include "include/common.h"
#include "include/message.h"
#include "include/aaryab2/thread_queue.h"

typedef struct Client {
	char nick[30];
	char username[30];
	char realname[30];
	int sock;
	thread_queue *queue;	
} Client;

void Client_init(Client *client);
void Client_destroy(Client *client);

void Client_connect(Client *client, char *hostname, char *port);
void Client_disconnect(Client *client);

void *start_reader_thread(void *client);
void *start_writer_thread(void *client);
