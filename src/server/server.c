
#include "include/server.h"

#include <sys/epoll.h>
#include <sys/stat.h>
#include <time.h>

#include "include/common.h"
#include "include/list.h"

/**
 * request handler
 */
typedef struct _UserRequestHandler {
    const char *name;                             /* command name */
    void (*handler)(Server *, User *, Message *); /* request handler function */
} UserRequestHandler;

typedef struct _PeerRequestHandler {
    const char *name;                             /* command name */
    void (*handler)(Server *, Peer *, Message *); /* request handler function */
} PeerRequestHandler;

/**
 * Look up table for peer request handlers
 */
static PeerRequestHandler peer_request_handlers[] = {
    {"SERVER", Server_handle_SERVER},
    {"PASS", Server_handle_PASS}};

/**
 * Look up table for user request handlers
 */
static UserRequestHandler user_request_handlers[] = {
    {"NICK", Server_handle_NICK},
    {"USER", Server_handle_USER},
    {"PRIVMSG", Server_handle_PRIVMSG},
    {"NOTICE", Server_handle_NOTICE},
    {"PING", Server_handle_PING},
    {"QUIT", Server_handle_QUIT},
    {"MOTD", Server_handle_MOTD},
    {"INFO", Server_handle_INFO},
    {"LIST", Server_handle_LIST},
    {"WHO", Server_handle_WHO},
    {"WHOIS", Server_handle_WHOIS},
    {"JOIN", Server_handle_JOIN},
    {"PART", Server_handle_PART},
    {"NAMES", Server_handle_NAMES},
    {"TOPIC", Server_handle_TOPIC},
    {"LUSERS", Server_handle_LUSERS},
    {"HELP", Server_handle_HELP},
    {"CONNECT", Server_handle_CONNECT},
};

User *Server_get_user_by_nick(Server *serv, const char *nick) {
    return ht_get(serv->nick_to_user_map, nick);
}

void Server_remove_all_connections(Server *serv) {
    HashtableIter conn_itr;
    ht_iter_init(&conn_itr, serv->connections);
    Connection *conn = NULL;

    while (ht_iter_next(&conn_itr, NULL, (void **)&conn)) {
        Server_remove_connection(serv, conn);
    }
}

void Server_destroy(Server *serv) {
    assert(serv);

    save_channels(serv->name_to_channel_map, CHANNELS_FILENAME);
    Server_remove_all_connections(serv);

    ht_free(serv->name_to_channel_map);
    ht_free(serv->name_to_peer_map);
    ht_free(serv->nick_to_user_map);
    ht_free(serv->connections);

    close(serv->fd);
    close(serv->epollfd);
    free(serv->hostname);
    free(serv->port);
    free(serv->passwd);
    free(serv->info);
    free(serv->name);
    free(serv);

    log_debug("Server stopped");
    exit(0);
}

void _add_channels_to_map(Server *serv) {
    HashtableIter itr;
    ht_iter_init(&itr, serv->name_to_channel_map);
    Channel *chan = NULL;
    while (ht_iter_next(&itr, NULL, (void **)&chan)) {
        ht_set(serv->channel_to_serv_name_map, chan->name, serv->name);
    }
}

/**
 * Create and initialise the server. Bind socket to given port.
 */
Server *Server_create(const char *name) {
    assert(name);

    Server *serv = calloc(1, sizeof *serv);
    assert(serv);

    serv->motd_file = MOTD_FILENAME;
    serv->config_file = CONFIG_FILENAME;

    struct peer_info_t serv_info;

    if (!get_peer_info(CONFIG_FILENAME, name, &serv_info)) {
        log_error("server %s not found in config file %s", name, serv->config_file);
        exit(1);
    }

    serv->name = serv_info.peer_name;
    serv->port = serv_info.peer_port;
    serv->passwd = serv_info.peer_passwd;
    serv->hostname = serv_info.peer_host;

    serv->info = strdup(DEFAULT_INFO);

    serv->connections = ht_alloc_type(INT_TYPE, SHALLOW_TYPE);                /* Map<int, Connection *> */
    serv->name_to_peer_map = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE);        /* Map<string, Peer *> */
    serv->nick_to_serv_name_map = ht_alloc(STRING_TYPE, STRING_TYPE);         /* Map<string, string> */
    serv->channel_to_serv_name_map = ht_alloc_type(STRING_TYPE, STRING_TYPE); /* Map<string, string> */
    serv->nick_to_user_map = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE);        /* Map<string, User*>*/

    serv->name_to_channel_map = load_channels(CHANNELS_FILENAME); /* Map<string, Channel *>*/
    _add_channels_to_map(serv);

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
    serv->servaddr.sin_port = htons(atoi(serv->port));
    serv->servaddr.sin_addr.s_addr = INADDR_ANY;

    int yes = 1;

    // Set socket options
    CHECK(setsockopt(serv->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes), "setsockopt");

    // Bind
    CHECK(bind(serv->fd, (struct sockaddr *)&serv->servaddr, sizeof(struct sockaddr_in)), "bind");

    // Listen
    CHECK(listen(serv->fd, MAX_EVENTS), "listen");

    log_info("Server %s is running at %s:%s", serv->name, serv->hostname, serv->port);

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

