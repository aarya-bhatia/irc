#include "include/client.h"
#include "include/timer.h"
#include <sys/epoll.h>

#define DEBUG
#define PING_INTERVAL_SEC 10
#define EPOLL_TIMEOUT 1000
#define MAX_EPOLL_EVENTS 6

volatile bool alive = true;

// pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
		return 1;
	}

	pthread_t outbox_thread; /* This thread will send the messages from the outbox queue to the server */

	pthread_t inbox_thread; /* This thread will process messages from the inbox queue and display them to stdout */

	pthread_t reader_thread; /* This thread will read message from server add them to the inbox queue */

	Client client;

	Client_init(&client);

	char *host = argv[1];
	char *port = argv[2];

	client.client_sock = create_and_bind_socket(host, port);

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

	char *line = NULL;
	size_t line_len = 0;

	// Timer t;

	// Array to store the returned events from epoll_wait
	struct epoll_event events[5];

	// Create fd for epoll
	int epoll_fd = epoll_create1(0);
	CHECK(epoll_fd, "epoll_create1");

	// Create a timer fd to send PING messages
	// Timer_init(&t, PING_INTERVAL_SEC);

	// Add all fds to epoll set
	struct epoll_event ev;
	ev.events = EPOLLIN;

	ev.data.fd = STDIN_FILENO;
	CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &ev), "epoll_ctl");

	// ev.data.fd = t.timerfd;
	// CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, t.timerfd, &ev), "epoll_ctl");

	// Register signal handler to quit on CTRL-C
	signal(SIGINT, _signal_handler);

	// Register the client in IRC network
	char *registeration = make_string("NICK %s\r\nUSER %s * * :%s\r\n", client.client_nick, client.client_username, client.client_realname);

	queue_enqueue(client.client_outbox, registeration);

	pthread_create(&outbox_thread, NULL, outbox_thread_routine, &client);

	pthread_create(&inbox_thread, NULL, inbox_thread_routine, &client);

	pthread_create(&reader_thread, NULL, reader_thread_routine, &client);

	while (alive)
	{
		// Poll event set
		int nfd = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);

		if (nfd == -1)
		{
			perror("epoll_wait");
			exit(1);
		}
		if (nfd == 0)
		{
			// log_debug("No events occurred");
			continue;
		}

		// Check for events
		for (size_t i = 0; i < MAX_EPOLL_EVENTS; i++)
		{
			int fd = events[i].data.fd;
			int event = events[i].events;

			// stdin event
			if (fd == 0 && event & EPOLLIN)
			{
				// Read the next line till \n
				read_user_input(&line, &line_len);
				assert(line);

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

				continue;
			}

			// timer event
			// else if (fd == t.timerfd && event & EPOLLIN)
			// {
			// 	// check time since last message sent
			// 	// Only ping if idle
			// 	if (Timer_expired(&t) && stream.write_buf_len == 0 && difftime(time(NULL), stream.last_write_time) > PING_INTERVAL_SEC)
			// 	{
			// 		// Make ping request
			// 		IOStream_enqueue(&stream, strdup("PING\r\n"));
			// 		sleep(1);
			// 	}

			// 	continue;
			// }
		}
	}

	free(line);
	close(epoll_fd);

	queue_enqueue(client.client_inbox, strdup("exit"));
	queue_enqueue(client.client_outbox, strdup("exit"));

	pthread_join(inbox_thread, NULL);
	pthread_join(outbox_thread, NULL);

	// TODO: how to kill reader_thread?
	pthread_kill(reader_thread, SIGINT);
	pthread_join(reader_thread, NULL);

	// Timer_stop(&t);

	Client_destroy(&client);

	return 0;
}

void read_user_input(char **line, size_t *line_len)
{
	ssize_t nread = getline(line, line_len, stdin);

	if (nread == -1)
		die("getline");

	(*line)[nread] = 0;

	strcpy(*line + nread - 1, "\r\n");

	log_debug("%ld bytes read from stdin", nread);
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
