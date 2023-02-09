#include "common.h"
#include "server.h"

char *Server_Nick_Command(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);

    assert(!strcmp(msg->command, "NICK"));

    char *response;

    if (msg->n_params != 1)
    {
        asprintf(&response, ":%s INVALID_PARAMS\r\n", serv->hostname);
        return response;
    }

    assert(msg->params[0]);
    usr->nick = strdup(msg->params[0]);
    log_debug("user%d set nick to %s", usr->fd, usr->nick);

    if (usr->nick && usr->username && usr->realname)
    {
        log_debug("user%d registration complete", usr->fd);
    }

    asprintf(&response, ":%s OK\r\n", serv->hostname);
    return response;
}

char *Server_User_Command(Server *serv, User *usr, Message *msg)
{
    assert(serv);
    assert(usr);
    assert(msg);

    assert(!strcmp(msg->command, "USER"));

    char *response;

    if (msg->n_params != 3)
    {
        asprintf(&response, ":%s INVALID_PARAMS\r\n", serv->hostname);
        return response;
    }

    if (!msg->body)
    {
        asprintf(&response, ":%s ERR_NO_NAME_FOUND\r\n", serv->hostname);
        return response;
    }

    assert(msg->params[0]);
    usr->username = strdup(msg->params[0]);
    usr->realname = strdup(msg->body);

    log_debug("user %d username=%s realname=%s", usr->fd, usr->username, usr->realname);

    if (usr->nick && usr->username && usr->realname)
    {
        log_debug("user%d registration complete", usr->fd);
    }

    asprintf(&response, ":%s OK\r\n", serv->hostname);
    return response;
}

char *Server_Privmsg_Command(Server *serv, User *usr, Message *msg);