void Server_flush_message_queues(Server *serv) {
    HashtableIter itr;
    ht_iter_init(&itr, serv->connections);

    Connection *conn = NULL;

    while (ht_iter_next(&itr, NULL, (void **)&conn)) {
        if (conn->conn_type == UNKNOWN_CONNECTION) {
            continue;
        }

        List *messages = NULL;

        if (conn->conn_type == USER_CONNECTION) {
            messages = ((User *)conn->data)->msg_queue;
        } else if (conn->conn_type == PEER_CONNECTION) {
            messages = ((Peer *)conn->data)->msg_queue;
        }

        // Move messages from message queue to outbox queue
        if (messages != NULL && List_size(messages) > 0) {
            ListNode *current = messages->head;

            while (current) {
                ListNode *tmp = current->next;
                List_push_back(conn->outgoing_messages, current->elem);
                free(current);
                current = tmp;
            }

            messages->head = messages->tail = NULL;
            messages->size = 0;
        }
    }
}

void Server_process_request_from_unknown(Server *serv, Connection *conn) {
    assert(conn->conn_type == UNKNOWN_CONNECTION);
    assert(conn->data == NULL);

    char *message = List_peek_front(conn->incoming_messages);
    if (strncmp(message, "NICK", 4) == 0 || strncmp(message, "USER", 4) == 0) {
        conn->conn_type = USER_CONNECTION;
        conn->data = User_alloc();
        ((User *)conn->data)->hostname = strdup(conn->hostname);
    } else if (strncmp(message, "PASS", 4) == 0) {
        conn->conn_type = PEER_CONNECTION;
        conn->data = Peer_alloc();
    } else {  // Ignore message
        List_pop_front(conn->incoming_messages);
        return;
    }
}

void Server_process_request_from_user(Server *serv, Connection *conn) {
    assert(conn->conn_type == USER_CONNECTION);

    User *usr = conn->data;
    assert(usr);

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

        for (size_t j = 0; j < sizeof(user_request_handlers) / sizeof(user_request_handlers[0]); j++) {
            UserRequestHandler handle = user_request_handlers[j];
            if (!strcmp(handle.name, message->command)) {
                handle.handler(serv, usr, message);
                found = true;
                break;
            }
        }

        if (found) {
            continue;
        }

        // Handle every other command

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

        conn->quit = usr->quit;
    }

    Vector_free(messages);
}

void Server_process_request_from_peer(Server *serv, Connection *conn) {
    assert(conn->conn_type == PEER_CONNECTION);

    Peer *peer = conn->data;
    assert(peer);

    while (List_size(conn->incoming_messages) > 0) {
        char *message_str = List_peek_front(conn->incoming_messages);
        assert(message_str);

        Message message;
        message_init(&message);

        if (parse_message(message_str, &message) == -1) {
            message_destroy(&message);
            continue;
        }

        if (!strcmp(message.command, "ERROR")) {
            log_error("Server %s error: %s", peer->name, message.body);
            Server_remove_connection(serv, conn);
            message_destroy(&message);
            return;
        }

        bool found = false;

        // Find and execute request handler for special commands
        for (size_t i = 0; i < sizeof(peer_request_handlers) / sizeof(peer_request_handlers[0]); i++) {
            PeerRequestHandler handler = peer_request_handlers[i];
            if (!strcmp(handler.name, message.command)) {
                handler.handler(serv, peer, &message);
                found = true;
                break;
            }
        }

        // Relay other commands to network
        if (!found) {
            if (message.origin && strcmp(message.origin, serv->hostname) != 0) {
                Server_broadcast_message(serv, message_str);
            }

            if (!strcmp(message.command, "NICK")) {
                ht_set(serv->nick_to_serv_name_map, message.params[0], peer->name);
            }
        }

        message_destroy(&message);
        List_pop_front(conn->incoming_messages);
    }

    conn->quit = peer->quit;
}

