#include "include/register.h"

/**
 * Check and complete user registration
 *
 * To complete registration, the user must have a username, realname and a nick.
 * If user has not set a nick, it is possible to use a previous nick if there is one and complete registration.
 */
bool check_registration_complete(Server *serv, User *usr)
{
    if (usr->registered)
    {
        return true;
    }

    if (!usr->nick || !usr->username || !usr->realname)
    {
        return false;
    }

    if (!usr->nick_changed)
    {
        CC_Array *nicks = get_current_nicks(serv, usr);

        if (nicks && cc_array_size(nicks) > 0)
        {
            char *nick = NULL;

            if (cc_array_get_at(nicks, 0, (void **)&nick) == CC_OK)
            {
                usr->nick_changed = true;
                usr->nick = strdup(nick);
            }
        }
    }

    if (usr->nick_changed)
    {
        usr->registered = true;

        User_add_msg(usr, make_reply(RPL_WELCOME_MSG, usr->nick, usr->nick));
        User_add_msg(usr, make_reply(RPL_YOURHOST_MSG, usr->nick, usr->hostname));
        User_add_msg(usr, make_reply(RPL_CREATED_MSG, usr->nick, serv->created_at));
        User_add_msg(usr, make_reply(RPL_MYINFO_MSG, usr->nick, serv->hostname, "*", "*", "*"));

        char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

        if (motd)
        {
            User_add_msg(usr, make_reply(RPL_MOTD_MSG, usr->nick, motd));
        }
        else
        {
            User_add_msg(usr, make_reply(ERR_NOMOTD_MSG, usr->nick));
        }

        update_nick_map(serv, usr);

        return true;
    }

    return false;
}

/**
 * Check if user's nick needs to be recorded in nick_map and do the update.
 * Returns true if update was successful or false if there was any error.
 */
bool update_nick_map(Server *serv, User *usr)
{
    assert(serv);
    assert(usr);

    if (!usr->registered)
    {
        log_warn("user %s has not registered", usr->nick);
        return false;
    }

    assert(usr->nick);
    assert(usr->username);
    assert(usr->realname);

    if (!cc_hashtable_contains_key(serv->user_to_nicks_map, usr->username))
    {
        log_debug("Adding user %s to nick_map", usr->nick);
        CC_Array *arr = NULL;
        cc_array_new(&arr);
        cc_array_add(arr, strdup(usr->nick));
        cc_hashtable_add(serv->user_to_nicks_map, strdup(usr->username), arr);
        return true;
    }

    CC_Array *nicks = NULL;

    if (cc_hashtable_get(serv->user_to_nicks_map, usr->username, (void **)&nicks) == CC_OK)
    {
        // Check if nick already exists in array
        for (size_t i = 0; i < cc_array_size(nicks); i++)
        {
            char *value = NULL;

            if (cc_array_get_at(nicks, i, (void **)&value) == CC_OK && !strcmp(value, usr->nick))
            {
                return true;
            }
        }

        log_info("Adding user %s to nick_map", usr->nick);
        cc_array_add(nicks, strdup(usr->nick));
        return true;
    }

    return false;
}

/**
 * Get current nicks for given user.
 */
CC_Array *get_current_nicks(Server *serv, User *usr)
{
    if (!usr->username || !cc_hashtable_contains_key(serv->user_to_nicks_map, usr->username))
    {
        return NULL;
    }

    CC_Array *nicks = NULL;

    if (cc_hashtable_get(serv->user_to_nicks_map, usr->username, (void **)&nicks) != CC_OK)
    {
        return NULL;
    }

    return nicks;
}

/**
 * This function checks if the given user is allowed to use the given nick.
 * A user can use a nick if no other user is using it now or has used it before,
 * or if the user has used same nick before.
 */
bool check_nick_available(Server *serv, User *usr, char *nick)
{
    if (usr->nick && !strcmp(usr->nick, nick))
    {
        // nick did not change
        return true;
    }

    CC_HashTableIter iter;
    cc_hashtable_iter_init(&iter, serv->user_to_nicks_map);

    TableEntry *entry = NULL;

    // Find nick
    while (cc_hashtable_iter_next(&iter, &entry) != CC_ITER_END)
    {
        CC_Array *nicks = entry->value;

        for (size_t i = 0; i < cc_array_size(nicks); i++)
        {
            char *found = NULL;

            if (cc_array_get_at(nicks, i, (void **)&found) == CC_OK)
            {
                if (!strcmp(found, nick))
                {
                    // Check if username belongs to current user
                    if (usr->username && !strcmp(entry->key, usr->username))
                    {
                        return true;
                    }

                    // nick is taken by another user
                    return false;
                }
            }
        }
    }

    // nick not used by any user
    return true;
}