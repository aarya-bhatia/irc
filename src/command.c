#include "include/common.h"
#include "include/server.h"
#include "include/replies.h"

bool is_nick_available(Server *serv, char *nick)
{
    CC_HashTableIter iter;
    cc_hashtable_iter_init(&iter, serv->user_to_nicks_map);

    TableEntry *entry = NULL;

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
                    return false;
                }
            }
        }
    }

    return true;
}

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

void Server_reply_to_MOTD(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);
    assert(!strcmp(msg->command, "MOTD"));

    char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

    if (motd)
    {
        User_add_msg(usr, make_reply(RPL_MOTD_MSG, usr->nick, motd));
    }
    else
    {
        User_add_msg(usr, make_reply(ERR_NOMOTD_MSG, usr->nick));
    }
}

void Server_reply_to_NICK(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);

    assert(!strcmp(msg->command, "NICK"));

    if (msg->n_params != 1)
    {
        User_add_msg(usr, make_reply(ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
        return;
    }

    assert(msg->params[0]);

    // same nick
    if (usr->nick && !strcmp(msg->params[0], usr->nick))
    {
        log_info("usr nick did not change: %s", usr->nick);
        return;
    }

    // Check existing nick for user if any
    CC_Array *nicks = NULL;
    bool exists = false;
    if ((nicks = get_current_nicks(serv, usr)) != NULL)
    {
        for (size_t i = 0; i < cc_array_size(nicks); i++)
        {
            char *nick = NULL;
            if (cc_array_get_at(nicks, i, (void **)&nick) == CC_OK)
            {
                if (!strcmp(nick, msg->params[0]))
                {
                    log_info("user nick exists: %s", nick);
                    exists = true;
                    break;
                }
            }
        }
    }

    // nick unavailable
    if (!exists && !is_nick_available(serv, msg->params[0]))
    {
        User_add_msg(usr, make_reply(ERR_NICKNAMEINUSE_MSG, msg->params[0]));
        return;
    }

    // set nick
    usr->nick = strdup(msg->params[0]);
    usr->nick_changed = true;

    if (!exists)
    {
        log_info("user %s updated nick", usr->nick);
    }

    // update nick_map with new nick
    if (!exists && usr->nick_changed && usr->username && usr->nick)
    {
        if (!cc_hashtable_contains_key(serv->user_to_nicks_map, usr->username))
        {
            CC_Array *arr = NULL;
            cc_array_new(&arr);
            cc_hashtable_add(serv->user_to_nicks_map, strdup(usr->username), arr);
        }

        CC_Array *nicks = NULL;

        if (cc_hashtable_get(serv->user_to_nicks_map, usr->username, (void **)&nicks) == CC_OK)
        {
            cc_array_add(nicks, strdup(usr->nick));
            log_info("Added nick %s to users_to_nick_map", usr->nick);
        }
    }

    // Check if registration is completed
    if (!usr->registered && usr->username && usr->realname)
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
    }
}

void Server_reply_to_USER(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);

    assert(!strcmp(msg->command, "USER"));

    if (msg->n_params != 3 || !msg->body)
    {
        User_add_msg(usr, make_reply(ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
        return;
    }

    if (usr->realname && usr->username)
    {
        User_add_msg(usr, make_reply(ERR_ALREADYREGISTRED_MSG, usr->nick));
        return;
    }

    assert(msg->params[0]);

    usr->username = strdup(msg->params[0]);
    usr->realname = strdup(msg->body);

    log_debug("user %s set username to %s and realname to %s", usr->nick, usr->username, usr->realname);

    // update nick_map
    if (usr->nick_changed && usr->nick && usr->username)
    {
        if (!cc_hashtable_contains_key(serv->user_to_nicks_map, usr->username))
        {
            CC_Array *arr = NULL;
            cc_array_new(&arr);
            cc_hashtable_add(serv->user_to_nicks_map, strdup(usr->username), arr);
        }

        CC_Array *nicks = NULL;

        if (cc_hashtable_get(serv->user_to_nicks_map, usr->username, (void **)&nicks) == CC_OK)
        {
            cc_array_add(nicks, strdup(usr->nick));
            log_info("Added nick %s to users_to_nick_map", usr->nick);
        }
    }

    // Set the nick if user had previously registered with some nick
    if (!usr->nick_changed)
    {
        CC_Array *nicks = get_current_nicks(serv, usr);

        if (nicks && cc_array_size(nicks) > 0)
        {
            cc_array_get_at(nicks, 0, (void **)&usr->nick);
            log_info("user nick set from history: %s", usr->nick);
            usr->nick_changed = true;
        }
    }

    if (usr->nick_changed && !usr->registered)
    {
        // Registration completed
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
    }
}

void Server_reply_to_PING(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);

    assert(!strcmp(msg->command, "PING"));
    User_add_msg(usr, make_reply("PONG %s", serv->hostname));
}

void Server_reply_to_PRIVMSG(Server *serv, User *usr, Message *msg);

void Server_reply_to_QUIT(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);

    assert(!strcmp(msg->command, "QUIT"));

    char *reason = (msg->body ? msg->body : "Client Quit");

    User_add_msg(usr, make_reply("ERROR :Closing Link: %s (%s)", usr->hostname, reason));
}

void Server_reply_to_WHO(Server *serv, User *usr, Message *msg);
void Server_reply_to_WHOIS(Server *serv, User *usr, Message *msg);

void Server_reply_to_JOIN(Server *serv, User *usr, Message *msg);
void Server_reply_to_LIST(Server *serv, User *usr, Message *msg);
void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg);

void Server_reply_to_SERVER(Server *serv, User *usr, Message *msg);
void Server_reply_to_PASS(Server *serv, User *usr, Message *msg);
void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg);
