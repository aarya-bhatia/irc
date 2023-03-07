#include "include/channel.h"
#include "include/replies.h"
#include "include/server.h"
#include "include/types.h"
#include "include/user.h"

/**
 * To complete registration, the user must have a username, realname and a nick.
 */
bool check_registration_complete(Server *serv, User *usr) {
    if (!usr->registered && usr->nick_changed && usr->username && usr->realname) {
        usr->registered = true;

        ht_set(serv->username_to_user_map, usr->username, usr);
        ht_set(serv->online_nick_to_username_map, usr->nick, usr->username);

        // Send welcome messages

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

        log_info("registration completed for user %s", usr->nick);

        return true;
    }

    return false;
}

/**
 * RPL_NAMES as multipart message
 */
void RPL_NAMEREPLY(Server *serv, User *usr, Channel *channel) {
    char *subject = make_string(":%s " RPL_NAMREPLY_MSG, serv->hostname, usr->nick, "=",
                                channel->name);

    char message[MAX_MSG_LEN + 1];
    memset(message, 0, sizeof message);

    strcat(message, subject);

    // Get channel members
    for (size_t i = 0; i < Vector_size(channel->members); i++) {
        Membership *member = Vector_get_at(channel->members, i);
        assert(member);

        const char *username = member->username;
        size_t len = strlen(username) + 1;  // Length for name and space

        if (strlen(message) + len > MAX_MSG_LEN) {
            // End current message
            List_push_back(usr->msg_queue, make_reply("%s", message));

            // Start new message with subject
            memset(message, 0, sizeof message);
            strcat(message, subject);
        }

        // Append username and space to message
        strcat(message, username);
        strcat(message, " ");
    }

    List_push_back(usr->msg_queue, make_reply("%s", message));

    free(subject);

    List_push_back(usr->msg_queue, make_reply(":%s " RPL_ENDOFNAMES_MSG, serv->hostname,
                                              usr->nick, channel->name));
}

/**
 * The USER and NICK command should be the first messages sent by a new client to complete registration.
 *
 * Syntax: `NICK <nickname> [<hopcount>]`
 * Example: `NICK aarya\r\n`
 *
 * Replies:
 *
 * - RPL_WELCOME
 * - ERR_NONICKNAMEGIVEN
 * - ERR_NICKNAMEINUSE
 */
void Server_reply_to_NICK(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "NICK"));

    if (msg->n_params < 1) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NEEDMOREPARAMS_MSG,
                                  serv->hostname, usr->nick,
                                  msg->command));
        return;
    }

    assert(msg->params[0]);
    char *new_nick = msg->params[0];

    if (ht_contains(serv->online_nick_to_username_map, new_nick) ||
        ht_contains(serv->offline_nick_to_username_map, new_nick)) {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_NICKNAMEINUSE_MSG,
                                                  serv->hostname, msg->params[0]));
        return;
    }

    if (usr->registered) {
        if (!ht_remove(serv->online_nick_to_username_map, usr->nick, NULL, NULL)) {
            log_error("Failed to removed old nick %s from online_nick_to_username_map", usr->nick);
        }

        ht_set(serv->online_nick_to_username_map, new_nick, usr->username);
    }

    free(usr->nick);

    usr->nick = strdup(new_nick);
    usr->nick_changed = true;

    log_info("user %s updated nick", usr->nick);

    if (!usr->registered) {
        check_registration_complete(serv, usr);
    }
}

/**
 * The USER and NICK command should be the first messages sent by a new client to complete registration.
 * - Syntax: `USER <username> * * :<realname>`
 *
 * - Description: USER message is used at the beginning to specify the username and realname of new user.
 * - It is used in communication between servers to indicate new user
 * - A client will become registered after both USER and NICK have been received.
 * - Realname can contain spaces.
 */
