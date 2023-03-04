#include "include/server.h"

#include <sys/epoll.h>
#include <sys/stat.h>
#include <time.h>

#include "include/K.h"
#include "include/channel.h"
#include "include/register.h"
#include "include/replies.h"
#include "include/types.h"
#include "include/user.h"

static FILE *nick_file = NULL;

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
            continue;
        }

        if (!strcmp(message->command, "MOTD")) {
            Server_reply_to_MOTD(serv, usr, message);
        } else if (!strcmp(message->command, "NICK")) {
            Server_reply_to_NICK(serv, usr, message);
        } else if (!strcmp(message->command, "USER")) {
            Server_reply_to_USER(serv, usr, message);
        } else if (!strcmp(message->command, "PING")) {
            Server_reply_to_PING(serv, usr, message);
        } else if (!strcmp(message->command, "PRIVMSG")) {
            Server_reply_to_PRIVMSG(serv, usr, message);
        } else if (!strcmp(message->command, "JOIN")) {
            Server_reply_to_JOIN(serv, usr, message);
        } else if (!strcmp(message->command, "QUIT")) {
            Server_reply_to_QUIT(serv, usr, message);
            assert(List_size(usr->msg_queue) > 0);
            usr->quit = true;
        } else if (!usr->registered) {
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

    Vector_destroy(messages);  // Should destroy the struct with all its elements
}

/**
 * Callback function to append next entry from hashmap to the open nick file
 */
void _write_nick_to_file(void *v_username, void *v_nicks) {
    assert(nick_file);

    fprintf(nick_file, "%s:", (char *)v_username);

    Vector *nicks = v_nicks;
    assert(nicks);

    for (size_t i = 0; i < Vector_size(nicks); i++) {
        fprintf(nick_file, "%s", (char *)Vector_get_at(nicks, i));

        if (i + 1 < Vector_size(nicks)) {
            fputc(',', nick_file);
        }
    }

    fprintf(nick_file, "\n");
}

/**
 * Copy nicks to file
 */
void write_nicks_to_file(Server *serv, char *filename) {
    if (!nick_file) {
        nick_file = fopen(filename, "w");
    }

    if (!nick_file) {
        log_error("Failed to open nick file: %s", filename);
        return;
    }

    ht_foreach(serv->user_to_nicks_map, _write_nick_to_file);

    fclose(nick_file);
    nick_file = NULL;

    log_info("Wrote nicks to file: %s", NICKS_FILENAME);
}

void _close_connection(void *fd, void *usr) {
    (void)fd;
    User_free((User *)usr);
}

void Server_destroy(Server *serv) {
    assert(serv);

    ht_foreach(serv->connections, _close_connection);
    ht_destroy(serv->connections);

    write_nicks_to_file(serv, NICKS_FILENAME);
    ht_destroy(serv->user_to_nicks_map);

    ht_destroy(serv->user_to_sock_map);

    Vector_foreach(serv->channels, (void (*)(void *)) Channel_save_to_file);
    Vector_destroy(serv->channels);

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

    serv->connections = calloc(1, sizeof *serv->connections);             /* Map<int, User *> */
    serv->user_to_sock_map = calloc(1, sizeof *serv->user_to_sock_map);   /* Map<char *, int> */
    serv->user_to_nicks_map = calloc(1, sizeof *serv->user_to_nicks_map); /* Map<chat *, Vector<char *>> */

    ht_init(serv->connections);
    ht_init(serv->user_to_sock_map);
    ht_init(serv->user_to_nicks_map);

    serv->connections->key_len = sizeof(int);
    serv->connections->key_compare = (compare_type) int_compare;
    serv->connections->key_copy = (elem_copy_type)int_copy;
    serv->connections->key_free = free;
    serv->connections->value_copy = NULL;
    serv->connections->value_free = NULL;

    serv->user_to_sock_map->key_len = sizeof(char *);
    serv->user_to_sock_map->key_compare = (compare_type)strcmp;
    serv->user_to_sock_map->key_copy = (elem_copy_type)strdup;
    serv->user_to_sock_map->key_free = free;
    serv->user_to_sock_map->value_copy = (elem_copy_type)int_copy;
    serv->user_to_sock_map->value_free = free;

    serv->user_to_nicks_map->key_len = sizeof(char *);
    serv->user_to_nicks_map->key_compare = (compare_type)strcmp;
    serv->user_to_nicks_map->key_copy = (elem_copy_type)strdup;
    serv->user_to_nicks_map->key_free = (elem_free_type)free;
    serv->user_to_nicks_map->value_copy = NULL;
    serv->user_to_nicks_map->value_free = (elem_free_type)Vector_destroy;

    load_nicks(serv->user_to_nicks_map, NICKS_FILENAME);

    serv->channels = calloc(1, sizeof *serv->channels);

    Vector_init(serv->channels, 16, NULL, (elem_free_type)Channel_free);

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
 * There are new connections available
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
