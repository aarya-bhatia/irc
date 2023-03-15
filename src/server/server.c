#include "include/server.h"

#include <sys/epoll.h>
#include <sys/stat.h>
#include <time.h>

struct rpl_handle_t {
    const char *name;
    void (*user_handler)(Server *, User *, Message *);
    void (*peer_handler)(Server *, Peer *, Message *);
};

static struct rpl_handle_t rpl_handlers[] = {
    {"NICK", Server_reply_to_NICK, NULL},
    {"USER", Server_reply_to_USER, NULL},
    {"PRIVMSG", Server_reply_to_PRIVMSG, NULL},
    {"NOTICE", Server_reply_to_NOTICE, NULL},
    {"PING", Server_reply_to_PING, NULL},
    {"QUIT", Server_reply_to_QUIT, NULL},
    {"MOTD", Server_reply_to_MOTD, NULL},
    {"LIST", Server_reply_to_LIST, NULL},
    {"WHO", Server_reply_to_WHO, NULL},
    {"WHOIS", Server_reply_to_WHOIS, NULL},
    {"JOIN", Server_reply_to_JOIN, NULL},
    {"PART", Server_reply_to_PART, NULL},
    {"NAMES", Server_reply_to_NAMES, NULL},
    {"TOPIC", Server_reply_to_TOPIC, NULL},
    {"LUSERS", Server_reply_to_LUSERS, NULL},
    {"HELP", Server_reply_to_HELP, NULL},
    {"CONNECT", Server_reply_to_CONNECT, NULL},
    {"SERVER", NULL, Server_reply_to_SERVER},
};

User *Server_get_user_by_nick(Server *serv, const char *nick) {
    const char *username = ht_get(serv->online_nick_to_username_map, nick);
    return username ? ht_get(serv->username_to_user_map, username) : NULL;
}

User *Server_get_user_by_username(Server *serv, const char *username) {
    return ht_get(serv->username_to_user_map, username);
}

void Server_destroy(Server *serv) {
    assert(serv);

    save_channels(serv->channels_map, CHANNELS_FILENAME);

    ht_free(serv->channels_map);

    ht_free(serv->name_to_peer_map);
    ht_free(serv->username_to_user_map);
    ht_free(serv->online_nick_to_username_map);
    ht_free(serv->offline_nick_to_username_map);

    // close all connections
    HashtableIter conn_itr;
    ht_iter_init(&conn_itr, serv->connections);
    Connection *conn = NULL;

    while (ht_iter_next(&conn_itr, NULL, (void **)&conn)) {
        Server_remove_connection(serv, conn);
    }

    ht_free(serv->connections);

    close(serv->fd);
    close(serv->epollfd);
    free(serv->hostname);
    free(serv->port);
    free(serv);

    log_debug("Server stopped");
    exit(0);
}

/**
 * Create and initialise the server. Bind socket to given port.
 */
Server *Server_create(int port) {
    Server *serv = calloc(1, sizeof *serv);
    assert(serv);

    serv->connections = ht_alloc();                  /* Map<int, Connection *> */
    serv->name_to_peer_map = ht_alloc();             /* Map<string, Peer *> */
    serv->username_to_user_map = ht_alloc();         /* Map<string, User*> */
    serv->online_nick_to_username_map = ht_alloc();  /* Map<string, string>*/
    serv->offline_nick_to_username_map = ht_alloc(); /* Map<string, string>*/

    serv->connections->key_len = sizeof(int);
    serv->connections->key_compare = (compare_type)int_compare;
    serv->connections->key_copy = (elem_copy_type)int_copy;
    serv->connections->key_free = free;

    serv->online_nick_to_username_map->value_copy = (elem_copy_type)strdup;
    serv->offline_nick_to_username_map->value_copy = (elem_copy_type)strdup;

    serv->online_nick_to_username_map->value_free = free;
    serv->offline_nick_to_username_map->value_free = free;

    serv->channels_map = load_channels(CHANNELS_FILENAME);

    serv->motd_file = MOTD_FILENAME;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    size_t n = strftime(serv->created_at, sizeof(serv->created_at), "%c", tm);
    assert(n > 0);

    // Create epoll fd for listen socket and clients
    serv->epollfd = epoll_create(1 + MAX_EVENTS);
    CHECK(serv->epollfd, "epoll_create");

    // TCP Socket non-blocking
    serv->fd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    CHECK(serv->fd, "socket");

    // Server Address
    serv->servaddr.sin_family = AF_INET;
    serv->servaddr.sin_port = htons(port);
    serv->servaddr.sin_addr.s_addr = INADDR_ANY;
    serv->hostname = strdup(addr_to_string((struct sockaddr *)&serv->servaddr,
                                           sizeof(serv->servaddr)));
    serv->port = make_string("%d", port);

    int yes = 1;

    // Set socket options
    CHECK(setsockopt(serv->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes),
          "setsockopt");

    // Bind
    CHECK(bind(serv->fd, (struct sockaddr *)&serv->servaddr,
               sizeof(struct sockaddr_in)),
          "bind");

    // Listen
    CHECK(listen(serv->fd, MAX_EVENTS), "listen");

    log_info("server running on port %d", port);

    return serv;
}

