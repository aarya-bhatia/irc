#include "replies.h"

char *Reply_Welcome(Server *serv, User *usr)
{
    return make_string(RPL_WELCOME, serv->hostname, usr->nick, usr->nick);
}

char *Reply_Your_Host(Server *serv, User *usr)
{
    return make_string(RPL_YOURHOST, serv->hostname, usr->nick, usr->hostname);
}

char *Reply_Created(Server *serv, User *usr)
{
    return make_string(RPL_CREATED, serv->hostname, usr->nick, serv->created_at);
}

char *Reply_My_Info(Server *serv, User *usr)
{
    return make_string(RPL_MYINFO, serv->hostname, usr->nick, serv->hostname);
}


