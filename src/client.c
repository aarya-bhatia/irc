// #define _GNU_SOURCE

// Standard Libraries
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Networking
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

#include "common.h"
#include "message.h"

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
		return 1;
	}

	char *hostname = argv[1];
	char *port = argv[2];
	int sock = create_and_bind_socket(hostname, port);

	char nick[30];
	char username[30];
	char realname[30];

	char prompt[MAX_MSG_LEN];
	char servmsg[MAX_MSG_LEN];

	int nread;

	// Input nick, username and realname
	sprintf(prompt, "Enter your nick > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", nick);

	sprintf(prompt, "Enter your username > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", username);

	sprintf(prompt, "Enter your realname > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", realname);

	// Register client

	sprintf(servmsg, "NICK %s\r\nUSER %s * * :%s\r\n", nick, username, realname);

	if (write(sock, servmsg, strlen(servmsg)) == -1)
		die("write");

	nread = read(sock, servmsg, MAX_MSG_LEN);

	if (nread == -1)
		die("read");

	servmsg[nread] = 0;

	puts(servmsg);

	close(sock);
	log_info("Goodbye!");

	return 0;
}