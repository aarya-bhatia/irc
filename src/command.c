#include "common.h"
#include "server.h"
#include "replies.h"

// TODO: Add server hostname prefixes

#define make_reply(format, ...) make_string(format "\r\n", __VA_ARGS__)

bool _is_nick_available(Server *serv, char *nick)
{
    CC_HashTableIter iter;
    cc_hashtable_iter_init(&iter, serv->connections);
    TableEntry *entry = NULL;
    while(cc_hashtable_iter_next(&iter, &entry) != CC_ITER_END){
        User *user = entry->value;
        if(user && user->nick && strcmp(user->nick, nick) == 0) {
            return false;
        }
    }

    return true;
}

void Server_reply_to_Nick(Server *serv, User *usr, Message *msg)
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

    if(!_is_nick_available(serv, msg->params[0]))
    {
        User_add_msg(usr, make_reply(ERR_NICKNAMEINUSE_MSG, msg->params[0]));
        return;
    }

    usr->nick = strdup(msg->params[0]);

    log_info("user %d set nick to %s", usr->fd, usr->nick);

    if (usr->nick && usr->username && usr->realname)
    {
        // Registration completed

        User_add_msg(usr, make_reply(RPL_WELCOME_MSG, usr->nick, usr->nick));
        User_add_msg(usr, make_reply(RPL_YOURHOST_MSG, usr->nick, usr->hostname));
        User_add_msg(usr, make_reply(RPL_CREATED_MSG, usr->nick, serv->created_at));
        User_add_msg(usr, make_reply(RPL_MYINFO_MSG, usr->nick, serv->hostname, "*", "*", "*"));
    }
}

void Server_reply_to_User(Server *serv, User *usr, Message *msg)
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

    if(usr->realname && usr->username)
    {
        User_add_msg(usr, make_reply(ERR_ALREADYREGISTRED_MSG, usr->nick));
        return;
    }

    assert(msg->params[0]);

    usr->username = strdup(msg->params[0]);
    usr->realname = strdup(msg->body);

    log_debug("user %d username=%s realname=%s", usr->fd, usr->username, usr->realname);

    if (usr->nick && usr->username && usr->realname)
    {
        // Registration completed

        User_add_msg(usr, make_reply(RPL_WELCOME_MSG, usr->nick, usr->nick));
        User_add_msg(usr, make_reply(RPL_YOURHOST_MSG, usr->nick, usr->hostname));
        User_add_msg(usr, make_reply(RPL_CREATED_MSG, usr->nick, serv->created_at));
        User_add_msg(usr, make_reply(RPL_MYINFO_MSG, usr->nick, serv->hostname, "*", "*", "*"));
    }
}

void Server_reply_to_Privmsg(Server *serv, User *usr, Message *msg);
