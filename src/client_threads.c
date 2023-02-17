#include "include/client.h"
#include <sys/epoll.h>

extern pthread_mutex_t mutex_stdout;

void *inbox_thread_routine(void *args)
{
    Client *client = (Client *)args;

    while (1)
    {
        char *message = queue_dequeue(client->client_inbox);

        if (!strcmp(message, "exit"))
        {
            free(message);
            break;
        }

        SAFE(mutex_stdout, {
            printf("Server: %s\n", message);
        });

        // Message msg_info;

        // message_init(&msg_info);

        // if (parse_message(message, &msg_info) == -1)
        // {
        //     SAFE(mutex_stdout, { log_error("Failed to parse message"); });
        // }
        // else
        // {
        //     // process message
        // }

        // message_destroy(&msg_info);

        free(message);
    }

    return client;
}

void *reader_thread_routine(void *args)
{
    Client *client = (Client *)args;
    char buf[MAX_MSG_LEN + 1];
    memset(buf, 0, sizeof buf);
    size_t len = 0;

    // We will poll the socket for reads
    int epollfd = epoll_create1(0);

    struct epoll_event ev = {.events = EPOLLIN, .data.fd = client->client_sock};
    epoll_ctl(epollfd, EPOLL_CTL_ADD, client->client_sock, &ev);

    struct epoll_event events[1];

    while (1)
    {
        int nfd = epoll_wait(epollfd, events, 1, 2000);

        if (nfd == -1)
        {
            perror("epoll_wait");
            break;
        }

        // No events polled
        if (nfd == 0)
        {
            continue;
        }

        if (events[0].data.fd != client->client_sock)
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
            ssize_t nrecv = read_all(client->client_sock, buf + len, MAX_MSG_LEN);

            if (nrecv == -1)
                die("read");

            if (nrecv == 0)
            {
                break;
            }

            len += nrecv;
            buf[len] = 0;

            SAFE(mutex_stdout, { log_debug("reader_thread: read %zd bytes", nrecv); });

            if (!strstr(buf, "\r\n"))
            {
                continue;
            }

            // check for partial messages

            // char *partial = rstrstr(buf, "\r\n"); // get ptr to last \r\n in buf
            // partial += 2;                         // move ptr to start of last message

            // // all messages are complete
            // if (*partial == 0)
            // {
            //     partial = NULL;
            // }
            // else
            // {
            //     *partial = 0; // erase tail of buffer
            // }

            char *tok = strtok(buf, "\r\n");
            int count = 0;

            // add all complete messages to inbox
            while (tok)
            {
                queue_enqueue(client->client_inbox, strdup(tok));
                tok = strtok(NULL, "\r\n");
                count++;
            }

            // if (partial)
            // {
            //     memmove(buf, partial, strlen(partial) + 1); // also copy null byte
            // }
            // else
            // {
            //     memset(buf, 0, sizeof buf);
            // }

            // len = strlen(buf);

            len = 0;
            SAFE(mutex_stdout, { log_debug("%d messages were read from server", count); });
        }
    }

    close(epollfd);

    SAFE(mutex_stdout, { log_debug("reader_thread quitting"); });

    return client;
}

void *outbox_thread_routine(void *args)
{
    Client *client = (Client *)args;

    while (1)
    {
        char *message = queue_dequeue(client->client_outbox);

        if (!strcmp(message, "exit"))
        {
            free(message);
            break;
        }

        log_debug("outbox_thread: sending message: %s", message);

        ssize_t nsent = write_all(client->client_sock, message, strlen(message));

        if (nsent == -1)
            die("write_all");

        SAFE(mutex_stdout, { log_debug("outbox_thread: sent %zd bytes", nsent); });

        free(message);
    }

    return client;
}
