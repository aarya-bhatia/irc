#include "include/user.h"

#include <sys/epoll.h>

#include "include/channel.h"
#include "include/server.h"
#include "include/types.h"

User *User_alloc(int fd, struct sockaddr *addr, socklen_t addrlen) {
    User *this = calloc(1, sizeof *this);
    this->fd = fd;                                              // client socket
    this->hostname = strdup(addr_to_string(addr, addrlen));     // client address
    this->nick = make_string("user%05d", (rand() % (int)1e5));  // temporary nick
    this->registered = false;
    this->nick_changed = false;
    this->quit = false;
    this->msg_queue = List_alloc(NULL, free);
    this->channels = Vector_alloc(4, (elem_copy_type)strdup, free);
    return this;
}

/**
 * Close connection with user and free all memory associated with them.
 */
void User_free(User *usr) {
    if (!usr) {
        return;
    }

    free(usr->nick);
    free(usr->username);
    free(usr->realname);
    free(usr->hostname);

    Vector_free(usr->channels);
    List_free(usr->msg_queue);

    // Close socket
    shutdown(usr->fd, SHUT_RDWR);
    close(usr->fd);
    free(usr);
}

/**
 * User is available to send data to server
 */
ssize_t User_Read_Event(Server *serv, User *usr) {
    assert(usr);
    assert(serv);

    // Read at most MAX_MSG_LEN bytes into the buffer
    ssize_t nread = read_all(usr->fd, usr->req_buf + usr->req_len,
                             MAX_MSG_LEN - usr->req_len);

    // If no bytes read and no messages sent
    if (nread <= 0 && !strstr(usr->req_buf, "\r\n")) {
        Server_remove_user(serv, usr);
        return -1;
    }

    usr->req_len += nread;
    usr->req_buf[usr->req_len] = 0;  // Null terminate the buffer

    log_debug("Read %zd bytes from user %s", nread, usr->nick);

    // Checks if a complete message exists (if buffer has a \r\n)
    if (strstr(usr->req_buf, "\r\n")) {
        // Buffer to store partial message
        char tmp[MAX_MSG_LEN + 1];
        tmp[0] = 0;

        // get last "\r\n"
        char *t = rstrstr(usr->req_buf, "\r\n");
        t += 2;

        // Check if there is a partial message
        if (*t) {
            strcpy(tmp, t);  // Copy partial message to temp buffer
            *t = 0;          // Shorten the request buffer to last complete message
        }
        // Process all CRLF-terminated messages from request buffer
        Server_process_request(serv, usr);

        // Copy pending message to request buffer
        strcpy(usr->req_buf, tmp);
        usr->req_len = strlen(tmp);

        // log_debug("Request buffer size for user %s: %zu", usr->nick, usr->req_len);
    }

    return nread;
}

/**
 * User is available to recieve data from server
 */
ssize_t User_Write_Event(Server *serv, User *usr) {
    assert(usr);
    assert(usr->msg_queue);
    assert(serv);

    // A message is still being sent
    if (usr->res_len && usr->res_off < usr->res_len) {
        // Send remaining message to user
        ssize_t nsent = write_all(usr->fd, usr->res_buf + usr->res_off,
                                  usr->res_len - usr->res_off);

        log_debug("Sent %zd bytes to user %s", nsent, usr->nick);

        if (nsent <= 0) {
            Server_remove_user(serv, usr);
            return -1;
        }

        usr->res_off += nsent;

        // Entire message was sent
        if (usr->res_off >= usr->res_len) {
            // Mark response buffer as empty
            usr->res_off = usr->res_len = 0;
        }

        return nsent;
    }

    // Check for pending messages
    if (List_size(usr->msg_queue)) {
        char *msg = List_peek_front(usr->msg_queue);

        strcpy(usr->res_buf, msg);
        usr->res_len = strlen(msg);
        usr->res_off = 0;

        List_pop_front(usr->msg_queue);
    }

    return 0;
}

bool Server_add_user(Server *serv, User *usr) {
    // Make user socket non-blocking
    if (fcntl(usr->fd, F_SETFL, fcntl(usr->fd, F_GETFL) | O_NONBLOCK) != 0) {
        perror("fcntl");
        return false;
    }

    // Create event for read/write events from user
    struct epoll_event ev = {.data.fd = usr->fd, .events = EPOLLIN | EPOLLOUT};

    // Add user socket to epoll set
    if (epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, usr->fd, &ev) !=
        0) {
        perror("epoll_ctl");
        return false;
    }

    // Add user socket to sock_to_user_map hashmap
    ht_set(serv->sock_to_user_map, &usr->fd, usr);

    log_info("Got connection %d from %s", usr->fd, usr->hostname);

    return true;
}

void Server_remove_user(Server *serv, User *usr) {
    assert(serv);
    assert(usr);

    log_info("Closing connection with user (%d): %s", usr->fd, usr->nick);

    epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, usr->fd, NULL);

    ht_remove(serv->sock_to_user_map, &usr->fd, NULL, NULL);
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
}

bool User_is_member(User *usr, const char *channel_name) {
    for (size_t i = 0; i < Vector_size(usr->channels); i++) {
        if (!strcmp(Vector_get_at(usr->channels, i), channel_name)) {
            return true;
        }
    }

    return false;
}

void User_add_channel(User *usr, const char *channel_name) {
    if (!User_is_member(usr, channel_name)) {
        Vector_push(usr->channels, (void *) channel_name);
    }
}

bool User_remove_channel(User *usr, const char *channel_name) {
    for (size_t i = 0; i < Vector_size(usr->channels); i++) {
        if (!strcmp(Vector_get_at(usr->channels, i), channel_name)) {
            Vector_remove(usr->channels, i, NULL);
            return true;
        }
    }

    return false;
}