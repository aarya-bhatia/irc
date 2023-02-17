#include "include/common.h"
#include "include/server.h"
#include "include/replies.h"

bool _is_nick_available(Server *serv, char *nick)
{
    CC_HashTableIter iter;
    cc_hashtable_iter_init(&iter, serv->connections);
    TableEntry *entry = NULL;
    while (cc_hashtable_iter_next(&iter, &entry) != CC_ITER_END)
    {
        User *user = entry->value;
        if (user && user->nick && strcmp(user->nick, nick) == 0)
        {
            return false;
        }
    }

    return true;
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

    if (!_is_nick_available(serv, msg->params[0]))
    {
        User_add_msg(usr, make_reply(ERR_NICKNAMEINUSE_MSG, msg->params[0]));
        return;
    }

    usr->nick = strdup(msg->params[0]);

    log_info("user %s updated nick", usr->nick);

    usr->nick_changed = true;

    if (!usr->registered && usr->username && usr->realname)
    {
        // Registration completed
        usr->registered = true;

        User_add_msg(usr, make_reply(RPL_WELCOME_MSG, usr->nick, usr->nick));
        User_add_msg(usr, make_reply(RPL_YOURHOST_MSG, usr->nick, usr->hostname));
        User_add_msg(usr, make_reply(RPL_CREATED_MSG, usr->nick, serv->created_at));
        User_add_msg(usr, make_reply(RPL_MYINFO_MSG, usr->nick, serv->hostname, "*", "*", "*"));
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

    if (usr->nick_changed && !usr->registered)
    {
        // Registration completed
        usr->registered = true;

        User_add_msg(usr, make_reply(RPL_WELCOME_MSG, usr->nick, usr->nick));
        User_add_msg(usr, make_reply(RPL_YOURHOST_MSG, usr->nick, usr->hostname));
        User_add_msg(usr, make_reply(RPL_CREATED_MSG, usr->nick, serv->created_at));
        User_add_msg(usr, make_reply(RPL_MYINFO_MSG, usr->nick, serv->hostname, "*", "*", "*"));
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