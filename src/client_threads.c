#include "include/client.h"

void *reader_thread_routine(void *args)
{
    Client *client = (Client *)args;

    char buf[MAX_MSG_LEN + 1];
    size_t bytes_recv = 0;
    buf[0] = 0;

    while (1)
    {
        while (1)
        {
            ssize_t nrecv = read(client->client_sock, buf + bytes_recv, MAX_MSG_LEN - bytes_recv);

            if (nrecv == -1)
                die("read");

            if (nrecv == 0)
                break;

            bytes_recv += nrecv;

            buf[bytes_recv] = 0;

            if (strstr(buf, "\r\n"))
            {
                break;
            }
        }

        char *next = strstr(buf, "\r\n");
        char *prev = next;

        while (next)
        {
            next = strstr(prev + 2, "\r\n");

            if (next)
            {
                prev = next;
            }
        }

        // check for partial messages
        char tmp[MAX_MSG_LEN + 1];
        strcpy(tmp, prev + 2);
        *prev = 0;

        char *tok = strtok(buf, "\r\n");

        // add complete messages to inbox
        while (tok)
        {
            queue_enqueue(client->client_inbox, strdup(tok));
            tok = strtok(NULL, "\r\n");
        }

        strcpy(buf, tmp);
        bytes_recv = strlen(tmp);
    }

    return client;
}

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

        Message msg_info;
        
        message_init(&msg_info);

        if (parse_message(message, &msg_info) == -1)
        {
            log_error("Failed to parse message");
        }
        else
        {
            // process message
            puts(message);
        }

        message_destroy(&msg_info);

        free(message);
    }

    return client;
}

void *outbox_thread_routine(void *args)
{
    Client *client = (Client *)args;

    while (1)
    {
        char *message = queue_dequeue(client->client_outbox);

        size_t msg_len = strlen(message);
        size_t bytes_sent = 0;

        while (bytes_sent < msg_len)
        {
            ssize_t nsent = write(client->client_sock, message + bytes_sent, msg_len - bytes_sent);

            if (nsent == -1)
                die("write");

            if (nsent == 0)
                break;

            bytes_sent += nsent;
        }

        free(message);
    }

    return client;
}
