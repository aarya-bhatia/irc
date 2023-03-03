#pragma once

#include "include/common.h"
#include "collectc/cc_array.h"
#include "collectc/cc_hashtable.h"
#include "collectc/cc_list.h"

enum MODES
{
	MODE_NORMAL,
	MODE_AWAY,
	MODE_OPERATOR
};

typedef struct _Server
{
	CC_HashTable *connections;		 // Map socket fd to User data object
	CC_HashTable *user_to_nicks_map; // Map username to array of nicks owned by user
	CC_HashTable *user_to_sock_map;	 // Map username to client socket
	CC_Array *channels;				 // List of channels
	struct sockaddr_in servaddr;	 // address info for server
	int fd;							 // listen socket
	int epollfd;					 // epoll fd
	char *hostname;					 // server hostname
	char *port;						 // server port
	char created_at[64];			 // server time created at as string
	char *motd_file;				 // file to use for message of the day greetings
} Server;

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

