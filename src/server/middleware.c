#include "include/channel.h"
#include "include/replies.h"
#include "include/server.h"
#include "include/types.h"
#include "include/user.h"

bool Server_registered_middleware(Server *serv, User *usr, Message *msg) {
    assert(serv);
    assert(usr);
    assert(msg);

    if (!usr->registered) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NOTREGISTERED_MSG,
                                  serv->hostname, usr->nick));
        return false;
    }

    return true;
}

/**
 * Validate channel request and return status.
 * Returns true if user is able to proceed or false if request failed.
 */
bool Server_channel_middleware(Server *serv, User *usr, Message *msg) {
    assert(serv);
    assert(usr);
    assert(msg);
    assert(msg->command);
    assert(usr->registered);
    assert(usr->username);

    // Param for channel name required
    if (msg->n_params == 0) {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_NEEDMOREPARAMS_MSG, serv->hostname, usr->nick, msg->command));
        return false;
    }

    assert(msg->params[0]);

    // check if channel exists
    Channel *channel = NULL;

    if (*msg->params[0] != '#' || (channel = ht_get(serv->channels_map, msg->params[0] + 1)) == NULL) {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_NOSUCHCHANNEL_MSG, serv->hostname, usr->nick, msg->params[0]));
        return false;
    }

    return true;
}
