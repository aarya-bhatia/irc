#include "include/client.h"

#include <pthread.h>

// Todo: Make threads to read from server, and read from stdin
// Make thread safe queue to pull/push messages from/to server

static Client g_client;

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
		return 1;
	}

	Client_init(&g_client, argv[1], argv[2]);

	char prompt[MAX_MSG_LEN];
	char servmsg[MAX_MSG_LEN];

	int nread;

	// Input nick, username and realname
	sprintf(prompt, "Enter your nick > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", g_client.nick);

	sprintf(prompt, "Enter your username > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", g_client.username);

	sprintf(prompt, "Enter your realname > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", g_client.realname);

	// Register client

	sprintf(servmsg, "NICK %s\r\nUSER %s * * :%s\r\n", g_client.nick, g_client.username, g_client.realname);

	if (write(g_client.sock, servmsg, strlen(servmsg)) == -1)
		die("write");

	nread = read(g_client.sock, servmsg, MAX_MSG_LEN);

	if (nread == -1)
		die("read");

	servmsg[nread] = 0;

	puts(servmsg);

	Client_destroy(&g_client);

	log_info("Goodbye!");

	return 0;
}

void Client_init(Client *client, char *hostname, char *port)
{
	assert(client);
	assert(hostname);
	assert(port);

	memset(client, 0, sizeof *client);

	client->inbox = thread_queue_create(-1);
	client->outbox = thread_queue_create(-1);

	client->sock = create_and_bind_socket(hostname, port);
}

void Client_destroy(Client *client)
{
	if (!client)
	{
		return;
	}

	thread_queue_destroy(client->inbox);
	thread_queue_destroy(client->outbox);

	shutdown(client->sock, SHUT_RDWR);
	close(client->sock);
}