#include "include/client.h"

#include <pthread.h>

// Todo: Make threads to read from server, and read from stdin
// Make thread safe queue to pull/push messages from/to server

static Client g_client;
static pthread_t inbox_thread;
static pthread_t outbox_thread;

void stop(int sig)
{
	(void)sig;

	pthread_cancel(inbox_thread);
	pthread_cancel(outbox_thread);

	pthread_join(inbox_thread, NULL);
	pthread_join(outbox_thread, NULL);

	Client_destroy(&g_client);

	log_info("Goodbye!");
	exit(0);
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
		return 1;
	}

	Client_init(&g_client, argv[1], argv[2]);

	pthread_create(&inbox_thread, NULL, start_inbox_thread, (void *)&g_client);
	pthread_create(&outbox_thread, NULL, start_outbox_thread, (void *)&g_client);

	char prompt[MAX_MSG_LEN];
	char servmsg[MAX_MSG_LEN];

	signal(SIGINT, stop);

	// Register client
	sprintf(prompt, "Enter your nick > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", g_client.nick);
	sprintf(prompt, "Enter your username > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", g_client.username);
	sprintf(prompt, "Enter your realname > ");
	write(1, prompt, strlen(prompt));
	scanf("%s", g_client.realname);

	sprintf(servmsg, "NICK %s\r\nUSER %s * * :%s\r\n", g_client.nick, g_client.username, g_client.realname);

	thread_queue_push(g_client.outbox, strdup(servmsg));

	char *line = NULL;
	size_t cap = 0;
	ssize_t ret;

	while (1)
	{
		printf("Enter command > ");
		if ((ret = getline(&line, &cap, stdin)) <= 0)
		{
			break;
		}

		if (line[ret - 1] == '\n')
			line[ret - 1] = 0;
		else
			line[ret] = 0;
	}

	stop(0);
	exit(0);
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

void *start_inbox_thread(void *args)
{
	Client *client = (Client *)args;

	char buf[MAX_MSG_LEN + 1];

	while (1)
	{
		int nread = read(client->sock, buf, MAX_MSG_LEN);
		if (nread == -1)
			die("read");

		buf[nread] = 0;

		log_info("Message from server: %s", buf);

		// log_info("Inbox: Read %d bytes from server", nread);
		// thread_queue_push(client->inbox, strdup(buf));
	}

	return (void *)client;
}

void *start_outbox_thread(void *args)
{
	Client *client = (Client *)args;

	while (1)
	{
		char *msg = (char *)thread_queue_pull(client->outbox);
		int written = write(client->sock, msg, strlen(msg));
		if (written == -1)
			die("write");

		log_info("Outbox: Sent %d bytes to server", written);
		free(msg);
	}

	return (void *)client;
}