/**
 * There are new sock_to_user_map available
 */
void Server_accept_all(Server *serv) {
    struct sockaddr_storage client_addr;
    socklen_t addrlen = sizeof(client_addr);

    while (1) {
        int conn_sock = accept(serv->fd, (struct sockaddr *)&client_addr, &addrlen);

        if (conn_sock == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }

            die("accept");
        }

        Connection *conn = Connection_alloc(conn_sock, (struct sockaddr *)&client_addr, addrlen);

        if (!Server_add_connection(serv, conn)) {
            Server_remove_connection(serv, conn);
        }
    }
}

void Server_process_request(Server *serv, Connection *conn) {
    assert(serv);
    assert(conn);

    // Get array of parsed messages
    Vector *messages = parse_message_list(conn->incoming_messages);
    assert(messages);

    log_debug("Processing %zu messages from connection %d", Vector_size(messages), conn->fd);

    // Iterate over the request messages and add response message(s) to user's message queue in the same order.
    for (size_t i = 0; i < Vector_size(messages); i++) {
        Message *message = Vector_get_at(messages, i);
        assert(message);
        if (!message->command) {
            log_error("invalid message");
            continue;
        }

        // Find a handler to execute for given request

        bool found = false;

        for (size_t j = 0; j < sizeof rpl_handlers / sizeof *rpl_handlers; j++) {
            struct rpl_handle_t handle = rpl_handlers[j];
            if (!strcmp(handle.name, message->command)) {
                if (conn->conn_type == USER_CONNECTION) {
                    handle.user_handler(serv, conn->data, message);
                } else if (conn->conn_type == PEER_CONNECTION) {
                    handle.peer_handler(serv, conn->data, message);
                }

                found = true;
                break;
            }
        }

        // flush message queues
        if (found) {
            if (conn->conn_type == USER_CONNECTION) {
                User *usr = conn->data;

                while (List_size(usr->msg_queue) > 0) {
                    List_push_back(conn->outgoing_messages, strdup(List_peek_front(usr->msg_queue)));
                    List_pop_front(usr->msg_queue);
                }
            } else if (conn->conn_type == PEER_CONNECTION) {
                Peer *peer = conn->data;

                while (List_size(peer->msg_queue) > 0) {
                    List_push_back(conn->outgoing_messages, strdup(List_peek_front(peer->msg_queue)));
                    List_pop_front(peer->msg_queue);
                }
            }

            continue;
        }

        // Handle every other command:

        if (conn->conn_type == USER_CONNECTION) {
            User *usr = conn->data;

            if (!usr->registered) {
                char *reply = make_reply(":%s 451 %s :Connection not registered", serv->hostname, usr->nick);
                List_push_back(conn->outgoing_messages, reply);
            } else {
                char *reply = make_reply(":%s 421 %s %s :Unknown command",
                                         serv->hostname,
                                         usr->nick,
                                         message->command);
                List_push_back(conn->outgoing_messages, reply);
            }
        } else {
            // TODO
        }
    }

    Vector_free(messages);
}

void Server_broadcast_message(Server *serv, const char *message) {
    HashtableIter itr;
    ht_iter_init(&itr, serv->connections);

    Connection *conn = NULL;

    while (ht_iter_next(&itr, NULL, (void **)&conn)) {
        List_push_back(conn->outgoing_messages, strdup(message));
    }
}

void Server_broadcast_to_channel(Server *serv, Channel *channel, const char *message) {
    for (size_t i = 0; i < Vector_size(channel->members); i++) {
        Membership *member = Vector_get_at(channel->members, i);
        assert(member);

        User *member_user = Server_get_user_by_username(serv, member->username);

        if (member_user) {
            List_push_back(member_user->msg_queue, strdup(message));
        }
    }
}

