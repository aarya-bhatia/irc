#include "include/client.h"

#include <pthread.h>

// The irc client
static Client g_client;

// The reader thread will read messages from server and print them to stdout
static pthread_t reader_thread;

// The writer thread will pull messages from client queue and send them to server
static pthread_t writer_thread;

void stop(int sig);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
        return 1;
    }

    Client_init(&g_client);

    Client_connect(&g_client, argv[1], argv[2]);

    pthread_create(&reader_thread, NULL, start_reader_thread, (void *)&g_client);

    pthread_create(&writer_thread, NULL, start_writer_thread, (void *)&g_client);

    char buffer[MAX_MSG_LEN] = {0};

    signal(SIGINT, stop);

    // Register client

    sprintf(buffer, "Enter your nick > ");
    write(1, buffer, strlen(buffer));
    scanf("%s", g_client.nick);

    sprintf(buffer, "Enter your username > ");
    write(1, buffer, strlen(buffer));
    scanf("%s", g_client.username);

    sprintf(buffer, "Enter your realname > ");
    write(1, buffer, strlen(buffer));
    scanf("%s", g_client.realname);

    sprintf(buffer, "NICK %s\r\nUSER %s * * :%s\r\n", g_client.nick, g_client.username, g_client.realname);

    thread_queue_push(g_client.queue, strdup(buffer));

    char *line = NULL;
    size_t cap = 0;
    ssize_t ret;

    while (1)
    {
        printf("Enter command > ");
        if ((ret = getline(&line, &cap, stdin)) <= 0)
        {
            break;
        }

        if (line[ret - 1] == '\n')
            line[ret - 1] = 0;
        else
            line[ret] = 0;
    }

    stop(0);
    exit(0);
}

/* signal handler */
void stop(int sig)
{
	(void)sig;

	pthread_cancel(reader_thread);
	pthread_cancel(writer_thread);

	pthread_join(reader_thread, NULL);
	pthread_join(writer_thread, NULL);

	Client_destroy(&g_client);

	log_info("Goodbye!");
	exit(0);
}
