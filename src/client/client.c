#include "include/client.h"
#include "include/queue.h"
#include <sys/epoll.h>

#define DEBUG
#define PING_INTERVAL_SEC 10
#define EPOLL_TIMEOUT 1000
#define MAX_EPOLL_EVENTS 6

static volatile bool alive = true;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_stdout = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr,
			"Usage: %s <hostname> <port> <nick> <userame> <realname>\n",
			*argv);
		return 1;
	}

	pthread_t outbox_thread;	/* This thread will send the messages from the outbox queue to the server */
	pthread_t inbox_thread;	/* This thread will process messages from the inbox queue and display them to stdout */
	pthread_t reader_thread;	/* This thread will read message from server add them to the inbox queue */

	Client client;
	Client_init(&client);

	char *host = argv[1];
	char *port = argv[2];

	// establish connection with the server
	client.client_sock = create_and_bind_socket(host, port);

	// Make client socket non blocking
	fcntl(client.client_sock, F_SETFL,
	      fcntl(client.client_sock, F_GETFL) | O_NONBLOCK);

	if (argc >= 4) {
		strcpy(client.client_nick, argv[3]);
	} else {
		strcpy(client.client_nick, "aaryab2");
	}

	if (argc >= 5) {
		strcpy(client.client_username, argv[4]);
	} else {
		strcpy(client.client_username, "aarya.bhatia1678");
	}

	if (argc >= 6) {
		strcpy(client.client_realname, argv[5]);
	} else {
		strcpy(client.client_realname, "Aarya Bhatia");
	}

	// Register signal handler to quit on CTRL-C
	// struct sigaction sa;
	// sigemptyset(&sa.sa_mask);
	// sa.sa_handler = _signal_handler;
	// sa.sa_flags = SA_RESTART;

	// if (sigaction(SIGINT, &sa, NULL) == -1)
	//      die("sigaction");

	// Register the client in IRC network
	char *registration =
	    make_string("NICK %s\r\nUSER %s * * :%s\r\n", client.client_nick,
			client.client_username, client.client_realname);
	queue_enqueue(client.client_outbox, registration);

	pthread_create(&outbox_thread, NULL, outbox_thread_routine, &client);
	pthread_create(&inbox_thread, NULL, inbox_thread_routine, &client);
	pthread_create(&reader_thread, NULL, reader_thread_routine, &client);

	sleep(1);		// let threads start and send registration message

	char *line = NULL;	// buffer to store user input
	size_t line_len = 0;	// length of line buffer

	while (alive) {
		// Read the next line till \n
		ssize_t nread = getline(&line, &line_len, stdin);
		if (nread == -1)
			die("getline");
		line[nread - 1] = 0;	// remove newline character

		// Empty
		if (strlen(line) == 0) {
			continue;
		}

		if (strlen(line) > MAX_MSG_LEN) {
			SAFE(mutex_stdout, {
			     log_error("Message is greater than %d bytes",
				       MAX_MSG_LEN);
			     }
			);
			continue;
		}
		// Send message to server
		queue_enqueue(client.client_outbox,
			      make_string("%s\r\n", line));

		if (!strncmp(line, "QUIT", 4)) {
			alive = false;
			break;
		}
	}

	free(line);

	SAFE(mutex_stdout, {
	     log_debug("joining theads");
	     }
	);

	pthread_join(inbox_thread, NULL);
	SAFE(mutex_stdout, {
	     log_debug("inbox thread quit");
	     }
	);

	pthread_join(outbox_thread, NULL);
	SAFE(mutex_stdout, {
	     log_debug("outbox thread quit");
	     }
	);

	pthread_join(reader_thread, NULL);
	SAFE(mutex_stdout, {
	     log_debug("reader thread quit");
	     }
	);

	Client_destroy(&client);
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

void Client_init(Client * client)
{
	memset(client, 0, sizeof *client);

	client->client_inbox = calloc(1, sizeof *client->client_inbox);
	client->client_outbox = calloc(1, sizeof *client->client_outbox);

	queue_init(client->client_inbox);
	queue_init(client->client_outbox);
}

void Client_destroy(Client * client)
{
	queue_destroy(client->client_inbox, _free_callback);
	queue_destroy(client->client_outbox, _free_callback);

	free(client->client_inbox);
	free(client->client_outbox);

	shutdown(client->client_sock, SHUT_RDWR);
	close(client->client_sock);
}
