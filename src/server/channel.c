#include "include/channel.h"

#include <time.h>

#include "include/K.h"
#include "include/membership.h"
#include "include/types.h"

/**
 * Create and initialise new channel with given name and return it.
 */
Channel *Channel_alloc(const char *name) {
    Channel *channel = calloc(1, sizeof *channel);
    channel->name = strdup(name);
    channel->time_created = time(NULL);
    channel->user_limit = MAX_CHANNEL_USERS;
    channel->members = Vector_alloc(10, NULL, (elem_free_type)Membership_free);
    return channel;
}

/**
 * Destroy all memory associated with channel
 */
void Channel_free(Channel *this) {
    Vector_free(this->members);
    free(this->topic);
    free(this->name);
    free(this);
}

bool _find_member(void *elem, const void *arg) {
    Membership *m = elem;
    const char *username = arg;
    return m && m->username && strcmp(m->username, username) == 0;
}

/**
 * Check if given user is part of channel.
 */
bool Channel_has_member(Channel *this, const char *username) {
    assert(this);
    assert(username);

    return Vector_find(this->members, _find_member, username) != NULL;
}

/**
 * Create membership for given user and add them to channel.
 */
void Channel_add_member(Channel *this, const char *username) {
    assert(this);
    assert(username);

    if (!Channel_has_member(this, username)) {
        Vector_push(this->members, Membership_alloc(this->name, username, 0));
        log_info("User %s added to channel %s", username, this->name);
    }
}

/**
 * Removes member from channel if they exist.
 * Returns true on success, false if they did not exist.
 */
bool Channel_remove_member(Channel *this, const char *username) {
    assert(this);
    assert(username);

    for (size_t i = 0; i < Vector_size(this->members); i++) {
        Membership *m = Vector_get_at(this->members, i);
        assert(!strcmp(m->channel, this->name));
        if (!strcmp(m->username, username)) {
            Vector_remove(this->members, i, NULL);
            return true;
        }
    }

    log_error("User %s not found in channel %s", username, this->name);
    return false;
}

/**
 * Save the channel information to given file.
 * Note: The given file will be overwritten.
 *
 * Line 1: name, time_created, mode, user_limit
 * Line 2: topic
 * Line 3-n: member name, member mode
 *
 */
void Channel_save_to_file(Channel *this) {
    assert(this);

    char filename[100];
    sprintf(filename, CHANNELS_DIRNAME "/%s", this->name);

    FILE *file = fopen(filename, "w");

    if (!file) {
        perror("fopen");
        log_error("Failed to open file %s", filename);
    }

    fprintf(file, "%s %ld %d %d\n", this->name,
            (unsigned long)this->time_created, this->mode, this->user_limit);

    fprintf(file, "%s\n", this->topic);

    for (size_t i = 0; i < Vector_size(this->members); i++) {
        Membership *member = Vector_get_at(this->members, i);
        fprintf(file, "%s %d\n", member->username, member->mode);
    }

    fclose(file);

    log_info("Saved channel %s to file %s", this->name, filename);
}

/**
 * Allocate a channel struct and loads its data from given file.
 * Returns NULL if file not found or file has invalid format.
 * On success, it will return the new struct.
 *
 * Line 1: name, time_created, mode, user_limit
 * Line 2: topic
 * Line 3-n: member name, member mode
 */
Channel *Channel_load_from_file(const char *filename) {
    Vector *lines = readlines(filename);

    if (!lines) {
        return NULL;
    }

    if (Vector_size(lines) < 2) {
        Vector_free(lines);
        log_error("invalid file format");
        return NULL;
    }

    log_debug("Read %zu lines from file %s", Vector_size(lines), filename);

    Channel *this = calloc(1, sizeof *this);

    char *line = Vector_get_at(lines, 0);
    char *ptr = strstr(line, " ");
    this->name = calloc(ptr - line, 1);

    if (sscanf(line, "%s %ld %d %d", this->name, (long *)&this->time_created, (int *)&this->mode, (int *)&this->user_limit) != 4) {
        log_error("invalid line: %s", line);
        Vector_free(lines);
        free(this);
        return NULL;
    }

    this->topic = strdup(Vector_get_at(lines, 1));
    this->members = Vector_alloc(10, NULL, (elem_free_type)Membership_free);

    for (size_t i = 2; i < Vector_size(lines); i++) {
        char *line = Vector_get_at(lines, i);
        char *save = NULL;
        char *username = strtok_r(line, " ", &save);

        if (!username) {
            log_error("invalid file format");
            break;
        }

        char *mode = strtok_r(NULL, " ", &save);

        if (!mode) {
            log_error("invalid file format");
            break;
        }

        log_debug("Adding member %s (mode %s) to channel %s", username, mode, this->name);
        Vector_push(this->members, Membership_alloc(this->name, username, atoi(mode)));
    }

    log_info("Loaded channel %s from file %s", this->name, filename);

    Vector_free(lines);
    return this;
}

/**
 * Find channel by name if loaded and return it.
 * Loads channel from file into memory if exists.
 * Returns NULL if not found.
 */
Channel *Server_get_channel(Server *serv, const char *name) {
    // Check if channel exists in memory
    for (size_t i = 0; i < Vector_size(serv->channels); i++) {
        Channel *channel = Vector_get_at(serv->channels, i);
        if (!strcmp(channel->name, name)) {
            return channel;
        }
    }

    // Check if channel exists in file
    char filename[100];
    sprintf(filename, CHANNELS_DIRNAME "/%s", name);

    if (access(filename, F_OK) == 0) {
        Channel *channel = Channel_load_from_file(filename);

        if (channel) {
            log_info("members: %d, topic: %s, mode: %d", Vector_size(channel->members), channel->topic, channel->mode);
            Vector_push(serv->channels, channel);
            return channel;
        }
    }

    log_warn("Channel %s not found", name);

    // Channel does not exist or file is corrupted
    return NULL;
}

/**
 * Removes channel from server array and destroys it.
 * Returns true on success and false on failure.
 */
bool Server_remove_channel(Server *serv, const char *name) {
    assert(serv);
    assert(name);

    Channel *channel = Server_get_channel(serv, name);

    if (!channel) {
        return false;
    }

    // Remove channel from channel list
    for (size_t i = 0; i < Vector_size(serv->channels); i++) {
        Channel *current = Vector_get_at(serv->channels, i);
        log_info("Channel %s was removed from server", name);
        if (!strcmp(current->name, name)) {
            Vector_remove(serv->channels, i, NULL);
            return true;
        }
    }

    log_warn("Channel %s not found", name);

    return false;
}
