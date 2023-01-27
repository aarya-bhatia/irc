#pragma once

#include "log.h"
#include "common.h"

typedef struct Request
{
	char *origin;
	char *command;
	char *body;
	char *params[MAX_MSG_PARAM];
} Request;

void request_init(Request *req);
void request_destroy(Request *req);

void parse_message(char *msg);
int parse_request(char *msg, Request *req);


