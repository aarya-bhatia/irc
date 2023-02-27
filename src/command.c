#include "include/common.h"
#include "include/server.h"
#include "include/replies.h"

bool check_nick_available(Server *serv, User *usr, char *nick);
CC_Array *get_current_nicks(Server *serv, User *usr);
bool update_nick_map(Server *serv, User *usr);
bool check_registration_complete(Server *serv, User *usr);

void _sanity_check(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);
}

void Server_reply_to_NICK(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);

    assert(!strcmp(msg->command, "NICK"));

    if (msg->n_params != 1)
    {
        User_add_msg(usr, make_reply(ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
        return;
    }

    assert(msg->params[0]);

    char *new_nick = msg->params[0];

    if (!check_nick_available(serv, usr, new_nick))
    {
        User_add_msg(usr, make_reply(ERR_NICKNAMEINUSE_MSG, msg->params[0]));
        return;
    }

    free(usr->nick);

    usr->nick = strdup(new_nick);
    usr->nick_changed = true;

    log_info("user %s updated nick", usr->nick);

    check_registration_complete(serv, usr);
    update_nick_map(serv, usr);
}

void Server_reply_to_USER(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);

    assert(!strcmp(msg->command, "USER"));

    if (msg->n_params != 3 || !msg->body)
    {
        User_add_msg(usr, make_reply(ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
        return;
    }

    if (usr->registered)
    {
        User_add_msg(usr, make_reply(ERR_ALREADYREGISTRED_MSG, usr->nick));
        return;
    }

    assert(msg->params[0]);

    char *username = msg->params[0];
    char *realname = msg->body;

    free(usr->username);
    free(usr->realname);

    usr->username = strdup(username);
    usr->realname = strdup(realname);

    log_debug("user %s set username to %s and realname to %s", usr->nick, username, realname);

    check_registration_complete(serv, usr);
}

void Server_reply_to_MOTD(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
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

void Server_reply_to_PING(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "PING"));
    User_add_msg(usr, make_reply("PONG %s", serv->hostname));
}

void Server_reply_to_PRIVMSG(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_QUIT(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "QUIT"));

    char *reason = (msg->body ? msg->body : "Client Quit");

    User_add_msg(usr, make_reply("ERROR :Closing Link: %s (%s)", usr->hostname, reason));
}

void Server_reply_to_WHO(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_WHOIS(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_JOIN(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_LIST(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_SERVER(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_PASS(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg)
{
    _sanity_check(serv, usr, msg);
}