void Server_reply_to_USER(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "USER"));

    if (msg->n_params < 3 || !msg->body) {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_NEEDMOREPARAMS_MSG,
                                                  serv->hostname, usr->nick,
                                                  msg->command));
        return;
    }

    // User cannot change username after registration
    if (usr->registered) {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_ALREADYREGISTRED_MSG,
                                                  serv->hostname, usr->nick));
        return;
    }

    assert(msg->params[0]);
    assert(msg->body);

    char *username = msg->params[0];
    char *realname = msg->body;

    // Set the username and realname
    usr->username = strdup(username);
    usr->realname = strdup(realname);

    log_debug("user %s set username to %s and realname to %s", usr->nick, username, realname);

    // Complete registration
    check_registration_complete(serv, usr);
}

void Server_reply_to_MOTD(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "MOTD"));

    char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

    if (motd) {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_MOTD_MSG, serv->hostname,
                                                  usr->nick, motd));
    } else {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_NOMOTD_MSG, serv->hostname,
                                                  usr->nick));
    }
}

void Server_reply_to_PING(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "PING"));
    List_push_back(usr->msg_queue, make_reply(":%s "
                                              "PONG %s",
                                              serv->hostname, serv->hostname));
}

/**
 * The PRIVMSG command is used to deliver a message to from one client to another within an IRC network.
 *
 * - Sytnax: `[:prefix] PRIVMSG <receiver> :<text>`
 * - Example: `PRIVMSG Aarya :hello, how are you?`
 *
 * Replies
 *
 * - RPL_AWAY
 * - ERR_NORECIPEINT
 * - ERR_NOSUCHNICK
 * - ERR_TOOMANYTARGETS
 */
void Server_reply_to_PRIVMSG(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "PRIVMSG"));

    if (!Server_registered_middleware(serv, usr, msg)) {
        return;
    }

    if (msg->n_params == 0) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NORECIPIENT_MSG,
                                  serv->hostname, usr->nick));
        return;
    }

    assert(msg->params[0]);

    if (msg->n_params > 1) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_TOOMANYTARGETS_MSG,
                                  serv->hostname, usr->nick));
        return;
    }
    // The nick to send message to
    const char *target_nick = msg->params[0];

    // Message target is channel
    if (target_nick[0] == '#') {
        
    } else {
        // Message target is user
        if (ht_contains(serv->offline_nick_to_username_map, target_nick)) {
            List_push_back(usr->msg_queue,
                           make_reply(":%s " RPL_AWAY_MSG, serv->hostname,
                                      usr->nick, target_nick));
            return;
        }

        char *target_username = ht_get(serv->online_nick_to_username_map, target_nick);

        if (!target_username) {
            List_push_back(usr->msg_queue,
                           make_reply(":%s " ERR_NOSUCHNICK_MSG,
                                      serv->hostname, usr->nick,
                                      target_nick));
            return;
        }

        User *target_data = ht_get(serv->username_to_user_map, target_username);

        if (!target_data) {
            List_push_back(usr->msg_queue,
                           make_reply(":%s " ERR_NOSUCHNICK_MSG,
                                      serv->hostname, usr->nick,
                                      target_nick));
            return;
        }

        // Add message to target user's queue
        List_push_back(target_data->msg_queue,
                       make_reply(":%s!%s@%s PRIVMSG %s :%s", usr->nick,
                                  usr->username, usr->hostname,
                                  target_nick, msg->body));

        log_info("PRIVMSG sent from user %s to user %s", usr->nick,
                 target_nick);
    }
}

/**
 * The QUIT command should be the final message sent by client to close the connection.
 *
 * - Syntax: `QUIT :<message>`
 * - Example: `QUIT :Gone to have lunch.`
 *
 * Replies:
 *
 * - ERROR
 */
void Server_reply_to_QUIT(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "QUIT"));

    char *reason = (msg->body ? msg->body : "Client Quit");

    List_push_back(usr->msg_queue, make_reply(":%s "
                                              "ERROR :Closing Link: %s (%s)",
                                              serv->hostname, usr->hostname, reason));
    assert(List_size(usr->msg_queue) > 0);
    usr->quit = true;
}

