#include "common.h"
#include "server.h"
#include "replies.h"

void Server_reply_to_Nick(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);

    assert(!strcmp(msg->command, "NICK"));

    if (msg->n_params != 1)
    {
        // invalid params
    }

    assert(msg->params[0]);

    usr->nick = strdup(msg->params[0]);

    log_info("user %d set nick to %s", usr->fd, usr->nick);

    if (usr->nick && usr->username && usr->realname)
    {
        // Registration completed
        cc_list_add_last(usr->msg_queue, Reply_Welcome(serv, usr));
        cc_list_add_last(usr->msg_queue, Reply_Your_Host(serv, usr));
        cc_list_add_last(usr->msg_queue, Reply_Created(serv, usr));
        cc_list_add_last(usr->msg_queue, Reply_My_Info(serv, usr));
    }
}

void Server_reply_to_User(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);

    assert(!strcmp(msg->command, "USER"));

    if (msg->n_params != 3)
    {
        // invalid params
    }

    if (!msg->body)
    {
        // invalid message
    }

    assert(msg->params[0]);

    usr->username = strdup(msg->params[0]);
    usr->realname = strdup(msg->body);

    log_debug("user %d username=%s realname=%s", usr->fd, usr->username, usr->realname);

    if (usr->nick && usr->username && usr->realname)
    {
        // Registration completed
        cc_list_add_last(usr->msg_queue, Reply_Welcome(serv, usr));
        cc_list_add_last(usr->msg_queue, Reply_Your_Host(serv, usr));
        cc_list_add_last(usr->msg_queue, Reply_Created(serv, usr));
        cc_list_add_last(usr->msg_queue, Reply_My_Info(serv, usr));
    }
}

void Server_reply_to_Privmsg(Server *serv, User *usr, Message *msg);
