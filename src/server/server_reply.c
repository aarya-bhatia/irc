#include "include/replies.h"
#include "include/server.h"

bool Server_registered_middleware(Server *serv, User *usr, Message *msg) {
    assert(serv);
    assert(usr);
    assert(msg);

    if (!usr->registered) {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_NOTREGISTERED_MSG, serv->hostname, usr->nick)); return false;
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

void send_who_reply(Server *serv, User *usr, Channel *target_channel, User *target_usr) {
    // RPL_WHOREPLY: "<client> <channel> <username> <host> <server> <nick> <flags> :<hopcount> <realname>"
    char *flags = ht_contains(serv->online_nick_to_username_map, target_usr->nick) ? "H" : "G";
    List_push_back(usr->msg_queue, make_reply(":%s " RPL_WHOREPLY_MSG,
                                              serv->hostname,
                                              usr->nick,
                                              target_channel->name,
                                              target_usr->username,
                                              target_usr->hostname,
                                              serv->hostname,
                                              target_usr->nick,
                                              flags,
                                              0,
                                              target_usr->realname));
}

void send_motd_reply(Server *serv, User *usr) {
    char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

    if (motd) {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_MOTD_MSG,
                                                  serv->hostname, usr->nick,
                                                  motd));
    } else {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_NOMOTD_MSG,
                                                  serv->hostname, usr->nick));
    }
}

void send_welcome_reply(Server *serv, User *usr) {
    List_push_back(usr->msg_queue, make_reply(":%s " RPL_WELCOME_MSG, serv->hostname,
                                              usr->nick, usr->nick));
    List_push_back(usr->msg_queue, make_reply(":%s " RPL_YOURHOST_MSG, serv->hostname,
                                              usr->nick, usr->hostname));
    List_push_back(usr->msg_queue, make_reply(":%s " RPL_CREATED_MSG, serv->hostname,
                                              usr->nick, serv->created_at));
    List_push_back(usr->msg_queue, make_reply(":%s " RPL_MYINFO_MSG, serv->hostname,
                                              usr->nick, serv->hostname, "*", "*",
                                              "*"));
}

void send_topic_reply(Server *serv, User *usr, Channel *channel) {
    if (channel->topic) {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_TOPIC_MSG, serv->hostname, usr->nick, channel->name, channel->topic));
    } else {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_NOTOPIC_MSG, serv->hostname, usr->nick, channel->name));
    }
}

/**
 * To complete registration, the user must have a username, realname and a nick.
 */
bool check_registration_complete(Server *serv, User *usr) {
    if (!usr->registered && usr->nick_changed && usr->username && usr->realname) {
        usr->registered = true;

        ht_set(serv->username_to_user_map, usr->username, usr);
        ht_set(serv->online_nick_to_username_map, usr->nick, usr->username);

        send_welcome_reply(serv, usr);
        send_motd_reply(serv, usr);

        log_info("registration completed for user %s", usr->nick);

        return true;
    }

    return false;
}

/**
 * RPL_NAMES as multipart message
 */
