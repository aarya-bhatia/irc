#pragma once

#include "common.h"

typedef struct _Message
{
	char *origin;
	char *command;
	char *params[MAX_MSG_PARAM];
	char *body;
} Message;

void message_init(Message *msg);
void message_destroy(Message *msg);
int parse_message(char *str, Message *msg);
CC_Array *parse_all_messages(char *str);

