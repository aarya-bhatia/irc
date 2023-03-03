#pragma once

#include "common.h"

typedef struct _Server Server;

enum MODES
{
	MODE_NORMAL,
	MODE_AWAY,
	MODE_OPERATOR
};

typedef struct _Membership
{
	char *username;
	char *channel;
	int mode;
} Membership;

typedef struct _Channel
{
	char *name; // name of channel
	char *topic;
	int mode;
	int user_limit;
	time_t time_created; // time channel was created
	CC_Array *members;	 // usernames of members in the channel
} Channel;


void Channel_destroy(Channel *this);

Channel *Channel_create(const char *name);
Channel *Server_get_channel(Server *serv, const char *name);
bool Server_remove_channel(Server *serv, const char *name);
bool Channel_has_member(Channel *this, const char *username);

void Channel_save_to_file(Channel *this, const char *filename);
Channel *Channel_load_from_file(const char *filename);
bool Channel_add_member(Channel *this, const char *username);
bool Channel_remove_member(Channel *this, const char *username);

