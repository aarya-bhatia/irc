#pragma once

#include "include/common.h"
#include "include/message.h"
#include "include/aaryab2/thread_queue.h"

#include <pthread.h>

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

// The irc client
extern Client g_client;

// The reader thread will read messages from server and print them to stdout
extern pthread_t g_reader_thread;

// The writer thread will pull messages from client queue and send them to server
extern pthread_t g_writer_thread;

// Send messages from threads to stdout through pipe
// extern int g_pipe[2];