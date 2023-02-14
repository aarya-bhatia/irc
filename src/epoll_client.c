#include "include/common.h"
#include "include/message.h"
#include "include/client.h"
#include "include/aaryab2/EventQueue.h"
#include "include/collectc/cc_list.h"

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>

#define DEBUG
#define PING_INTERVAL_SEC 3
#define EPOLL_TIMEOUT 1000
#define MAX_QUEUE_SIZE 100
#define MAX_EPOLL_EVENTS 3 // stdin, socket, timer

static char *g_line = NULL;
static size_t g_line_len = 0;
static time_t g_last_request;
static Client g_client;

static volatile bool running = true;

// message queue to store outgoing messages
static CC_List *g_queue = NULL;

void read_user_input();
void signal_handler(int sig);
void cb(void *msg);
void quit();
void read_from_socket(int sock, char read_buf[], size_t *read_buf_len);
void write_to_socket(int sock, char write_buf[], size_t *write_buf_len, size_t *write_buf_off);
int create_timer(int duration_sec);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", *argv);
        return 1;
    }

    Client_init(&g_client);
    Client_connect(&g_client, argv[1], argv[2]);

    cc_list_new(&g_queue);

    g_last_request = time(NULL);

    // Create fd for epoll
    int epoll_fd = epoll_create1(0);
    CHECK(epoll_fd, "epoll_create1");

    // Create a timer fd to send PING messages
    int timerfd = create_timer(PING_INTERVAL_SEC);
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

    ev.data.fd = g_client.sock;
    ev.events = EPOLLIN | EPOLLOUT;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, g_client.sock, &ev) == 0)
        log_info("Added socket (%d) to epoll set", g_client.sock);
    else
        die("epoll_ctl");

    char read_buf[MAX_MSG_LEN + 1];  // buffer to store bytes being read from the server
    char write_buf[MAX_MSG_LEN + 1]; // buffer to store bytes being sent to the server

    size_t read_buf_len = 0;  // size of the read buffer
    size_t write_buf_len = 0; // size of the write buffer
    size_t write_buf_off = 0; // position in the write buffer

    // Array to store the returned events from epoll_wait
    struct epoll_event events[5];

    // Register signal handler to quit on CTRL-C
    signal(SIGINT, signal_handler);

    // Register the client in IRC network
#ifdef DEBUG
    // strcpy(g_client.nick, "aaryab2");
    strcpy(g_client.username, "aarya.bhatia1678");
    strcpy(g_client.realname, "Aarya Bhatia");
// #else
//     sprintf(buffer, "Enter your nick > ");
//     write(1, buffer, strlen(buffer));
//     scanf("%s", g_client.nick);

//     sprintf(buffer, "Enter your username > ");
//     write(1, buffer, strlen(buffer));
//     scanf("%s", g_client.username);

//     sprintf(buffer, "Enter your realname > ");
//     write(1, buffer, strlen(buffer));
//     scanf("%s", g_client.realname);
#endif

    char *initial_requests = NULL;
    asprintf(&initial_requests, "NICK %s\r\nUSER %s * * :%s\r\n", g_client.nick, g_client.username, g_client.realname);
    cc_list_add_last(g_queue, initial_requests);

    while (running)
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
                cc_list_add_last(g_queue, strdup(g_line));
            }

            // timer event
            else if (fd == timerfd && event & EPOLLIN)
            {
                uint64_t t = 0;

                // Timer expired
                if (read(timerfd, &t, 8) > 0)
                {
                    log_debug("Timer expired at %d", (int)time(NULL));

                    // Only ping if idle
                    if (write_buf_len > 0)
                    {
                        continue;
                    }

                    // Check time since last message sent
                    if (difftime(time(NULL), g_last_request) > PING_INTERVAL_SEC)
                    {
                        // Make ping request
                        cc_list_add_last(g_queue, strdup("PING\r\n"));
                        sleep(1);
                    }
                }
            }

            // socket event
            else if (fd == g_client.sock)
            {
                if (event & (EPOLLERR | EPOLLHUP))
                {
                    die(NULL);
                }

                // socket is ready to send data i.e. client can read
                if (event & EPOLLIN)
                {
                    read_from_socket(g_client.sock, read_buf, &read_buf_len);
                }

                // socket is ready to recieve data i.e. client can write data
                if (event & EPOLLOUT)
                {
                    write_to_socket(g_client.sock, write_buf, &write_buf_len, &write_buf_off);
                }
            }
        }
    }

    quit();

    return 0;
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

void signal_handler(int sig)
{
    (void)sig;
    running = false;
}

void cb(void *msg)
{
    free(msg);
}

void quit()
{
    free(g_line);
    cc_list_destroy_cb(g_queue, cb);
    Client_destroy(&g_client);
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

int create_timer(int duration_sec)
{
    struct itimerspec tv = {
        .it_interval.tv_sec = duration_sec,
        .it_interval.tv_nsec = 0,
        .it_value.tv_sec = duration_sec,
        .it_value.tv_nsec = 0};

    int fd = timerfd_create(CLOCK_MONOTONIC, 0);

    if (timerfd_settime(fd, 0, &tv, NULL) != 0)
        die("timerfd_settime");

    return fd;
}