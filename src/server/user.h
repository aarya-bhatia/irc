#pragma once

#include "common.h"

typedef struct _Server Server;
typedef struct _Channel Channel;

typedef struct _User
{
	int fd;						   // socket connection
	char *nick;					   // display name
	char *username;				   // unique identifier
	char *realname;				   // full name
	char *hostname;				   // client ip
	CC_List *msg_queue;			   // messages to be delivered to user
	CC_Array *memberships;		   // info about channels joined
	size_t req_len;				   // length of request buffer
	size_t res_len;				   // length of response buffer
	size_t res_off;				   // no of bytes of the response sent
	char req_buf[MAX_MSG_LEN + 1]; // the request message
	char res_buf[MAX_MSG_LEN + 1]; // the response message
	bool registered;			   // flag to indicate user has registered with username, realname and nick
	bool nick_changed;			   // flag to indicate user has set a nick
	bool quit;					   // flag to indicate user is leaving server
} User;

ssize_t User_Read_Event(Server *serv, User *usr);
ssize_t User_Write_Event(Server *serv, User *usr);
void User_Disconnect(Server *serv, User *usr);
void User_Destroy(User *usr);
void User_add_msg(User *usr, char *msg);
void User_save_to_file(User *usr, const char *filename);
User *User_load_from_file(const char *filename);

bool User_is_member(User *usr, const char *channel);
void User_leave_channel(User *usr, Channel *channel);
void User_join_channel(User *usr, Channel *channel);

