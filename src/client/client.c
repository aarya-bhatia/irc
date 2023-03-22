#include "include/client.h"
#include "include/queue.h"

#define DEBUG
#define PING_INTERVAL_SEC 10
#define MAX_EPOLL_EVENTS 6

static volatile bool alive = true;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_stdout = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
		return 1;
	}

	pthread_t outbox_thread; /* This thread will send the
							  * messages from the outbox queue
							  * to the server */
	pthread_t inbox_thread;	 /* This thread will process
							  * messages from the inbox queue
							  * and display them to stdout */
	pthread_t reader_thread; /* This thread will read message
							  * from server add them to the
							  * inbox queue */

	Client client;

	client.client_inbox = queue_alloc();
	client.client_outbox = queue_alloc();

	// establish connection with the server
	client.client_sock = create_and_bind_socket(argv[1], argv[2]);

	// non blocking socket
	fcntl(client.client_sock, F_SETFL,
		  fcntl(client.client_sock, F_GETFL) | O_NONBLOCK);

	pthread_create(&outbox_thread, NULL, outbox_thread_routine, &client);
	pthread_create(&inbox_thread, NULL, inbox_thread_routine, &client);
	pthread_create(&reader_thread, NULL, reader_thread_routine, &client);

	if (argc == 6)
	{
		char *nick = argv[3];
		char *username = argv[4];
		char *realname = argv[5];
		queue_enqueue(client.client_outbox,
					  make_string("NICK %s\r\nUSER %s * * :%s\r\n",
								  nick, username, realname));
	}

	char *line = NULL;	 // buffer to store user input
	size_t line_len = 0; // length of line buffer

	while (alive)
	{
		// Read the next line till \n
		ssize_t nread = getline(&line, &line_len, stdin);
		if (nread == -1)
			die("getline");
		line[nread - 1] = 0; // remove newline character

		// Empty
		if (strlen(line) == 0)
		{
			continue;
		}

		if (strlen(line) > MAX_MSG_LEN)
		{
			SAFE(mutex_stdout, { log_error("Message is greater than %d bytes",
										   MAX_MSG_LEN); });
			continue;
		}
		// Send message to server
		queue_enqueue(client.client_outbox,
					  make_string("%s\r\n", line));

		if (!strncmp(line, "QUIT", 4))
		{
			alive = false;
			break;
		}
	}

	free(line);

	SAFE(mutex_stdout, { log_debug("joining theads"); });

	pthread_join(inbox_thread, NULL);
	SAFE(mutex_stdout, { log_debug("inbox thread quit"); });

	pthread_join(outbox_thread, NULL);
	SAFE(mutex_stdout, { log_debug("outbox thread quit"); });

	pthread_join(reader_thread, NULL);
	SAFE(mutex_stdout, { log_debug("reader thread quit"); });

	queue_free(client.client_inbox, _free_callback);
	queue_free(client.client_outbox, _free_callback);

	shutdown(client.client_sock, SHUT_RDWR);
	close(client.client_sock);

	printf("Goodbye!\n");

	return 0;
}

void _free_callback(void *data)
{
	free(data);
}

void _signal_handler(int sig)
{
	(void)sig;
	log_debug("alive: %d", alive);
	alive = false;
}
