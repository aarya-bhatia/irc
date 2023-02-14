#include "client.h"

volatile bool g_is_running = true;

char *g_line = NULL;
size_t g_line_len = 0;

time_t g_last_request;

char g_client_nick[30] = {0};
char g_client_username[30] = {0};
char g_client_realname[30] = {0};
int g_client_sock = -1;

// message queue to store outgoing messages
static CC_List *g_queue = NULL;

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
		return 1;
	}

	g_client_sock = create_and_bind_socket(argv[1], argv[2]);

	cc_list_new(&g_queue);

	g_last_request = time(NULL);

	// Create fd for epoll
	int epoll_fd = epoll_create1(0);
	CHECK(epoll_fd, "epoll_create1");

	// Create a timer fd to send PING messages

	struct itimerspec tv = {
		.it_interval.tv_sec = PING_INTERVAL_SEC,
		.it_interval.tv_nsec = 0,
		.it_value.tv_sec = PING_INTERVAL_SEC,
		.it_value.tv_nsec = 0};

	int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);

	if (timerfd_settime(timerfd, 0, &tv, NULL) != 0)
		die("timerfd_settime");

	CHECK(timerfd, "create_timer");

	// Add all fds to epoll set
	struct epoll_event ev;

	ev.data.fd = 0;
	ev.events = EPOLLIN;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &ev) == 0)
		log_info("Added stdin to epoll set");
	else
		die("epoll_ctl");

	ev.data.fd = timerfd;
	ev.events = EPOLLIN;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timerfd, &ev) == 0)
		log_info("Added timer (%d) to epoll set", timerfd);
	else
		die("epoll_ctl");

	ev.data.fd = g_client_sock;
	ev.events = EPOLLIN | EPOLLOUT;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, g_client_sock, &ev) == 0)
		log_info("Added socket (%d) to epoll set", g_client_sock);
	else
		die("epoll_ctl");

	char read_buf[MAX_MSG_LEN + 1];	 // buffer to store bytes being read from the server
	char write_buf[MAX_MSG_LEN + 1]; // buffer to store bytes being sent to the server

	size_t read_buf_len = 0;  // size of the read buffer
	size_t write_buf_len = 0; // size of the write buffer
	size_t write_buf_off = 0; // position in the write buffer

	// Array to store the returned events from epoll_wait
	struct epoll_event events[5];

	// Register signal handler to quit on CTRL-C
	void _signal_handler(int sig);
	signal(SIGINT, _signal_handler);

	// Register the client in IRC network
#ifdef DEBUG
	strcpy(g_client_nick, "aaryab2");
	strcpy(g_client_username, "aarya.bhatia1678");
	strcpy(g_client_realname, "Aarya Bhatia");

#else
	char *msg1 = "Enter your nick > ";
	char *msg2 = "Enter your username > ";
	char *msg3 = "Enter your realname > ";

	write(1, msg1, strlen(msg1));
	scanf("%s", g_client_nick);

	write(1, msg2, strlen(msg2));
	scanf("%s", g_client_username);

	write(1, msg3, strlen(msg3));
	scanf("%s", g_client_realname);
#endif

	char *initial_requests = NULL;
	asprintf(&initial_requests, "NICK %s\r\nUSER %s * * :%s\r\n", g_client_nick, g_client_username, g_client_realname);
	cc_list_add_last(g_queue, initial_requests);

	while (g_is_running)
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
			log_debug("No events occurred");
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
				read_user_input();
				assert(g_line);
				cc_list_add_last(g_queue, strdup(g_line));
			}

			// timer event
			else if (fd == timerfd && event & EPOLLIN)
			{
				uint64_t t = 0;
				int ret;

				// Timer expired
				if ((ret = read(timerfd, &t, 8)) > 0)
				{
					log_debug("Timer expired at %d", (int)time(NULL));
					// check time since last message sent
					// Only ping if idle
					if (write_buf_len == 0 && difftime(time(NULL), g_last_request) > PING_INTERVAL_SEC)
					{
						// Make ping request
						cc_list_add_last(g_queue, strdup("PING\r\n"));
						sleep(1);
					}
				}
				else if (ret == -1)
				{
					perror("read");
				}
			}

			// socket event
			else if (fd == g_client_sock)
			{
				if (event & (EPOLLERR | EPOLLHUP))
				{
					die("Server disconnected");
				}

				// socket is ready to send data i.e. client can read
				if (event & EPOLLIN)
				{
					read_from_socket(g_client_sock, read_buf, &read_buf_len);
				}

				// socket is ready to recieve data i.e. client can write data
				if (event & EPOLLOUT)
				{
					write_to_socket(g_client_sock, write_buf, &write_buf_len, &write_buf_off);
				}
			}
		}
	}

	free(g_line);

	void _free_callback(void *msg);
	cc_list_destroy_cb(g_queue, _free_callback);

	close(epoll_fd);
	close(timerfd);

	shutdown(g_client_sock, SHUT_RDWR);
	close(g_client_sock);

	return 0;
}

void read_from_socket(int sock, char read_buf[], size_t *read_buf_len)
{
	ssize_t nread = read(sock, read_buf + *read_buf_len, MAX_MSG_LEN - *read_buf_len);

	if (nread == -1)
		die("read");

	read_buf[nread] = 0;

	// Check if entire message was recieved
	char *msg_end = strstr(read_buf + *read_buf_len, "\r\n");

	if (msg_end)
	{
		// Print message from server to stdout
		log_info("Server: %s", read_buf);

		size_t pending_bytes = strlen(msg_end + 2);

		// check if there are more messages in buffer
		if (pending_bytes > 0)
		{
			// copy partial message to beginning of buffer
			memmove(read_buf, msg_end + 2, pending_bytes);
			read_buf[pending_bytes] = 0;
			*read_buf_len = strlen(read_buf);
		}
		else
		{
			// Reset buffer to be empty
			*read_buf_len = 0;
		}
	}
	else
	{
		*read_buf_len += nread;
	}
}

void write_to_socket(int sock, char write_buf[], size_t *write_buf_len, size_t *write_buf_off)
{
	// Some bytes of request remain to be sent
	if (*write_buf_len > 0 && *write_buf_off < *write_buf_len)
	{
		ssize_t nwrite = write(sock, write_buf + *write_buf_off, *write_buf_len - *write_buf_off);
		if (nwrite == -1)
			die("write");

		*write_buf_off += nwrite;

		// All bytes of request were sent to server
		if (*write_buf_off >= *write_buf_len)
		{
			*write_buf_off = *write_buf_len = 0;
			g_last_request = time(NULL);
		}

		return;
	}

	// No more messages
	if (cc_list_size(g_queue) == 0)
	{
		return;
	}

	// Pop a message from queue
	char *message;
	cc_list_remove_first(g_queue, (void **)&message);

	assert(message);
	assert(strlen(message) <= MAX_MSG_LEN);

	// Copy the message to request buffer
	strcpy(write_buf, message);
	*write_buf_len = strlen(message);
	*write_buf_off = 0;
	free(message);
}

void read_user_input()
{
	ssize_t nread = getline(&g_line, &g_line_len, stdin);

	if (nread == -1)
		die("getline");

	g_line[nread] = 0;

	strcpy(g_line + nread - 1, "\r\n");

	log_debug("Bytes read from stdin: %ld", nread);
	puts(g_line);
}

void _signal_handler(int sig)
{
	(void)sig;
	g_is_running = false;
}

void _free_callback(void *msg)
{
	free(msg);
}
