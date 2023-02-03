#pragma once

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <log.h>

#define MAX_MSG_LEN 512
#define MAX_MSG_PARAM 15
#define CRLF "\r\n"

/*
 
enum ReqType
{
	NICK,
	USER,
	PRIVMSG
};

enum ResType
{
	RPL_WELCOME=1,
};

enum ErrorType
{
	ERR_NICKNAMEINUSE=433
};

struct Client
{
	struct sockaddr_in client_addr;
	struct sockaddr_in serv_addr;
	socklen_t client_addrlen;	
	socklen_t serv_addrlen;	
	int client_sock;
	int server_sock;
};

struct User
{
	char *username;
	char *nickname;
	char *hostname;
	struct sockaddr_in addr;
};

struct Server
{
	list<Server> neighbours;
	list<User> users;
	list<Client> clients;
	int server_sock;
};

char *get_user_identifier_string(User *u)
{
	char *s = NULL;

	asprintf(&s, "%s!%s@%s", 
			u->nickname, 
			u->username, 
			u->hostname);

	return s;
}


*/
