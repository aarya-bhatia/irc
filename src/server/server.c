#include "include/server.h"

#include <sys/epoll.h>
#include <sys/stat.h>
#include <time.h>

#include "include/channel.h"
#include "include/replies.h"
#include "include/types.h"
#include "include/user.h"

static struct rpl_handle_t rpl_handlers[] = {
    {"NICK", Server_reply_to_NICK},
    {"USER", Server_reply_to_USER},
    {"PRIVMSG", Server_reply_to_PRIVMSG},
    {"NOTICE", Server_reply_to_NOTICE},
    {"PING", Server_reply_to_PING},
    {"QUIT", Server_reply_to_QUIT},
    {"MOTD", Server_reply_to_MOTD},
    {"LIST", Server_reply_to_LIST},
    {"WHO", Server_reply_to_WHO},
    {"WHOIS", Server_reply_to_WHOIS},
    {"SERVER", Server_reply_to_SERVER},
    {"CONNECT", Server_reply_to_CONNECT},
    {"JOIN", Server_reply_to_JOIN},
    {"PART", Server_reply_to_PART},
    {"NAMES", Server_reply_to_NAMES},
    {"TOPIC", Server_reply_to_TOPIC},
    {"LUSERS", Server_reply_to_LUSERS},
    {"HELP", Server_reply_to_HELP},
};

User *Server_get_user_by_socket(Server *serv, int sock) {
    return ht_get(serv->sock_to_user_map, &sock);
}

User *Server_get_user_by_nick(Server *serv, const char *nick) {
    const char *username = ht_get(serv->online_nick_to_username_map, nick);
    return username == NULL ? NULL : ht_get(serv->username_to_user_map, username);
}

User *Server_get_user_by_username(Server *serv, const char *username) {
    return ht_get(serv->username_to_user_map, username);
}

void _close_connection(void *fd, void *usr) {
    (void)fd;
    User_free((User *)usr);
}

void Server_destroy(Server *serv) {
    assert(serv);
    ht_foreach(serv->sock_to_user_map, _close_connection);
    save_channels(serv->channels_map, CHANNELS_FILENAME);

    ht_free(serv->sock_to_user_map);
    ht_free(serv->username_to_user_map);
    ht_free(serv->online_nick_to_username_map);
    ht_free(serv->offline_nick_to_username_map);
    ht_free(serv->channels_map);

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

    serv->sock_to_user_map = ht_alloc();             /* Map<int, User *> */
    serv->username_to_user_map = ht_alloc();         /* Map<string, User*> */
    serv->online_nick_to_username_map = ht_alloc();  /* Map<string, string>*/
    serv->offline_nick_to_username_map = ht_alloc(); /* Map<string, string>*/

    serv->sock_to_user_map->key_len = sizeof(int);
    serv->sock_to_user_map->key_compare = (compare_type)int_compare;
    serv->sock_to_user_map->key_copy = (elem_copy_type)int_copy;
    serv->sock_to_user_map->key_free = free;

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
        int conn_sock =
            accept(serv->fd, (struct sockaddr *)&client_addr, &addrlen);

        if (conn_sock == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }

            die("accept");
        }

        User *usr = User_alloc(conn_sock, (struct sockaddr *)&client_addr, addrlen);

        if (!Server_add_user(serv, usr)) {
            Server_remove_user(serv, usr);
        }
    }
}

void Server_process_request(Server *serv, User *usr) {
    assert(serv);
    assert(usr);
    assert(strstr(usr->req_buf, "\r\n"));

    // Get array of parsed messages
    Vector *messages = parse_all_messages(usr->req_buf);
    assert(messages);

    log_debug("Processing %zu messages from user %s", Vector_size(messages), usr->nick);

    usr->req_buf[0] = 0;
    usr->req_len = 0;

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
                handle.function(serv, usr, message);
                found = true;
                break;
            }
        }

        if (found) {
            continue;
        }

        // Handle every other command:

        if (!usr->registered) {
            char *reply = make_reply(":%s " ERR_NOTREGISTERED_MSG,
                                     serv->hostname,
                                     usr->nick);
            List_push_back(usr->msg_queue, reply);
        } else {
            char *reply = make_reply(":%s " ERR_UNKNOWNCOMMAND_MSG,
                                     serv->hostname,
                                     usr->nick,
                                     message->command);
            List_push_back(usr->msg_queue, reply);
        }
    }

    Vector_free(messages);
}

void Server_broadcast_message(Server *serv, const char *message) {
    HashtableIter itr;
    ht_iter_init(&itr, serv->sock_to_user_map);
    int user_sock;
    User *user_data = NULL;

    while (ht_iter_next(&itr, (void **)&user_sock, (void **)&user_data)) {
        assert(user_data);
        if (user_data->registered) {
            List_push_back(user_data->msg_queue, strdup(message));
        }
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