char *get_motd(char *fname) {
    FILE *file = fopen(fname, "r");

    if (!file) {
        log_warn("failed to open %s", fname);
        return NULL;
    }

    char *res = NULL;
    size_t res_len = 0;
    size_t num_lines = 1;

    // count number of lines
    for (int c = fgetc(file); c != EOF; c = fgetc(file)) {
        if (c == '\n') {
            num_lines = num_lines + 1;
        }
    }

    fseek(file, 0, SEEK_SET);  // go to beginning

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    size_t line_no = tm.tm_yday % num_lines;  // select a line from file to use

    for (size_t i = 0; i < line_no + 1; i++) {
        if (getline(&res, &res_len, file) == -1) {
            perror("getline");
            break;
        }
    }

    if (res) {
        size_t len = strlen(res);
        if (res[len - 1] == '\n') {
            res[res_len - 1] = 0;
        }
    }

    fclose(file);

    return res;
}

bool Server_add_connection(Server *serv, Connection *connection) {
    ht_set(serv->connections, &connection->fd, connection);

    // Make user socket non-blocking
    if (fcntl(connection->fd, F_SETFL, fcntl(connection->fd, F_GETFL) | O_NONBLOCK) != 0) {
        perror("fcntl");
        return false;
    }

    // Add event
    struct epoll_event ev = {.data.fd = connection->fd, .events = EPOLLIN | EPOLLOUT};

    // Add user socket to epoll set
    if (epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, connection->fd, &ev) !=
        0) {
        perror("epoll_ctl");
        return false;
    }

    log_info("Got connection %d from %s", connection->fd, connection->hostname);

    return true;
}

void Server_remove_connection(Server *serv, Connection *connection) {
    ht_remove(serv->connections, &connection->fd, NULL, NULL);
    epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, connection->fd, NULL);

    if (connection->conn_type == USER_CONNECTION) {
        User *usr = connection->data;

        log_info("Closing connection with user %s", usr->nick);

        ht_remove(serv->online_nick_to_username_map, usr->nick, NULL, NULL);
        ht_remove(serv->offline_nick_to_username_map, usr->nick, NULL, NULL);
        ht_remove(serv->username_to_user_map, usr->username, NULL, NULL);

        // Remove user from channels
        for (size_t i = 0; i < Vector_size(usr->channels); i++) {
            char *name = Vector_get_at(usr->channels, i);
            Channel *channel = ht_get(serv->channels_map, name);
            if (channel) {
                Channel_remove_member(channel, usr->username);
            }
        }

        User_free(usr);

    } else if (connection->conn_type == PEER_CONNECTION) {
        Peer *peer = connection->data;

        log_info("Closing connection with peer %d", connection->fd);

        if (peer->name) {
            ht_remove(serv->name_to_peer_map, peer->name, NULL, NULL);
        }

        Peer_free(peer);
    }

    Connection_free(connection);
}

bool Server_add_peer(Server *serv, const char *name) {
    // Load server info from file
    FILE *file = fopen(serv->config_file, "r");

    if (!file) {
        log_error("failed to open config file %s", serv->config_file);
        return false;
    }

    char *remote_name = NULL;
    char *remote_host = NULL;
    char *remote_port = NULL;
    char *remote_passwd = NULL;

    char *line = NULL;
    size_t capacity = 0;
    ssize_t nread = 0;

    while ((nread = getline(&line, &capacity, file)) > 0) {
        if (line[nread - 1] == '\n') {
            line[nread - 1] = 0;
        }

        remote_name = strtok(line, ",");
        remote_host = strtok(NULL, ",");
        remote_port = strtok(NULL, ",");
        remote_passwd = strtok(NULL, ",");

        assert(remote_name);
        assert(remote_host);
        assert(remote_port);
        assert(remote_passwd);

        if (!strcmp(remote_name, name)) {
            break;
        }
    }

    fclose(file);
    free(line);

    if (strcmp(remote_name, name) != 0) {
        log_error("Server not configured in file %s", serv->config_file);
        return false;
    }

    int fd = connect_to_host(remote_host, remote_port);

    if (fd == -1) {
        return false;
    }

    Connection *conn = calloc(1, sizeof *conn);
    conn->fd = fd;
    conn->hostname = strdup(remote_host);
    conn->port = atoi(remote_port);
    conn->incoming_messages = List_alloc(NULL, free);
    conn->outgoing_messages = List_alloc(NULL, free);

    Peer *peer = Peer_alloc();
    peer->name = strdup(remote_name);

    conn->conn_type = PEER_CONNECTION;
    conn->data = peer;

    ht_set(serv->connections, &fd, conn);
    ht_set(serv->name_to_peer_map, remote_name, peer);

    List_push_back(conn->outgoing_messages, make_string("PASS %s * *\r\n", remote_passwd));
    List_push_back(conn->outgoing_messages, make_string("SERVER %s :\r\n", serv->name));

    return true;
}
