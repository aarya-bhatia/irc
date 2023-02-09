#include "common.h"

int create_and_bind_socket(char *hostname, char *port);

void recv_input(char prompt[], char input[])
{
	write(1, prompt, strlen(prompt));
	scanf("%s", input);
}

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		fprintf(stderr, "Usage: %s <hostname> <port> <nick>\n", *argv);
		return 1;
	}

	char *hostname = argv[1];
	char *port = argv[2];
	char *nick = argv[3];

	int sock = create_and_bind_socket(hostname, port);

	char username[30];
	char realname[30];

	char prompt[MAX_MSG_LEN];
	char servmsg[MAX_MSG_LEN];

	int write_status, read_status;

	///////////////// REGISTRATION ///////////////////

	// Send user nickname to server
	sprintf(servmsg, "NICK %s\r\n", nick);
	write_all(sock, servmsg, strlen(servmsg));

	// read reply
	read_all(sock, servmsg, MAX_MSG_LEN);
	puts(servmsg);

	// Recv user name and realname as input
	sprintf(prompt, "Enter username >");
	recv_input(prompt, username);

	sprintf(prompt, "Enter realname >");
	recv_input(prompt, realname);

	// Send user name to server
	sprintf(servmsg, "USER %s * * :%s\r\n", username, realname);
	write_all(sock, servmsg, strlen(servmsg));

	// Get server reply
	read_all(sock, servmsg, MAX_MSG_LEN);
	puts(servmsg);

	// Exit
	close(sock);
	log_info("Goodbye!");
	return 0;
}

int create_and_bind_socket(char *hostname, char *port)
{
	struct addrinfo hints, *servinfo = NULL;

	memset(&hints, 0, sizeof hints);

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(hostname, port, &hints, &servinfo) != 0)
		die("getaddrinfo");

	int sock = socket(servinfo->ai_family,
					  servinfo->ai_socktype,
					  servinfo->ai_protocol);

	if (sock < 0)
		die("socket");

	if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
		die("connect");

	freeaddrinfo(servinfo);

	log_info("Connection with server is established...");

	return sock;
}