#include "common.h"

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

	int nread;

	// Send user nickname to server and read reply
	sprintf(servmsg, "NICK %s\r\n", nick);
	if (write(sock, servmsg, strlen(servmsg)) == -1)
		die("write");

	// Recv username, realname as input
	sprintf(prompt, "Enter username >");
	write(1, prompt, strlen(prompt));
	scanf("%s", username);

	sprintf(prompt, "Enter realname >");
	write(1, prompt, strlen(prompt));
	scanf("%s", realname);

	// Send user name to server and read reply
	sprintf(servmsg, "USER %s * * :%s\r\n", username, realname);
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