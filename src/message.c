#include "message.h"

void message_init(Message *msg)
{
	assert(msg);
	memset(msg, 0, sizeof(Message));
}

void message_destroy(Message *msg)
{
	if(!msg) {return;}

	free(msg->origin);
	free(msg->command);
	free(msg->body);

	for(size_t i = 0; i < MAX_MSG_PARAM; i++) {
		free(msg->params[i]);
	}
}

CC_Array *parse_all_messages(char *str)
{
	assert(str);
	
	char *saveptr;
	char *tok = strtok_r(str,"\r\n",&saveptr);

	int ret;

	CC_Array *array;

	cc_array_new(&array);

	while(tok)
	{
		Message *msg = calloc(1, sizeof *msg);

		message_init(msg);

		if((ret = parse_message(tok, msg)) == -1) {
			log_warn("Invalid message");
			message_destroy(msg);
		}
		else {
			cc_array_add(array, msg);
		}

		tok = strtok_r(NULL,"\r\n",&saveptr);
	}

	return array;
}

int parse_message(char *str, Message *msg)
{
	assert(str);

	char *ptr = strstr(str, ":");

	if(ptr == str) {
		ptr = strstr(ptr+1, ":");
	}

	if(ptr) {
		msg->body = strdup(ptr+1);
		*ptr = 0;
	}

	char *saveptr;
	char *tok = strtok_r(str," ",&saveptr);

	if(!tok) {return -1;}

	// prefix
	if(tok[0] == ':') {
		msg->origin = strdup(tok);
		tok = strtok_r(NULL," ",&saveptr);
	}

	if(!tok) {
		return -1;
	}

	// command
	msg->command = strdup(tok);

	// params
	size_t i = 0;

	while(tok && i < 15) 
	{
		if((tok = strtok_r(NULL," ",&saveptr)) != NULL) {
			msg->params[i++] = strdup(tok);
		}
	}

	log_debug("Origin: %s", msg->origin);
	log_debug("Command: %s", msg->command);
	log_debug("Body: %s", msg->body);

	msg->n_params = i;

	for(size_t j = 0; j < i; j++) {
		log_debug("Param %d: %s", j+1, msg->params[j]);
	}

	return 0;
}
