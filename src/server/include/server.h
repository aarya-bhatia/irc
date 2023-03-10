#pragma once

#include "include/message.h"
#include "types.h"

#define MOTD_FILENAME "./data/motd.txt"

const struct help_t *get_help_text(const char *subject);

// server.c
char *get_motd(char *fname);

Server *Server_create(int port);
void Server_destroy(Server *serv);
void Server_accept_all(Server *serv);
void Server_process_request(Server *serv, User *usr);
void Server_broadcast_message(Server *serv, const char *message);
void Server_broadcast_to_channel(Server *serv, Channel *channel, const char *message);

User *Server_get_user_by_socket(Server *serv, int sock);
User *Server_get_user_by_nick(Server *serv, const char *nick);
User *Server_get_user_by_username(Server *serv, const char *username);

// server_reply.c
bool check_registration_complete(Server *serv, User *usr);

void Server_reply_to_NICK(Server *serv, User *usr, Message *msg);
void Server_reply_to_USER(Server *serv, User *usr, Message *msg);
void Server_reply_to_PRIVMSG(Server *serv, User *usr, Message *msg);
void Server_reply_to_NOTICE(Server *serv, User *usr, Message *msg);
void Server_reply_to_PING(Server *serv, User *usr, Message *msg);
void Server_reply_to_QUIT(Server *serv, User *usr, Message *msg);
void Server_reply_to_MOTD(Server *serv, User *usr, Message *msg);
void Server_reply_to_WHO(Server *serv, User *usr, Message *msg);
void Server_reply_to_WHOIS(Server *serv, User *usr, Message *msg);
void Server_reply_to_JOIN(Server *serv, User *usr, Message *msg);
void Server_reply_to_PART(Server *serv, User *usr, Message *msg);
void Server_reply_to_TOPIC(Server *serv, User *usr, Message *msg);
void Server_reply_to_LIST(Server *serv, User *usr, Message *msg);
void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg);
void Server_reply_to_SERVER(Server *serv, User *usr, Message *msg);
void Server_reply_to_PASS(Server *serv, User *usr, Message *msg);
void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg);
void Server_reply_to_LUSERS(Server *serv, User *usr, Message *msg);
void Server_reply_to_HELP(Server *serv, User *usr, Message *msg);

// middleware.c
bool Server_registered_middleware(Server *serv, User *usr, Message *msg);
bool Server_channel_middleware(Server *serv, User *usr, Message *msg);

// user.c
bool Server_add_user(Server *, User *);
void Server_remove_user(Server *, User *);
