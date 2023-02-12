#include "include/client.h"

#define DEBUG

void stop(int sig);

// The irc client
Client g_client;

// The reader thread will read messages from server and print them to stdout
pthread_t g_reader_thread;

// The writer thread will pull messages from client queue and send them to server
pthread_t g_writer_thread;

pthread_t g_ping_thread;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
        return 1;
    }

    Client_init(&g_client);

    Client_connect(&g_client, argv[1], argv[2]);

    pthread_create(&g_reader_thread, NULL, start_reader_thread, (void *)&g_client);

    pthread_create(&g_writer_thread, NULL, start_writer_thread, (void *)&g_client);

    pthread_create(&g_ping_thread, NULL, start_ping_thread, (void *)&g_client);

    char buffer[MAX_MSG_LEN] = {0};

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = stop;
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1)
        die("sigaction");

    // Register client

#ifdef DEBUG
    // strcpy(g_client.nick, "aaryab2");
    strcpy(g_client.username, "aarya.bhatia1678");
    strcpy(g_client.realname, "Aarya Bhatia");
#else
    sprintf(buffer, "Enter your nick > ");
    write(1, buffer, strlen(buffer));
    scanf("%s", g_client.nick);

    sprintf(buffer, "Enter your username > ");
    write(1, buffer, strlen(buffer));
    scanf("%s", g_client.username);

    sprintf(buffer, "Enter your realname > ");
    write(1, buffer, strlen(buffer));
    scanf("%s", g_client.realname);
#endif

    sprintf(buffer, "NICK %s\r\nUSER %s * * :%s\r\n", g_client.nick, g_client.username, g_client.realname);

    thread_queue_push(g_client.queue, strdup(buffer));
    sleep(1);

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
        {
            line[ret - 1] = 0;
        }
        else
        {
            line[ret] = 0;
        }

        if(!strcmp(line, "exit"))
        {
            break;
        }
        else 
        {
            printf("Command not found: %s\n", line);
        }
    }

    stop(0);
    exit(0);
}

/* signal handler */
void stop(int sig)
{
    (void)sig;

    pthread_cancel(g_reader_thread);
    pthread_cancel(g_writer_thread);
    pthread_cancel(g_ping_thread);

    pthread_join(g_reader_thread, NULL);
    pthread_join(g_writer_thread, NULL);
    pthread_join(g_ping_thread, NULL);

    Client_destroy(&g_client);

    log_info("Goodbye!");
    exit(0);
}
