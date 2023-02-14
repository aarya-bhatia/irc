#pragma once

#include "include/common.h"
#include "include/message.h"

typedef struct Client
{
	char nick[30];
	char username[30];
	char realname[30];
	int sock;
} Client;

void Client_init(Client *client);
void Client_destroy(Client *client);
void Client_connect(Client *client, char *hostname, char *port);
void Client_disconnect(Client *client);