void Server_process_request(Server *serv, Connection *conn) {
    assert(serv);
    assert(conn);

    if (List_size(conn->incoming_messages) == 0) {
        return;
    }

    if (conn->conn_type == UNKNOWN_CONNECTION) {
        Server_process_request_from_unknown(serv, conn);
    }

    if (conn->conn_type == PEER_CONNECTION) {
        Server_process_request_from_peer(serv, conn);
    }

    if (conn->conn_type == USER_CONNECTION) {
        Server_process_request_from_user(serv, conn);
    }

    Server_flush_message_queues(serv);
}

void Server_broadcast_message(Server *serv, const char *message) {
    assert(serv);
    assert(message);

    if (!strlen(message)) {
        return;
    }

    HashtableIter itr;
    ht_iter_init(&itr, serv->connections);

    Connection *conn = NULL;

    while (ht_iter_next(&itr, NULL, (void **)&conn)) {
        List_push_back(conn->outgoing_messages, strdup(message));
    }
}

void Server_broadcast_to_channel(Server *serv, Channel *channel, const char *message) {
    assert(serv);
    assert(channel);
    assert(message);

    if (!strlen(message)) {
        return;
    }

    HashtableIter itr;
    ht_iter_init(&itr, channel->members);
    User *member = NULL;
    while (ht_iter_next(&itr, NULL, (void **)&member)) {
        List_push_back(member->msg_queue, strdup(message));
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
            res[len - 1] = 0;
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
        ht_remove(serv->nick_to_user_map, usr->nick, NULL, NULL);
        ht_remove(serv->nick_to_serv_name_map, usr->nick, NULL, NULL);

        // Remove user from channels
        for (size_t i = 0; i < Vector_size(usr->channels); i++) {
            char *name = Vector_get_at(usr->channels, i);
            Channel *channel = ht_get(serv->name_to_channel_map, name);
            if (channel) {
                Channel_remove_member(channel, usr);
            }
        }

        User_free(usr);

    } else if (connection->conn_type == PEER_CONNECTION) {
        Peer *peer = connection->data;

        log_info("Closing connection with peer %d", connection->fd);

        if (peer->name) {
            ht_remove(serv->name_to_peer_map, peer->name, NULL, NULL);
        }

        for (size_t i = 0; i < Vector_size(peer->nicks); i++) {
            char *nick = Vector_get_at(peer->nicks, i);
            ht_remove(serv->nick_to_serv_name_map, nick, NULL, NULL);
        }

        for (size_t i = 0; i < Vector_size(peer->channels); i++) {
            char *channel = Vector_get_at(peer->channels, i);
            ht_remove(serv->channel_to_serv_name_map, channel, NULL, NULL);
        }

        Peer_free(peer);
    }

    Connection_free(connection);
}

bool Server_add_peer(Server *serv, const char *name, const char *port) {
    // Load server info from file
    FILE *file = fopen(serv->config_file, "r");

    if (!file) {
        log_error("failed to open config file %s", serv->config_file);
        return false;
    }

    struct peer_info_t peer_info;

    if (!get_peer_info(serv->config_file, name, &peer_info)) {
        return false;
    }

    int fd = connect_to_host(peer_info.peer_host, port ? port : peer_info.peer_port);

    if (fd == -1) {
        return false;
    }

    Connection *conn = calloc(1, sizeof *conn);
    conn->fd = fd;
    conn->hostname = strdup(peer_info.peer_host);
    conn->port = atoi(peer_info.peer_port);
    conn->incoming_messages = List_alloc(NULL, free);
    conn->outgoing_messages = List_alloc(NULL, free);

    Peer *peer = Peer_alloc();
    peer->name = strdup(peer_info.peer_name);

    conn->conn_type = PEER_CONNECTION;
    conn->data = peer;

    ht_set(serv->connections, &fd, conn);
    ht_set(serv->name_to_peer_map, peer_info.peer_name, peer);

    List_push_back(conn->outgoing_messages, make_string("PASS %s * *\r\n", peer_info.peer_passwd));
    List_push_back(conn->outgoing_messages, make_string("SERVER %s :\r\n", serv->name));

    free(peer_info.peer_host);
    free(peer_info.peer_name);
    free(peer_info.peer_port);
    free(peer_info.peer_passwd);

    return true;
}
