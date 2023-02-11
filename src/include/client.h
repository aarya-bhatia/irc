#pragma once

#include "include/common.h"
#include "include/message.h"
#include "include/aaryab2/thread_queue.h"

typedef struct Client {
	thread_queue *inbox;
	thread_queue *outbox;	
	char nick[30];
	char username[30];
	char realname[30];
	int sock;

} Client;

void Client_init(Client *client, char *hostname, char *port);
void Client_destroy(Client *client);

void *start_inbox_thread(void *client);
void *start_outbox_thread(void *client);
