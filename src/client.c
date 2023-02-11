#include "include/common.h"
#include "include/message.h"
#include "include/aaryab2/queue.h"

#include <pthread.h>

// Todo: Make threads to read from server, and read from stdin
// Make thread safe queue to pull/push messages from/to server


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
