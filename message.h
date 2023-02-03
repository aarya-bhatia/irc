#pragma once

#include "log.h"
#include "common.h"

typedef struct Message
{
	char *origin;
	char *command;
	char *body;
	char *params[MAX_MSG_PARAM];
} Message;

void message_init(Message *msg);
void message_destroy(Message *msg);

void parse_messages(char *str);
int parse_message(char *str, Message *msg);

