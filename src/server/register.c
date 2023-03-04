#include "include/register.h"

#include "include/K.h"
#include "include/replies.h"
#include "include/server.h"
#include "include/types.h"
#include "include/user.h"

/**
 * Check and complete user registration
 *
 * To complete registration, the user must have a username, realname and a nick.
 * If user has not set a nick, it is possible to use a previous nick if there is one and complete registration.
 */
bool check_registration_complete(Server *serv, User *usr) {
    if (usr->registered) {
        return true;
    }

    if (!usr->nick || !usr->username || !usr->realname) {
        return false;
    }

    // Get user nick from previously used nick
    if (!usr->nick_changed) {
        Vector *nicks = ht_get(serv->user_to_nicks_map, usr->username);

        if (nicks && Vector_size(nicks)) {
            char *nick = Vector_get_at(nicks, 0);
            usr->nick_changed = true;
            free(usr->nick);
            usr->nick = strdup(nick);
        }
    }

    if (usr->nick_changed) {
        usr->registered = true;

        List_push_back(usr->msg_queue, make_reply(":%s " RPL_WELCOME_MSG, serv->hostname,
                                                  usr->nick, usr->nick));
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_YOURHOST_MSG, serv->hostname,
                                                  usr->nick, usr->hostname));
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_CREATED_MSG, serv->hostname,
                                                  usr->nick, serv->created_at));
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_MYINFO_MSG, serv->hostname,
                                                  usr->nick, serv->hostname, "*", "*",
                                                  "*"));

        char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

        if (motd) {
            List_push_back(usr->msg_queue, make_reply(":%s " RPL_MOTD_MSG,
                                                      serv->hostname, usr->nick,
                                                      motd));
        } else {
            List_push_back(usr->msg_queue, make_reply(":%s " ERR_NOMOTD_MSG,
                                                      serv->hostname, usr->nick));
        }

        update_nick_map(serv, usr);

        log_info("registration completed for user %s", usr->nick);

        // TODO: Load user information from file if found

        return true;
    }

    return false;
}

/**
 * Check if user's nick needs to be recorded in nick_map and do the update.
 * Returns true if update was successful or false if there was any error.
 */
bool update_nick_map(Server *serv, User *usr) {
    assert(serv);
    assert(usr);

    if (!usr->registered) {
        log_warn("user %s has not registered", usr->nick);
        return false;
    }

    assert(usr->nick);
    assert(usr->username);
    assert(usr->realname);

    Vector *nicks = ht_get(serv->user_to_nicks_map, usr->username);

    if (!nicks) {
        log_info("Adding user %s to nick_map", usr->nick);
        nicks = calloc(1, sizeof *nicks);
        Vector_init(nicks, 4, strdup, free);
        ht_set(serv->user_to_nicks_map, usr->username, nicks);
    } else {
        log_info("Adding user %s to nick_map", usr->nick);
        Vector_push(nicks, usr->nick);
    }

    return true;
}

/**
 * This function checks if the given user is allowed to use the given nick.
 * A user can use a nick if no other user is using it now or has used it before,
 * or if the user has used same nick before.
 */
bool check_nick_available(Server *serv, User *usr, const char *nick) {
    // nick did not change
    if (usr->nick && !strcmp(usr->nick, nick)) {
        return true;
    }

    HashtableIter itr;
    ht_iter_init(&itr, serv->user_to_nicks_map);

    char *username = NULL;
    Vector *nicks = NULL;

    while (ht_iter_next(&itr, &username, &nicks)) {
        assert(username);
        assert(nicks);
        for (size_t i = 0; i < Vector_size(nicks); i++) {
            char *found = Vector_get_at(nicks, i);
            assert(found);

            if (!strcmp(found, nick)) {
                // Check if username belongs to current user
                if (usr->username && !strcmp(username, usr->username)) {
                    return true;
                }

                // nick is taken by another user
                return false;
            }
        }
    }

    // nick not used by any user
    return true;
}
