#pragma once

#include "include/common.h"
#include "include/hashtable.h"
#include "include/list.h"
#include "include/vector.h"

enum MODES {
    MODE_NORMAL,
    MODE_AWAY,
    MODE_OPERATOR
};

typedef struct _Server {
    Hashtable *sock_to_user_map;              // Maps socket to user struct for every connected user
    Hashtable *username_to_user_map;          // Map username to user struct for registered users
    Hashtable *online_nick_to_username_map;   // Map nick to username of online user
    Hashtable *offline_nick_to_username_map;  // Map nick to username of offline user
    Hashtable *channels_map;                  // Map channel name to channel struct pointer
    struct sockaddr_in servaddr;              // address info for server
    int fd;                                   // listen socket
    int epollfd;                              // epoll fd
    char *hostname;                           // server hostname
    char *port;                               // server port
    char created_at[64];                      // server time created at as string
    char *motd_file;                          // file to use for message of the day greetings
} Server;

typedef struct _User {
    int fd;                         // socket connection
    char *nick;                     // display name
    char *username;                 // unique identifier
    char *realname;                 // full name
    char *hostname;                 // client ip
    List *msg_queue;                // messages to be delivered to user
    Vector *channels;               // list of channels joined by user
    size_t req_len;                 // length of request buffer
    size_t res_len;                 // length of response buffer
    size_t res_off;                 // no of bytes of the response sent
    char req_buf[MAX_MSG_LEN + 1];  // the request message
    char res_buf[MAX_MSG_LEN + 1];  // the response message
    bool registered;                // flag to indicate user has registered with username, realname and nick
    bool nick_changed;              // flag to indicate user has set a nick
    bool quit;                      // flag to indicate user is leaving server
} User;

typedef struct _Membership {
    char *username;  // username of member
    char *channel;   // channel name
    int mode;        // member mode
} Membership;

typedef struct _Channel {
    char *name;   // name of channel
    char *topic;  // channel topic
    int mode;             // channel mode
    time_t time_created;  // time channel was created
    Vector *members;      // usernames of members in the channel
						  
    // time_t topic_changed_at;
    // char *topic_changed_by;
} Channel;
