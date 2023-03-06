#include "include/client.h"

#include <sys/epoll.h>

#include "include/queue.h"

#define DEBUG
#define PING_INTERVAL_SEC 10
#define EPOLL_TIMEOUT 1000
#define MAX_EPOLL_EVENTS 6

static volatile bool alive = true;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_stdout = PTHREAD_MUTEX_INITIALIZER;

/**
 * File to load client nick, username and realname
 */
bool read_client_file(Client *client, const char *filename) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        log_error("Failed to open file %s", filename);
        return false;
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len = getline(&line, &cap, file);

    if (len > 0) {
        if (line[len - 1] == '\n') {
            line[len - 1] = 0;
        }

        char *save1 = NULL;
        char *tok = strtok_r(line, ":", &save1);
        client->client_realname = strdup(strtok_r(NULL, "", &save1));

        char *save2 = NULL;
        client->serv_hostname = strdup(strtok_r(tok, " ", &save2));
        client->serv_port = strdup(strtok_r(NULL, " ", &save2));
        client->client_nick = strdup(strtok_r(NULL, " ", &save2));
        client->client_username = strdup(strtok_r(NULL, " ", &save2));

        log_info("hostname: %s, port: %s, nick:%s username:%s realname:%s", client->serv_hostname, client->serv_port, client->client_nick, client->client_username, client->client_realname);

        free(line);
        fclose(file);
        return true;
    }

    free(line);
    fclose(file);
    return false;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        // fprintf(stderr, "Usage: %s <hostname> <port> <nick> <userame> <realname>\n", *argv);
        fprintf(stderr, "Usage: %s <filename>\n", *argv);
        return 1;
    }

    pthread_t outbox_thread; /* This thread will send the messages from the outbox queue to the server */
    pthread_t inbox_thread;  /* This thread will process messages from the inbox queue and display them to stdout */
    pthread_t reader_thread; /* This thread will read message from server add them to the inbox queue */

    Client client;
    Client_init(&client);

    if (!read_client_file(&client, argv[1])) {
        log_error("Failed to read file %s", argv[1]);
        return 1;
    }

    // establish connection with the server
    client.client_sock = create_and_bind_socket(client.serv_hostname, client.serv_port);

    // Make client socket non blocking
    fcntl(client.client_sock, F_SETFL,
          fcntl(client.client_sock, F_GETFL) | O_NONBLOCK);

    // Register the client in IRC network
    char *registration =
        make_string("NICK %s\r\nUSER %s * * :%s\r\n", client.client_nick,
                    client.client_username, client.client_realname);
    queue_enqueue(client.client_outbox, registration);

    pthread_create(&outbox_thread, NULL, outbox_thread_routine, &client);
    pthread_create(&inbox_thread, NULL, inbox_thread_routine, &client);
    pthread_create(&reader_thread, NULL, reader_thread_routine, &client);

    sleep(1);  // let threads start and send registration message

    char *line = NULL;    // buffer to store user input
    size_t line_len = 0;  // length of line buffer

    while (alive) {
        // Read the next line till \n
        ssize_t nread = getline(&line, &line_len, stdin);
        if (nread == -1)
            die("getline");
        line[nread - 1] = 0;  // remove newline character

        // Empty
        if (strlen(line) == 0) {
            continue;
        }

        if (strlen(line) > MAX_MSG_LEN) {
            SAFE(mutex_stdout, {
                log_error("Message is greater than %d bytes",
                          MAX_MSG_LEN);
            });
            continue;
        }
        // Send message to server
        queue_enqueue(client.client_outbox,
                      make_string("%s\r\n", line));

        if (!strncmp(line, "QUIT", 4)) {
            alive = false;
            break;
        }
    }

    free(line);

    SAFE(mutex_stdout, {
        log_debug("joining theads");
    });

    pthread_join(inbox_thread, NULL);
    SAFE(mutex_stdout, {
        log_debug("inbox thread quit");
    });

    pthread_join(outbox_thread, NULL);
    SAFE(mutex_stdout, {
        log_debug("outbox thread quit");
    });

    pthread_join(reader_thread, NULL);
    SAFE(mutex_stdout, {
        log_debug("reader thread quit");
    });

    Client_destroy(&client);
    printf("Goodbye!\n");

    return 0;
}

void _free_callback(void *data) {
    free(data);
}

void _signal_handler(int sig) {
    (void)sig;
    log_debug("alive: %d", alive);
    alive = false;
}

void Client_init(Client *client) {
    memset(client, 0, sizeof *client);
    client->client_inbox = queue_alloc();
    client->client_outbox = queue_alloc();

    assert(client->client_inbox->l);
    assert(client->client_outbox->l);
}

void Client_destroy(Client *client) {
    queue_free(client->client_inbox, _free_callback);
    queue_free(client->client_outbox, _free_callback);

    free(client->client_nick);
    free(client->client_realname);
    free(client->client_username);
    free(client->serv_hostname);
    free(client->serv_port);

    shutdown(client->client_sock, SHUT_RDWR);
    close(client->client_sock);
}
