#include "include/server.h"

static volatile bool g_alive = true;

void sighandler(int sig);

/**
 * To start IRC server on given port
 */
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>", *argv);
        return 1;
    }

    // Create and start an IRC server on given port
    Server *serv = Server_create(atoi(argv[1]));

    // Array for events returned from epoll
    struct epoll_event events[MAX_EVENTS] = {0};

    // Event for listener socket
    struct epoll_event ev = {.events = EPOLLIN, .data.fd = serv->fd};

    CHECK(epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, serv->fd, &ev), "epoll_ctl");

    // Setup signal handler to stop server
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sighandler;
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1)
        die("sigaction");

    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        die("sigaction");

    // Run while g_alive flag is set
    while (g_alive)
    {
        int num = epoll_wait(serv->epollfd, events, MAX_EVENTS, -1);
        CHECK(num, "epoll_wait");

        for (int i = 0; i < num; i++)
        {
            // Event on listening socket
            if (events[i].data.fd == serv->fd)
            {
                // Accept all new connections
                Server_accept_all(serv);
            }
            else
            {
                int e = events[i].events;
                int fd = events[i].data.fd;

                User *usr;

                // Fetch user data from hashtable
                if (cc_hashtable_get(serv->connections, (void *)&fd, (void **)&usr) != CC_OK)
                {
                    epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, fd, NULL);
                    continue;
                }

                if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
                {
                    log_debug("user %s disconnected", usr->nick);
                    User_Disconnect(serv, usr);
                    continue;
                }

                if (e & EPOLLIN)
                {
                    if (User_Read_Event(serv, usr) == -1)
                    {
                        continue;
                    }
                }

                if (e & EPOLLOUT)
                {
                    if (User_Write_Event(serv, usr) == -1)
                    {
                        continue;
                    }
                }
            }
        }
    }

    cc_hashtable_destroy(serv->connections);
    Server_destroy(serv);

    return 0;
}

void sighandler(int sig)
{
    if (sig == SIGINT)
    {
        g_alive = false;
    }
}
