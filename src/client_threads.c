#include "include/client.h"

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
            printf("Server: %s\n", message);
        }

        message_destroy(&msg_info);

        free(message);
    }

    return client;
}

void *reader_thread_routine(void *args)
{
    Client *client = (Client *)args;

    char buf[MAX_MSG_LEN + 1];
    size_t len = 0;

    while (1)
    {
        ssize_t nrecv = read_all(client->client_sock, buf + len, MAX_MSG_LEN);

        if (nrecv == -1)
            die("read");

        len += nrecv;
        buf[len] = 0;

        log_debug("reader_thread: read %zd bytes", nrecv);

        if (!strstr(buf, "\r\n"))
        {
            log_error("Malformed reply");
            continue;
        }

        // check for partial messages

        char *partial = rstrstr(buf, "\r\n"); // get ptr to last \r\n in buf
        partial += 2; // move ptr to start of last message

        // all messages are complete
        if (*partial == 0)
        {
            partial = NULL;
        }
        else
        {
            *partial = 0; // erase tail of buffer
        }

        char *tok = strtok(buf, "\r\n");

        // add all complete messages to inbox
        while (tok)
        {
            queue_enqueue(client->client_inbox, strdup(tok));
            tok = strtok(NULL, "\r\n");
        }

        if (partial)
        {
            memmove(buf, partial, strlen(partial) + 1); // also copy null byte
        }
        else
        {
            memset(buf, 0, sizeof buf);
        }

        len = strlen(buf);
    }

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

        ssize_t nsent = write_all(client->client_sock, message, strlen(message));

        if (nsent == -1)
            die("write_all");

        log_debug("outbox_thread: sent %zd bytes", nsent);

        free(message);
    }

    return client;
}
