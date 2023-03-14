#pragma once

#include "common.h"
#include "vector.h"

typedef struct _Message
{
	char *origin;
	char *command;
	char *params[MAX_MSG_PARAM];
	char *body;
	size_t n_params;
} Message;

void message_init(Message *msg);
void message_destroy(Message *msg);
int parse_message(char *str, Message *msg);
Vector *parse_all_messages(char *str);
Vector *parse_message_list(List *list);