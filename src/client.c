#include "include/client.h"
#include "include/timer.h"
#include "include/queue.h"
#include <sys/epoll.h>

#define DEBUG
#define PING_INTERVAL_SEC 10
#define EPOLL_TIMEOUT 1000
#define MAX_EPOLL_EVENTS 6

volatile bool alive = true;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
		return 1;
	}

	pthread_t outbox_thread; /* This thread will send the messages from the outbox queue to the server */
	pthread_t inbox_thread;	 /* This thread will process messages from the inbox queue and display them to stdout */
	pthread_t reader_thread; /* This thread will read message from server add them to the inbox queue */

	Client client;
	Client_init(&client);

	char *host = argv[1];
	char *port = argv[2];

	// establish connection with the server
	client.client_sock = create_and_bind_socket(host, port); // TODO: should socket be non blocking?

	// Get nick, username and realname of client
#ifdef DEBUG
	strcpy(client.client_nick, "aaryab2");
	strcpy(client.client_username, "aarya.bhatia1678");
	strcpy(client.client_realname, "Aarya Bhatia");

#else
	char *msg1 = "Enter your nick > ";
	char *msg2 = "Enter your username > ";
	char *msg3 = "Enter your realname > ";

	write(1, msg1, strlen(msg1));
	scanf("%s", client.client_nick);

	write(1, msg2, strlen(msg2));
	scanf("%s", client.client_username);

	write(1, msg3, strlen(msg3));
	scanf("%s", client.client_realname);
#endif

	// Register signal handler to quit on CTRL-C
	signal(SIGINT, _signal_handler);

	// Register the client in IRC network
	char *registration = make_string("NICK %s\r\nUSER %s * * :%s\r\n", client.client_nick, client.client_username, client.client_realname);
	queue_enqueue(client.client_outbox, registration);

	pthread_create(&outbox_thread, NULL, outbox_thread_routine, &client);
	pthread_create(&inbox_thread, NULL, inbox_thread_routine, &client);
	pthread_create(&reader_thread, NULL, reader_thread_routine, &client);

	sleep(1); // let threads start and send registration message

	char *line = NULL;	 // buffer to store user input
	size_t line_len = 0; // length of line buffer

	while (alive)
	{
		// Read the next line till \n
		printf("Enter command\n");
		ssize_t nread = getline(&line, &line_len, stdin);

		if (nread == -1)
			die("getline");

		line[nread] = 0;

		strcpy(line + nread - 1, "\r\n");

		log_debug("%ld bytes read from stdin", nread);

		if (strlen(line) > MAX_MSG_LEN)
		{
			log_error("Message is greater than %d bytes", MAX_MSG_LEN);
			continue;
		}

		if (!strcmp(line, "exit"))
		{
			alive = false;
			break;
		}

		queue_enqueue(client.client_inbox, make_string("%s\r\n"));
	}

	free(line);

	// This message will allow the inbox/outbox thread to quit safely
	queue_enqueue(client.client_inbox, strdup("exit"));
	queue_enqueue(client.client_outbox, strdup("exit"));

	pthread_join(inbox_thread, NULL);
	pthread_join(outbox_thread, NULL);

	// TODO: How to kill reader_thread? It may be blocked on a read call
	pthread_kill(reader_thread, SIGINT);
	pthread_join(reader_thread, NULL);

	Client_destroy(&client);

	return 0;
}

void _free_callback(void *data)
{
	free(data);
}

void _signal_handler(int sig)
{
	(void)sig;
	alive = false;
}

void Client_init(Client *client)
{
	queue_init(client->client_inbox);
	queue_init(client->client_outbox);
}

void Client_destroy(Client *client)
{
	queue_destroy(client->client_inbox, _free_callback);
	queue_destroy(client->client_outbox, _free_callback);

	shutdown(client->client_sock, SHUT_RDWR);
	close(client->client_sock);
}