void send_names_reply(Server *serv, User *usr, Channel *channel) {
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

    List_push_back(usr->msg_queue, make_reply(":%s " RPL_ENDOFNAMES_MSG, serv->hostname, usr->nick, channel->name));
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
        
        free(motd);
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
        char *channel_name = target_nick + 1;
        Channel *channel = ht_get(serv->channels_map, channel_name);
        if (!channel) {
            List_push_back(usr->msg_queue, make_reply(":%s " ERR_NOSUCHCHANNEL_MSG, serv->hostname, usr->nick, channel_name));
            return;
        }

        if (!Channel_has_member(channel, usr->username)) {
            List_push_back(usr->msg_queue, make_reply(":%s " ERR_CANNOTSENDTOCHAN_MSG, serv->hostname, usr->nick, channel_name));
            return;
        }

        char *message = make_reply("%s!%s@%s PRIVMSG #%s :%s", usr->nick, usr->username, usr->hostname, channel->name, msg->body);
        Server_broadcast_to_channel(serv, channel, message);
        free(message);

        log_debug("user %s sent message to channel %s", usr->nick, channel->name);

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

        log_info("PRIVMSG sent from user %s to user %s", usr->nick, target_nick);
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

/**
 * This command is used to query a list of users who match the provided mask.
 * The server will answer this command with zero, one or more RPL_WHOREPLY, and end the list with RPL_ENDOFWHO.
 */
void Server_reply_to_WHO(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "WHO"));

    if (msg->n_params == 0) {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_ENDOFWHO_MSG, serv->hostname, usr->nick, ""));
        return;
    }

    char *mask = msg->params[0];
    assert(mask);

    if (mask[0] == '#') {
        if (ht_contains(serv->channels_map, mask + 1)) {
            // Return who reply for each user in channel
            Channel *channel = ht_get(serv->channels_map, mask + 1);

            for (size_t i = 0; i < Vector_size(channel->members); i++) {
                Membership *member = Vector_get_at(channel->members, i);
                User *other_user = Server_get_user_by_username(serv, member->username);
                send_who_reply(serv, usr, channel, other_user);
            }
        }
    } else {
        User *other_user = Server_get_user_by_nick(serv, mask);

        // Return who reply for channel given user is member of
        if (other_user) {
            for (size_t i = 0; i < Vector_size(other_user->channels); i++) {
                Channel *channel = ht_get(serv->channels_map, Vector_get_at(other_user->channels, i));
                if (channel) {
                    send_who_reply(serv, usr, channel, other_user);
                }
            }
        }
    }

    List_push_back(usr->msg_queue, make_reply(":%s " RPL_ENDOFWHO_MSG, serv->hostname, usr->nick, ""));
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

    char *channel_name = msg->params[0] + 1;  // skip #

    if (Vector_size(usr->channels) > MAX_CHANNEL_COUNT) {
        List_push_back(usr->msg_queue, make_reply(":%s " ERR_TOOMANYCHANNELS_MSG, serv->hostname, usr->nick, channel_name));
        return;
    }

    Channel *channel = ht_get(serv->channels_map, channel_name);

    if (!channel) {
        // Create channel
        channel = Channel_alloc(channel_name);
        ht_set(serv->channels_map, channel_name, channel);
        log_info("New channel %s created by user %s", channel_name, usr->nick);
    }

    // Add user to channel
    Channel_add_member(channel, usr->username);
    User_add_channel(usr, channel->name);

    // Broadcast JOIN to every client including current user
    char *join_message = make_reply("%s!%s@%s JOIN #%s", usr->nick, usr->username, usr->hostname, channel_name);
    Server_broadcast_message(serv, join_message);
    free(join_message);

    // Send channel topic
    if (channel->topic) {
        List_push_back(usr->msg_queue, make_reply(":%s " RPL_TOPIC_MSG, serv->hostname, usr->nick, channel_name, channel->topic));
    }

    // Send NAMES reply
    send_names_reply(serv, usr, channel);
}

void Server_reply_to_LIST(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "LIST"));

    // Reply start
    List_push_back(usr->msg_queue, make_reply(":%s " RPL_LISTSTART_MSG, serv->hostname, usr->nick));

    // List all channels
    if (msg->n_params == 0) {
        HashtableIter itr;
        ht_iter_init(&itr, serv->channels_map);
        Channel *channel = NULL;
        while (ht_iter_next(&itr, NULL, (void **)&channel)) {
            assert(channel);
            List_push_back(usr->msg_queue, make_reply(":%s " RPL_LIST_MSG, serv->hostname, usr->nick, channel->name, Vector_size(channel->members), channel->topic));
        }
    } else {  // List specified channels
        char *targets = msg->params[0];
        assert(targets);
        // get channel names separated by commas
        char *tok = strtok(targets, ",");
        while (tok) {
            if (tok[0] != '#') {
                continue;
            }
            char *target = tok + 1;
            if (ht_contains(serv->channels_map, target)) {
                Channel *channel = ht_get(serv->channels_map, target);
                assert(channel);
                List_push_back(usr->msg_queue, make_reply(":%s " RPL_LIST_MSG, serv->hostname, usr->nick, channel->name, Vector_size(channel->members), channel->topic));
            }
            tok = strtok(NULL, ",");
        }
    }

    // Reply end
    List_push_back(usr->msg_queue, make_reply(":%s " RPL_LISTEND_MSG, serv->hostname, usr->nick));
}

void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "NAMES"));

    if (!Server_registered_middleware(serv, usr, msg)) {
        return;
    }

    // send NAME reply for all channels on server
    if (msg->n_params == 0) {
        HashtableIter itr;
        ht_iter_init(&itr, serv->channels_map);
        Channel *channel = NULL;
        while (ht_iter_next(&itr, NULL, (void **)&channel)) {
            send_names_reply(serv, usr, channel);
        }

        return;
    }

    assert(msg->params[0]);
    char *targets = msg->params[0];

    // send NAME reply for each channel in comma separated list
    for (char *tok = strtok(targets, ","); tok != NULL; tok = strtok(NULL, ",")) {
        if (tok[0] != '#') {
            log_debug("no such channel");
            continue;
        }

        Channel *channel = ht_get(serv->channels_map, tok + 1);

        if (!channel) {
            log_debug("channel %s not found", tok);
            continue;
        }

        send_names_reply(serv, usr, channel);
    }
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
    } else {
        send_topic_reply(serv, usr, channel);
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

    Server_broadcast_to_channel(serv, channel, broadcast_message);
    Channel_remove_member(channel, usr->username);  // Remove user from channel's list

    // remove channel from user's personal list
    User_remove_channel(usr, channel->name);

    free(broadcast_message);
    free(reason);

    log_info("user %s has left channel %s", usr->nick, channel->name);

    if (Vector_size(channel->members) == 0) {
        log_info("removing channel %s from server", channel->name);
        ht_remove(serv->channels_map, channel->name, NULL, NULL);
    }
}

