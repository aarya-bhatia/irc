#include "include/client.h"

#define DEBUG

void sig_handler(int sig);

// The irc client
Client g_client;

// The reader thread will read messages from server and print them to stdout
pthread_t g_reader_thread;

// The writer thread will pull messages from client queue and send them to server
pthread_t g_writer_thread;

// Periodically send a PING request to the server
pthread_t g_ping_thread;

volatile time_t g_last_request;
volatile bool g_is_running = true;
pthread_mutex_t g_mutex;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
        return 1;
    }

    g_last_request = time(NULL);

    pthread_mutex_init(&g_mutex, NULL);

    Client_init(&g_client);

    Client_connect(&g_client, argv[1], argv[2]);

    pthread_create(&g_reader_thread, NULL, start_reader_thread, (void *)&g_client);

    pthread_create(&g_writer_thread, NULL, start_writer_thread, (void *)&g_client);

    pthread_create(&g_ping_thread, NULL, start_ping_thread, (void *)&g_client);

    char buffer[MAX_MSG_LEN] = {0};

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sig_handler;
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
    // sleep(1);

    char *line = NULL;
    size_t cap = 0;
    ssize_t ret;

    while (1)
    {
        pthread_mutex_lock(&g_mutex);
        if (!g_is_running)
        {
            pthread_mutex_unlock(&g_mutex);
            break;
        }
        pthread_mutex_unlock(&g_mutex);

        // pthread_mutex_lock(&g_mutex);
        // printf("Enter command > ");
        // pthread_mutex_unlock(&g_mutex);

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

        if (!strcmp(line, "exit"))
        {
            pthread_mutex_lock(&g_mutex);
            g_is_running = false;
            pthread_mutex_unlock(&g_mutex);
            break;
        }
        else
        {
            if (strlen(line) == 0)
            {
                pthread_mutex_lock(&g_mutex);
                printf("Enter Command: \n");
                pthread_mutex_unlock(&g_mutex);
            }
            else
            {
                pthread_mutex_lock(&g_mutex);
                printf("Command not found: %s\n", line);
                pthread_mutex_unlock(&g_mutex);
            }
        }
    }

    // pthread_cancel(g_reader_thread);
    // pthread_cancel(g_writer_thread);
    // pthread_cancel(g_ping_thread);

    pthread_join(g_reader_thread, NULL);
    pthread_join(g_writer_thread, NULL);
    pthread_join(g_ping_thread, NULL);

    pthread_mutex_destroy(&g_mutex);

    Client_destroy(&g_client);

    log_info("Goodbye!");
    exit(0);
}

void sig_handler(int sig)
{
    (void)sig;
    g_is_running = false;
}
