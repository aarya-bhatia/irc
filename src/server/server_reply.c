#include "include/K.h"
#include "include/channel.h"
#include "include/register.h"
#include "include/replies.h"
#include "include/server.h"
#include "include/types.h"
#include "include/user.h"

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
    _sanity_check(serv, usr, msg);

    assert(!strcmp(msg->command, "NICK"));

    if (msg->n_params != 1) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NEEDMOREPARAMS_MSG,
                                  serv->hostname, usr->nick,
                                  msg->command));
        return;
    }

    assert(msg->params[0]);

    char *new_nick = msg->params[0];

    if (!check_nick_available(serv, usr, new_nick)) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NICKNAMEINUSE_MSG,
                                  serv->hostname, msg->params[0]));
        return;
    }

    free(usr->nick);

    usr->nick = strdup(new_nick);
    usr->nick_changed = true;

    log_info("user %s updated nick", usr->nick);

    check_registration_complete(serv, usr);
    update_nick_map(serv, usr);
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
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "USER"));

    if (msg->n_params != 3 || !msg->body) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NEEDMOREPARAMS_MSG,
                                  serv->hostname, usr->nick,
                                  msg->command));
        return;
    }
    // User cannot set username/realname twice
    if (usr->username && usr->realname) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_ALREADYREGISTRED_MSG,
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

    log_debug("user %s set username to %s and realname to %s", usr->nick,
              username, realname);

    // To store the value of socket in hashmap
    int *userfd = malloc(sizeof *userfd);
    *userfd = usr->fd;

    // Register user's username to their socket in map
    cc_hashtable_add(serv->user_to_sock_map, usr->username, userfd);

    // Complete registration and prepare greeting messages
    check_registration_complete(serv, usr);
}

void Server_reply_to_MOTD(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "MOTD"));

    char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

    if (motd) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " RPL_MOTD_MSG, serv->hostname,
                                  usr->nick, motd));
    } else {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NOMOTD_MSG, serv->hostname,
                                  usr->nick));
    }
}

void Server_reply_to_PING(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
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
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "PRIVMSG"));

    if (!usr->registered) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NOTREGISTERED_MSG,
                                  serv->hostname, usr->nick));
        return;
    }

    assert(usr->username);

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
    char *target_username = NULL;

    CC_HashTableIter itr;  // iterate
    TableEntry *entry = NULL;
    cc_hashtable_iter_init(&itr, serv->user_to_nicks_map);

    // For each registered user, check if one of their nicks match the given nick
    while (!target_username && cc_hashtable_iter_next(&itr, &entry) != CC_ITER_END) {
        char *cur_username = entry->key;
        CC_Array *cur_nicks = entry->value;

        assert(cur_username);
        assert(cur_nicks);

        // Iterate the nicks array
        CC_ArrayIter nick_itr;
        cc_array_iter_init(&nick_itr, cur_nicks);
        char *cur_nick = NULL;

        while (cc_array_iter_next(&nick_itr, (void **)&cur_nick) !=
               CC_ITER_END) {
            assert(cur_nick);

            // match found
            if (!strcmp(cur_nick, target_nick)) {
                target_username = cur_username;
                break;
            }
        }
    }

    // no user found with a matching nick
    if (!target_username) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NOSUCHNICK_MSG,
                                  serv->hostname, usr->nick,
                                  target_nick));
        return;
    }
    // Look up the socket for target user through their username
    int *target_sock = NULL;

    cc_hashtable_get(serv->user_to_sock_map, target_username,
                     (void **)&target_sock);

    // user is offline
    if (!target_sock) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " RPL_AWAY_MSG, serv->hostname,
                                  usr->nick, target_nick));
        return;
    }

    assert(target_sock);

    // Look up the user data for the target user through their socket

    User *target_data = NULL;

    if (cc_hashtable_get(serv->connections, (void *)target_sock,
                         (void **)&target_data) == CC_OK) {
        assert(target_data);

        // Add message to target user's queue
        List_push_back(target_data->msg_queue,
                       make_reply(":%s!%s@%s PRIVMSG %s :%s", usr->nick,
                                  usr->username, usr->hostname,
                                  target_nick, msg->body));
        return;
    }

    log_info("PRIVMSG sent from user %s to user %s", usr->nick,
             target_nick);

    // TODO: Respond to current user for success
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
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "QUIT"));

    char *reason = (msg->body ? msg->body : "Client Quit");

    List_push_back(usr->msg_queue, make_reply(":%s "
                                              "ERROR :Closing Link: %s (%s)",
                                              serv->hostname, usr->hostname, reason));
}

void Server_reply_to_WHO(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_WHOIS(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
}

/**
 * Join a channel
 */
void Server_reply_to_JOIN(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
    assert(!strcmp(msg->command, "JOIN"));

    // JOIN is only for registered users
    if (!usr->registered) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NOTREGISTERED_MSG,
                                  serv->hostname, usr->nick));
        return;
    }

    assert(usr->username);

    // Param for channel name required
    if (msg->n_params == 0) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NEEDMOREPARAMS_MSG,
                                  serv->hostname, usr->nick,
                                  msg->command));
        return;
    }

    assert(msg->params[0]);

    // Channel should begin with #
    if (msg->params[0][0] != '#') {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NOSUCHCHANNEL_MSG,
                                  serv->hostname, usr->nick,
                                  msg->params[0]));
        return;
    }

    char *channel_name = msg->params[0] + 1;  // skip #

    Channel *channel = Server_get_channel(serv, channel_name);

    if (!channel) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_NOSUCHCHANNEL_MSG,
                                  serv->hostname, usr->nick,
                                  channel_name));
        return;
    }

    if (usr->n_memberships > MAX_CHANNEL_COUNT) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " ERR_TOOMANYCHANNELS_MSG,
                                  serv->hostname, usr->nick,
                                  channel_name));
        return;
    }

    Channel_add_member(channel, usr->username);
    usr->n_memberships++;

    // Send channel topic
    if (channel->topic) {
        List_push_back(usr->msg_queue,
                       make_reply(":%s " RPL_TOPIC_MSG, serv->hostname,
                                  usr->nick, channel_name,
                                  channel->topic));
    }

    // NAMES reply
    // Compose names as multipart message

    char *subject =
        make_string(":%s " RPL_NAMREPLY_MSG, serv->hostname, usr->nick, "=",
                    channel_name);

    char message[MAX_MSG_LEN + 1];
    memset(message, 0, sizeof message);

    strcat(message, subject);

    // Get channel members
    for (size_t i = 0; i < cc_array_size(channel->members); i++) {
        char *username = NULL;
        cc_array_get_at(channel->members, i, (void **)&username);
        assert(username);

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

    List_push_back(usr->msg_queue,
                   make_reply(":%s " RPL_ENDOFNAMES_MSG, serv->hostname,
                              usr->nick, channel_name));
}

void Server_reply_to_LIST(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_NAMES(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_SERVER(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_PASS(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
}

void Server_reply_to_CONNECT(Server *serv, User *usr, Message *msg) {
    _sanity_check(serv, usr, msg);
}