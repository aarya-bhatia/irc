#include "include/client.h"

// static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

void Client_init(Client *client)
{
	assert(client);

	*(client->nick) = 0;
	*(client->username) = 0;
	*(client->realname) = 0;

	client->queue = thread_queue_create(-1);
	client->sock = -1;
}

void Client_connect(Client *client, char *hostname, char *port)
{
	assert(client);
	assert(hostname);
	assert(port);

	if (client->sock != -1)
	{
		Client_disconnect(client);
	}

	client->sock = create_and_bind_socket(hostname, port);
}

void Client_disconnect(Client *client)
{
	assert(client);

	if (client->sock != -1)
	{
		shutdown(client->sock, SHUT_RDWR);
		close(client->sock);
	}
}

void Client_destroy(Client *client)
{
	if (!client)
	{
		return;
	}

	Client_disconnect(client);
	thread_queue_destroy(client->queue);
}

void *start_reader_thread(void *args)
{
	Client *client = (Client *)args;

	char buf[MAX_MSG_LEN + 1];

	while (1)
	{
		int nread = read(client->sock, buf, MAX_MSG_LEN);

		if (nread == -1)
			die("read");

		buf[nread] = 0;

		if (nread >= 2 && !strncmp(buf + nread - 2, "\r\n", 2))
		{
			buf[nread - 2] = 0;
		}

		char *str = "Server > "; 
		write(1, str, strlen(str));
		write(1, buf, strlen(buf));
		write(1, "\n", 1);
	}

	return (void *)client;
}

void *start_writer_thread(void *args)
{
	Client *client = (Client *)args;

	while (1)
	{
		char *msg = thread_queue_pull(client->queue);

		int written = write(client->sock, msg, strlen(msg));

		if (written == -1)
			die("write");

		// printf("Outbox: Sent %d bytes to server", written);

		free(msg);
	}

	return (void *)client;
}
