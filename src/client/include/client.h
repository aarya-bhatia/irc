#pragma once
#include "include/common.h"
#include "include/queue.h"
#include "include/message.h"
#include <pthread.h>

typedef struct Client
{
    char *serv_hostname;
    char *serv_port;

    char *client_nick;
    char *client_username;
    char *client_realname;

    int client_sock;

    queue_t *client_inbox;
    queue_t *client_outbox;
} Client;

void *reader_thread_routine(void *args);
void *inbox_thread_routine(void *args);
void *outbox_thread_routine(void *args);

void Client_init(Client *client);
void Client_destroy(Client *client);

void _signal_handler(int sig);
void _free_callback(void *data);
