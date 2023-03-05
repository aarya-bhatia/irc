/**
 * This file implements the functions to load and save nicks to file.
 *
 * File format: Each line of file should start with "username:" followed by a comma separated list of nicks for that username.
 */
#include "include/nicks.h"

#include "include/common.h"
#include "include/types.h"

/**
 * Load all nicks from file into a hashmap with the key being the username and value begin a vector of nicks.
 *
 */
Hashtable *load_nicks(const char *filename) {
    Hashtable *nick_map = ht_alloc();
    nick_map->value_copy = NULL;
    nick_map->value_free = (elem_free_type)Vector_free;

    FILE *file = fopen(filename, "r+");

    if (!file) {
        log_error("Failed to open file: %s", filename);
        return nick_map;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t nread = 0;

    while ((nread = getline(&line, &len, file)) > 0) {
        if (line[nread - 1] == '\n') {
            line[nread - 1] = 0;
        }

        char *saveptr1 = NULL;
        char *username = strtok_r(line, ":", &saveptr1);

        username = trimwhitespace(username);

        if (!username || username[0] == 0) {
            log_error("invalid line: %s", line);
            continue;
        }

        char *nicks = strtok_r(NULL, "", &saveptr1);

        // skip this user
        if (!nicks || nicks[0] == 0) {
            log_warn("username %s has no nicks", username);
            continue;
        }

        // Add all nicks on each line into one array

        Vector *linked = Vector_alloc(16, (elem_copy_type)strdup, free);

        char *saveptr = NULL;
        char *token = strtok_r(nicks, ",", &saveptr);

        while (token) {
            token = trimwhitespace(token);

            if (token && token[0] != 0) {
                // Check for duplicates
                bool found = false;

                for (size_t i = 0; i < Vector_size(linked); i++) {
                    char *nick = Vector_get_at(linked, i);
                    if (!strcmp(nick, token)) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    char *nick = strdup(token);
                    Vector_push(linked, nick);
                }
            }

            token = strtok_r(NULL, ",", &saveptr);
        }

        log_debug("Added %d nicks for username %s", Vector_size(linked), username);
        ht_set(nick_map, username, linked);
    }

    free(line);
    fclose(file);

    return nick_map;
}

/**
 * Write nicks to file from hashmap.
 */
void save_nicks(Hashtable *nick_map, const char *filename) {
    FILE *nick_file = fopen(filename, "w");

    if (!nick_file) {
        log_error("Failed to open nick file: %s", filename);
        return;
    }

    HashtableIter itr;
    ht_iter_init(&itr, nick_map);

    char *username = NULL;
    Vector *nicks = NULL;
    while (ht_iter_next(&itr, (void **)&username, (void **)&nicks)) {
        fprintf(nick_file, "%s:", username);

        for (size_t i = 0; i < Vector_size(nicks); i++) {
            fprintf(nick_file, "%s", (char *)Vector_get_at(nicks, i));

            if (i + 1 < Vector_size(nicks)) {
                fputc(',', nick_file);
            }
        }
        fprintf(nick_file, "\n");
    }

    fclose(nick_file);
    log_info("Saved nicks to file: %s", filename);
}