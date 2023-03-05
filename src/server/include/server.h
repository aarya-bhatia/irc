#pragma once

#include "include/message.h"
#include "types.h"

char *get_motd(char *fname);

Server *Server_create(int port);
void Server_destroy(Server *serv);
void Server_accept_all(Server *serv);
void Server_process_request(Server *serv, User *usr);

void Server_reply_to_NICK(Server *serv, User *usr, Message *msg);
void Server_reply_to_USER(Server *serv, User *usr, Message *msg);
void Server_reply_to_PRIVMSG(Server *serv, User *usr, Message *msg);
void Server_reply_to_PING(Server *serv, User *usr, Message *msg);
void Server_reply_to_QUIT(Server *serv, User *usr, Message *msg);
void Server_reply_to_MOTD(Server *serv, User *usr, Message *msg);

void Server_reply_to_WHO(Server *serv, User *usr, Message *msg);
void Server_reply_to_WHOIS(Server *serv, User *usr, Message *msg);

void Server_reply_to_JOIN(Server *serv, User *usr, Message *msg);
void Server_reply_to_LIST(Server *serv, User *usr, Message *msg);
void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg);

void Server_reply_to_SERVER(Server *serv, User *usr, Message *msg);
void Server_reply_to_PASS(Server *serv, User *usr, Message *msg);
void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg);

// implemented in user.c
bool Server_add_user(Server *, User *);
void Server_remove_user(Server *, User *);
