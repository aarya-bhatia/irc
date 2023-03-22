#include <sys/epoll.h>

#include "include/client.h"

#define EPOLL_TIMEOUT 2500 // Seconds

extern pthread_mutex_t mutex_stdout; // use this lock before printing
									 // to stdout

void display_message(const char *message)
{
	SAFE(mutex_stdout, { puts(message); });
}

/*
 * This thread will process messages from the inbox queue and display them
 * to stdout
 */
void *inbox_thread_routine(void *args)
{
	Client *client = (Client *)args;

	while(1)
		{
			char *message = queue_dequeue(client->client_inbox);
			char *msgcopy = strdup(message);

			Message msg_info;

			message_init(&msg_info);

			if(parse_message(msgcopy, &msg_info) == -1)
				{
					SAFE(mutex_stdout, {
						log_error("Failed to parse message: %s", msgcopy);
					});
				}
			else
				{
					// Display message to stdout

					if(!strcmp(msg_info.command, "PRIVMSG"))
						{
							assert(msg_info.n_params == 1);

							//   SAFE(mutex_stdout,
							//        { printf("%s: %s\n", msg_info.origin,
							//        msg_info.body); });
							SAFE(mutex_stdout, { puts(message); });
						}
					else
						{
							display_message(message);
						}
				}

			free(msgcopy);
			message_destroy(&msg_info);

			if(strstr(message, "ERROR"))
				{
					SAFE(mutex_stdout, { log_debug("inbox_thread: bye"); });
					free(message);
					break;
				}

			free(message);
		}

	return client;
}

/*
 * This thread will read message from server add them to the inbox queue
 */
void *reader_thread_routine(void *args)
{
	Client *client = (Client *)args;
	char buf[MAX_MSG_LEN + 1];
	memset(buf, 0, sizeof buf);
	size_t len = 0;

	// We will poll the socket for reads
	int epollfd = epoll_create1(0);

	struct epoll_event ev
	  = {.events = EPOLLIN, .data.fd = client->client_sock};
	epoll_ctl(epollfd, EPOLL_CTL_ADD, client->client_sock, &ev);

	struct epoll_event events[1];

	bool quit = false;

	while(!quit)
		{
			int nfd = epoll_wait(epollfd, events, 1, EPOLL_TIMEOUT);

			if(nfd == -1)
				{
					perror("epoll_wait");
					break;
				}

			// No events polled
			if(nfd == 0)
				{
					continue;
				}

			if(events[0].data.fd != client->client_sock)
				{
					continue;
				}

			// Server disconnect
			if(events[0].events & (EPOLLERR | EPOLLHUP))
				{
					SAFE(mutex_stdout, { log_info("Server disconnect"); });
					break;
				}

			if(events[0].events & EPOLLIN)
				{
					ssize_t nrecv
					  = read_all(client->client_sock, buf + len, MAX_MSG_LEN);

					if(nrecv == -1)
						die("read");

					if(nrecv == 0)
						{
							break;
						}

					len += nrecv;
					buf[len] = 0;

					SAFE(mutex_stdout, {
						log_debug("reader_thread: read %zd bytes", nrecv);
						log_debug("reader_thread: %s", buf);
					});

					if(!strstr(buf, "\r\n"))
						{
							continue;
						}

					// check for partial messages

					// char *partial = rstrstr(buf, "\r\n"); // get ptr to last
					// \r\n in buf
					// partial += 2; // move ptr to start of last message

					// // all messages are complete
					// if (*partial == 0)
					// {
					// partial = NULL;
					// }
					// else
					// {
					// *partial = 0; // erase tail of buffer
					// }

					char *tok = strtok(buf, "\r\n");
					int count = 0;

					// add all complete messages to inbox
					while(tok)
						{
							queue_enqueue(client->client_inbox, strdup(tok));

							if(strstr(tok, "ERROR"))
								{
									SAFE(mutex_stdout,
										 { log_debug("reader_thread: bye"); });
									quit = true;
									break;
								}

							tok = strtok(NULL, "\r\n");
							count++;
						}

					// if (partial)
					// {
					// memmove(buf, partial, strlen(partial) + 1); // also copy
					// null byte
					// }
					// else
					// {
					// memset(buf, 0, sizeof buf);
					// }

					// len = strlen(buf);

					len = 0;
					SAFE(mutex_stdout, {
						log_debug("%d messages were read from server", count);
					});
				}
		}

	close(epollfd);

	return client;
}

/*
 * This thread will send the messages from the outbox queue to the server
 */
void *outbox_thread_routine(void *args)
{
	Client *client = (Client *)args;

	while(1)
		{
			char *message = queue_dequeue(client->client_outbox);

			ssize_t nsent
			  = write_all(client->client_sock, message, strlen(message));

			if(nsent == -1)
				die("write_all");

			SAFE(mutex_stdout,
				 { log_debug("outbox_thread: sent %zd bytes", nsent); });

			if(!strncmp(message, "QUIT", 4))
				{
					SAFE(mutex_stdout, { log_debug("outbox_thread: bye"); });
					free(message);
					break;
				}

			free(message);
		}

	return client;
}
