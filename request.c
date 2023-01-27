#define _POSIX_C_SOURCE 200809L

#include "request.h"

#include <string.h>

void request_init(Request *req)
{
	assert(req);
	memset(req, 0, sizeof(Request));
}

void request_destroy(Request *req)
{
	if(!req) {return;}

	free(req->origin);
	free(req->command);
	free(req->body);

	for(size_t i = 0; i < MAX_MSG_PARAM; i++) {
		free(req->params[i]);
	}
}

void parse_message(char *msg)
{
	assert(msg);
	
	char *saveptr;
	char *tok = strtok_r(msg,"\r\n",&saveptr);

	while(tok)
	{
		Request req;

		request_init(&req);
		int ret = parse_request(tok, &req);

		if(ret == -1) {
			log_warn("Invalid request");
		}

		request_destroy(&req);

		tok = strtok_r(NULL,"\r\n",&saveptr);
	}
}

int parse_request(char *msg, Request *req)
{
	assert(msg);

	char *saveptr;
	char *tok = strtok_r(msg," ",&saveptr);

	if(!tok) {return -1;}

	if(tok[0] == ':') {
		req->origin = strdup(tok);
		tok = strtok_r(NULL," ",&saveptr);
		if(!tok) {
			return -1;
		}
		req->command = strdup(tok);
	} else {
		req->command = strdup(tok);
	}

	size_t i = 0;

	while(tok && i < 15) 
	{
		if((tok = strtok_r(NULL," ",&saveptr)) != NULL) {
			if(tok[0] == ':') {
				req->body = strdup(tok+1);
				break;
			}

			req->params[i++] = strdup(tok);
		}
	}

	log_debug("Origin: %s", req->origin);
	log_debug("Command: %s", req->command);
	log_debug("Body: %s", req->body);

	for(size_t j = 0; j < i; j++) {
		log_debug("Param %d: %s", j+1, req->params[j]);
	}

	return 0;
}

int main()
{
	char msg[] = "NICK aarya\r\n";
	char msg2[] = "USER aarya * * :Aarya Bhatia\r\n";
	char msg3[] = "NICK aarya\r\nUSER aarya * * :Aarya Bhatia\r\n";

	parse_message(msg);
	parse_message(msg2);
	parse_message(msg3);

	return 0;
}