void Server_reply_to_WHO(Server *serv, User *usr, Message *msg) {
}

void Server_reply_to_WHOIS(Server *serv, User *usr, Message *msg) {
}

/**
 * Join a channel
 */
void Server_reply_to_JOIN(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "JOIN"));

    if (!Server_registered_middleware(serv, usr, msg)) {
        return;
    }

    if (!Server_channel_middleware(serv, usr, msg)) {
        return;
    }

    char *channel_name = msg->params[0] + 1;  // skip #
    Channel *channel = ht_get(serv->channels_map, channel_name);
    assert(channel);

    // Add user to channel
    Channel_add_member(channel, usr->username);
    Vector_push(usr->channels, channel);

    // Broadcast JOIN to every client including current user
    char *join_message = make_reply("%s!%s@%s JOIN #%s", usr->nick, usr->username, usr->hostname, channel_name);
    Server_broadcast_message(serv, join_message);
    free(join_message);

    // Send channel topic
    if (channel->topic) {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_TOPIC_MSG, serv->hostname, usr->nick, channel_name, channel->topic));
    }

    // Send NAMES reply
    RPL_NAMEREPLY(serv, usr, channel);
}

void Server_reply_to_LIST(Server *serv, User *usr, Message *msg) {
}

void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "NAMES"));

    if (!Server_registered_middleware(serv, usr, msg)) {
        return;
    }

    if (!Server_channel_middleware(serv, usr, msg)) {
        return;
    }

    Channel *channel = ht_get(serv->channels_map, msg->params[0] + 1);
    assert(channel);

    RPL_NAMEREPLY(serv, usr, channel);
}

void Server_reply_to_TOPIC(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "TOPIC"));

    if (!Server_registered_middleware(serv, usr, msg)) {
        return;
    }

    if (!Server_channel_middleware(serv, usr, msg)) {
        return;
    }

    char *channel_name = msg->params[0] + 1;
    Channel *channel = ht_get(serv->channels_map, channel_name);
    assert(channel);

    // update or view topic

    if (msg->body) {
        channel->topic = strdup(msg->body);
        log_info("user %s set topic for channel %s", usr->nick, channel->name);
    } else if (channel->topic) {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_TOPIC_MSG, serv->hostname, usr->nick, channel_name, channel->topic));
    } else {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_NOTOPIC_MSG, serv->hostname, usr->nick, channel_name));
    }
}

void Server_reply_to_PART(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "PART"));

    if (!Server_registered_middleware(serv, usr, msg)) {
        return;
    }

    if (!Server_channel_middleware(serv, usr, msg)) {
        return;
    }

    char *channel_name = msg->params[0] + 1;
    Channel *channel = ht_get(serv->channels_map, channel_name);

    if (!Channel_has_member(channel, usr->username)) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NOTONCHANNEL_MSG, serv->hostname,
                                  usr->nick, channel->name));
        return;
    }

    char *reason = msg->body ? strdup(msg->body) : make_string("%s is leaving channel %s", usr->nick, channel->name);
    char *broadcast_message = make_reply("%s!%s@%s PART #%s :%s",
                                         usr->nick, usr->username, usr->hostname, channel->name, reason);

    // broadcast message to users on channel
    for (size_t i = 0; i < Vector_size(channel->members); i++) {
        Membership *member = Vector_get_at(channel->members, i);
        assert(member);
        User *member_user = ht_get(serv->online_nick_to_username_map, member->username);
        if (member_user) {
            List_push_back(member_user->msg_queue, strdup(broadcast_message));
        }
    }

    Channel_remove_member(channel, usr->username);
    log_info("user %s has left channel %s", usr->nick, channel->name);

    free(broadcast_message);
    free(reason);
}

void Server_reply_to_SERVER(Server *serv, User *usr, Message *msg) {
}

void Server_reply_to_PASS(Server *serv, User *usr, Message *msg) {
}

void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg) {
}