void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg) {
}

/**
 * Returns statistics about local and global users, as numeric replies.
 *
 * TODO
 */
void Server_reply_to_LUSERS(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "LUSERS"));

    List_push_back(usr->msg_queue, make_reply(":%s " RPL_LUSERCLIENT_MSG, serv->hostname,
                                              usr->nick, ht_size(serv->username_to_user_map), 0, 1));

    List_push_back(usr->msg_queue, make_reply(":%s " RPL_LUSEROP_MSG, serv->hostname,
                                              usr->nick, 0));

    List_push_back(usr->msg_queue, make_reply(":%s " RPL_LUSERUNKNOWN_MSG, serv->hostname,
                                              usr->nick, 0));

    List_push_back(usr->msg_queue, make_reply(":%s " RPL_LUSERCHANNELS_MSG, serv->hostname,
                                              usr->nick, ht_size(serv->channels_map)));

    List_push_back(usr->msg_queue, make_reply(":%s " RPL_LUSERME_MSG, serv->hostname,
                                              usr->nick, ht_size(serv->username_to_user_map), 0, 0));
}

/**
 * The HELP command is used to return documentation about the IRC server and the IRC commands it implements.
 */
void Server_reply_to_HELP(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "HELP"));

    const struct help_t *help = NULL;
    const char *subject = NULL;

    if (msg->n_params == 0 && !msg->body) {
        subject = "*";
        help = get_help_text("HELP");
    } else {
        subject = msg->n_params > 0 ? msg->params[0] : msg->body;
        help = get_help_text(subject);
    }

    if (help) {
        List_push_back(usr->msg_queue, make_reply(":%s 704 %s %s :%s", serv->hostname, usr->nick, subject, help->title));
        List_push_back(usr->msg_queue, make_reply(":%s 705 %s %s :", serv->hostname, usr->nick, subject));

        // Send help text as multipart messages and break long lines into multiple messages.
        Vector *lines = text_wrap(help->body, 200);

        for (size_t i = 0; i < Vector_size(lines); i++) {
            List_push_back(usr->msg_queue, make_reply(":%s 705 %s %s :%s", serv->hostname, usr->nick, subject, Vector_get_at(lines, i)));
        }

        Vector_free(lines);

        // List_push_back(usr->msg_queue, make_reply(":%s 705 %s %s :", serv->hostname, usr->nick, subject));
        List_push_back(usr->msg_queue, make_reply(":%s 706 %s %s :End of help", serv->hostname, usr->nick, subject));
        return;
    }

    List_push_back(usr->msg_queue, make_reply(":%s 524 %s %s :No help available on this topic", serv->hostname, usr->nick, subject));
}

/**
 * Command: NOTICE
 * Parameters: <target>{,<target>} <text to be sent>
 *
 * The NOTICE command is used to send notices between users, as well as to send notices to channels.
 * <target> is interpreted the same way as it is for the PRIVMSG command.
 * The difference between NOTICE and PRIVMSG is that automatic replies must never be sent in response to a NOTICE message.
 */
void Server_reply_to_NOTICE(Server *serv, User *usr, Message *msg) {
    assert(!strcmp(msg->command, "NOTICE"));

    if (msg->n_params == 0) {
        return;
    }

    char *targets = msg->params[0];

    if (!targets) {
        return;
    }

    char *save = NULL;
    char *target = strtok_r(targets, ",", &save);

    while (target) {
        if (target[0] == '#') {
            Channel *channel = ht_get(serv->channels_map, target + 1);
            if (!channel) {
                return;
            }

            Server_broadcast_to_channel(serv, channel, msg->body);

        } else {
            User *other_user = Server_get_user_by_nick(serv, target);
            if (!other_user) {
                return;
            }

            List_push_back(other_user->msg_queue, make_reply("%s!%s@%s NOTICE %s :%s",
                                                             usr->nick, usr->username, usr->hostname, target, msg->body));
        }
        target = strtok_r(NULL, ",", &save);
    }
}

void Server_reply_to_SERVER(Server *serv, Peer *peer, Message *msg) {
}

void Server_reply_to_PASS(Server *serv, Peer *peer, Message *msg) {
}
