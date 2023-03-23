#include "include/common.h"
#include "include/list.h"
#include "include/message.h"
#include "include/connection.h"

#include <pthread.h>
#include <sys/epoll.h>

#define DEBUG
#define EPOLL_TIMEOUT 2500 // Seconds
extern pthread_mutex_t mutex_stdout;
#define PING_INTERVAL_SEC 10

typedef struct Client
{
	const char *hostname;
	const char *port;
	Connection *conn;
	pthread_t thread;
	pthread_mutex_t mutex;
} Client;

static volatile bool alive = true;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_stdout = PTHREAD_MUTEX_INITIALIZER;

void _signal_handler(int sig);

void *client_thread(void *args)
{
	Client *client = (Client *)args;
	assert(client);
	assert(client->conn);

	int epollfd = epoll_create1(0);
	struct epoll_event events[1];

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT;
	ev.data.fd = client->conn->fd;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, client->conn->fd, &ev);

	bool quit = false;

	while (!quit)
	{
		int nfd = epoll_wait(epollfd, events, 1, EPOLL_TIMEOUT);

		if (nfd == -1)
		{
			log_error("epoll_wait: %s", strerror(errno));
			break;
		}

		// No events polled
		if (nfd == 0)
		{
			continue;
		}

		// Server disconnect
		if (events[0].events & (EPOLLERR | EPOLLHUP))
		{
			SAFE(mutex_stdout, { log_info("Server disconnect"); });
			break;
		}

		if (events[0].events & EPOLLIN)
		{
			if (Connection_read(client->conn) == -1)
			{
				die(NULL);
			}

			size_t num_msg = 0;

			while (List_size(client->conn->incoming_messages))
			{
				char *message = List_peek_front(client->conn->incoming_messages);
				SAFE(mutex_stdout, { puts(message); });
				List_pop_front(client->conn->incoming_messages);
				num_msg++;

				if (strstr(message, "ERROR"))
				{
					quit = true;
					break;
				}
			}

			log_debug("Received %zu messages from server", num_msg);
		}

		if (events[0].events & EPOLLOUT)
		{
			pthread_mutex_lock(&client->mutex);

			if (Connection_write(client->conn) == -1)
			{
				die(NULL);
			}

			pthread_mutex_unlock(&client->mutex);
		}
	}

	close(epollfd);
	SAFE(mutex_stdout, { log_info("client thread has quit!"); });
	return client;
}

void client_add_message(Client *client, char *message)
{
	pthread_mutex_lock(&client->mutex);
	List_push_back(client->conn->outgoing_messages, message);
	pthread_mutex_unlock(&client->mutex);
}

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <server> <hostname> <port>\n", *argv);
		return 1;
	}

	Client client;
	memset(&client, 0, sizeof client);
	client.hostname = argv[1];
	client.port = argv[2];

	// establish connection with the server
	int client_sock = create_and_bind_socket(argv[1], argv[2]);
	fcntl(client_sock, F_SETFL, fcntl(client_sock, F_GETFL) | O_NONBLOCK);

	client.conn = calloc(1, sizeof *client.conn);
	client.conn->fd = client_sock;
	client.conn->conn_type = CLIENT_CONNECTION;
	client.conn->incoming_messages = List_alloc(NULL, free);
	client.conn->outgoing_messages = List_alloc(NULL, free);

	pthread_create(&client.thread, NULL, client_thread, &client);

	if (argc == 6)
	{
		char *nick = argv[3];
		char *username = argv[4];
		char *realname = argv[5];

		client_add_message(&client, make_string("NICK %s\r\nUSER %s * * :%s\r\n", nick, username, realname));
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
			SAFE(mutex_stdout, { log_error("Message is greater than %d bytes", MAX_MSG_LEN); });
			continue;
		}

		if (!strncmp(line, "/server ", strlen("/server "))) // register as server using the specified name
		{
			char *name = strstr(line, " ") + 1;
			SAFE(mutex_stdout, { log_info("Registering as server %s", name); });
			client_add_message(&client, make_string("PASS erwin@1234\r\n", line));
			client_add_message(&client, make_string("SERVER %s\r\n", name));
		}
		else if (!strncmp(line, "/client ", strlen("/client "))) // register as user using the specified client file
		{
			char *fname = strstr(line, " ") + 1;

			FILE *fptr = fopen(fname, "r");

			if (!fptr)
			{
				SAFE(mutex_stdout, { log_error("client file not found: %s", fname); });
				continue;
			}

			char *contents = NULL;
			size_t cap = 0;
			ssize_t nread = getline(&contents, &cap, fptr);

			if (nread == -1)
				die("getline");

			char *realname = strstr(contents, ":");

			if (!realname)
			{
				SAFE(mutex_stdout, { log_error("realname not given"); });
				continue;
			}

			*realname = 0;
			realname += 1;

			char *nick = strtok(contents, " ");

			if (!nick)
			{
				SAFE(mutex_stdout, { log_error("nick not given"); });
				continue;
			}

			char *username = strtok(NULL, " ");

			if (!username)
			{
				SAFE(mutex_stdout, { log_error("username not given"); });
				continue;
			}

			SAFE(mutex_stdout, { log_info("Registering as user: nick=%s, username=%s, realname=%s", nick, username, realname); });

			client_add_message(&client, make_string("NICK %s\r\n", nick));
			client_add_message(&client, make_string("USER %s * * :%s\r\n", username, realname));

			free(contents);
			fclose(fptr);
		}
		else
		{
			client_add_message(&client, make_string("%s\r\n", line));
		}

		if (strstr(line, "QUIT"))
		{
			log_warn("quit");
			alive = false;
			break;
		}
	}

	free(line);
	pthread_join(client.thread, NULL);
	Connection_free(client.conn);
	printf("Goodbye!\n");
	return 0;
}

void _signal_handler(int sig)
{
	(void)sig;
	log_debug("alive: %d", alive);
	alive = false;
}
