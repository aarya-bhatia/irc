#pragma once

#include "common.h"
#include "server.h"

#define RPL_WELCOME ":%s 001 %s :Welcome to the IRC Network, %s\r\n"
#define RPL_YOURHOST ":%s 002 %s :Your host is %s\r\n"
#define RPL_CREATED ":%s 003 :%s :This server was created :%s\r\n"
#define RPL_MYINFO ":%s %s %s 1 ao mtov\r\n"

char *Reply_Welcome(Server *serv, User *usr);
char *Reply_Your_Host(Server *serv, User *usr);
char *Reply_Created(Server *serv, User *usr);
char *Reply_My_Info(Server *serv, User *usr);
