#include <sys/epoll.h>

#include "include/server.h"

User *User_alloc() {
    User *this = calloc(1, sizeof *this);
    this->nick = make_string("user%05d", (rand() % (int)1e5));  // temporary nick
    this->channels = Vector_alloc(4, (elem_copy_type)strdup, free);
    this->msg_queue = List_alloc(NULL, free);
    return this;
}

void User_free(User *usr) {
    free(usr->nick);
    free(usr->username);
    free(usr->realname);
    free(usr->hostname);

    Vector_free(usr->channels);
    List_free(usr->msg_queue);

    free(usr);
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
        Vector_push(usr->channels, (void *)channel_name);
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