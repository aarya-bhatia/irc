#include "include/client.h"
#include <sys/epoll.h>
#include <sys/timerfd.h>

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
		pthread_mutex_lock(&g_mutex);
		if (!g_is_running)
		{
			pthread_mutex_unlock(&g_mutex);
			break;
		}
		pthread_mutex_unlock(&g_mutex);

		int nread = read(client->sock, buf, MAX_MSG_LEN);

		if (nread == -1)
			die("read");

		buf[nread] = 0;

		// if (nread >= 2 && !strncmp(buf + nread - 2, "\r\n", 2))
		// {
		// 	buf[nread - 2] = 0;
		// }

		pthread_mutex_lock(&g_mutex);
		write(1, "Server: ", strlen("Server: "));
		write(1, buf, strlen(buf));
		// write(1, "\n", 1);
		pthread_mutex_unlock(&g_mutex);
	}

	return (void *)client;
}

void *start_writer_thread(void *args)
{
	Client *client = (Client *)args;

	while (1)
	{
		pthread_mutex_lock(&g_mutex);
		if (!g_is_running)
		{
			pthread_mutex_unlock(&g_mutex);
			break;
		}
		pthread_mutex_unlock(&g_mutex);

		char *msg = thread_queue_pull(client->queue);

		int written = write(client->sock, msg, strlen(msg));

		if (written == -1)
			die("write");

		pthread_mutex_lock(&g_mutex);
		log_info("Outbox: Sent %d bytes to server", written);
		g_last_request = time(NULL);
		pthread_mutex_unlock(&g_mutex);

		free(msg);
	}

	return (void *)client;
}

void *start_ping_thread(void *args)
{
	Client *client = (Client *)args;

	// create a timer

	int epollfd = epoll_create1(0);

	struct epoll_event events[10];

	struct itimerspec tv = {
		.it_interval.tv_sec = 3,
		.it_interval.tv_nsec = 0,
		.it_value.tv_sec = 3,
		.it_value.tv_nsec = 0};

	int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);

	struct epoll_event ev = {.data.fd = timerfd, .events = EPOLLIN};
	epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &ev);

	if (timerfd_settime(timerfd, 0, &tv, NULL) != 0)
		die("timerfd_settime");

	while (1)
	{
		pthread_mutex_lock(&g_mutex);
		if (!g_is_running)
		{
			pthread_mutex_unlock(&g_mutex);
			break;
		}
		pthread_mutex_unlock(&g_mutex);

		int n = epoll_wait(epollfd, events, sizeof events, 1000);

		if (n == 0)
		{
			continue;
		}

		uint64_t t = 0;

		// Timer expired
		if (read(timerfd, &t, 8) > 0)
		{
			pthread_mutex_lock(&g_mutex);
			log_debug("Timer expired at %d", (int)time(NULL));
			pthread_mutex_unlock(&g_mutex);

			bool send_ping = false;

			// Check time since last message sent
			pthread_mutex_lock(&g_mutex);
			double elapsed_seconds = difftime(time(NULL), g_last_request);

			if (elapsed_seconds > 5)
			{
				send_ping = true;
			}

			pthread_mutex_unlock(&g_mutex);

			if (send_ping)
			{
				// Make ping request
				char *msg = strdup("PING\r\n");
				thread_queue_push(client->queue, msg);
				sleep(1);
			}
		}
	}

	return (void *)client;
}