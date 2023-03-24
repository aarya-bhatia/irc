#include "include/list.h"
#include "include/replies.h"
#include "include/server.h"

void Server_handle_PING(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "PING"));
	List_push_back(usr->msg_queue, Server_create_message(serv, "PONG %s", serv->hostname));
}

bool check_registered(Server *serv, User *usr)
{
	if (!usr->registered)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOTREGISTERED_MSG, usr->nick));
		return false;
	}

	return true;
}

/**
 * <client> <channel> <username> <host> <server> <nick> <flags> :<hopcount>
 * <realname>
 */
void send_who_reply(Server *serv, User *usr, Channel *target_channel, User *target_usr)
{
	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_WHOREPLY_MSG, usr->nick, target_channel->name, target_usr->username, target_usr->hostname, serv->hostname, target_usr->nick, "H", 0, target_usr->realname));
}

void send_motd_reply(Server *serv, User *usr)
{
	char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

	if (motd)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, RPL_MOTD_MSG, usr->nick, motd));
	}
	else
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOMOTD_MSG, usr->nick));
	}

	free(motd);
}

void send_welcome_reply(Server *serv, User *usr)
{
	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_WELCOME_MSG, usr->nick, usr->nick));
	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_YOURHOST_MSG, usr->nick, usr->hostname));
	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_CREATED_MSG, usr->nick, serv->created_at));
	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_MYINFO_MSG, usr->nick, serv->hostname, "*", "*", "*"));
}

void send_topic_reply(Server *serv, User *usr, Channel *channel)
{
	if (channel->topic)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, RPL_TOPIC_MSG, usr->nick, channel->name, channel->topic));
	}
	else
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, RPL_NOTOPIC_MSG, usr->nick, channel->name));
	}
}

/**
 * To complete registration, the user must have a username, realname and a nick.
 */
bool check_user_registration(Server *serv, User *usr)
{
	if (!usr->registered && usr->nick_changed && usr->username && usr->realname)
	{
		usr->registered = true;

		ht_set(serv->nick_to_user_map, usr->nick, usr);
		ht_set(serv->nick_to_serv_name_map, usr->nick, serv->name);

		send_welcome_reply(serv, usr);
		send_motd_reply(serv, usr);
		log_info("registration completed for user %s", usr->nick);

		// notify peers about new user
		// <nickname> <hopcount> <username> <host> <servertoken> <umode> <realname>
		char *message = Server_create_message(serv, "NICK %s 1 %s %s 1 + :%s", usr->nick, usr->username, usr->hostname, usr->realname);
		Server_broadcast_message(serv, message);
		free(message);
		return true;
	}

	return false;
}

/**
 * Send RPL_NAMES as multipart message
 */
void send_names_reply(Server *serv, User *usr, Channel *channel)
{
	char *subject = make_string(":%s " RPL_NAMREPLY_MSG, serv->name, usr->nick, "=", channel->name);
	char message[MAX_MSG_LEN + 1];
	memset(message, 0, sizeof message);
	strcat(message, subject);

	// Get channel members
	HashtableIter itr;
	ht_iter_init(&itr, channel->members);
	User *member = NULL;
	while (ht_iter_next(&itr, NULL, (void **)&member))
	{
		const char *username = member->username;
		size_t len = strlen(username) + 1; // Length for name and space

		if (strlen(message) + len > MAX_MSG_LEN)
		{
			// End current message
			List_push_back(usr->msg_queue, make_string("%s\r\n", message));

			// Start new message with subject
			memset(message, 0, sizeof message);
			strcat(message, subject);
		}
		// Append username and space to message
		strcat(message, username);
		strcat(message, " ");
	}

	List_push_back(usr->msg_queue, make_string("%s\r\n", message));
	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_ENDOFNAMES_MSG, usr->nick, channel->name));
	free(subject);
}

/**
 * The USER and NICK command should be the first messages sent by a new client
 * to complete registration.
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
void Server_handle_NICK(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "NICK"));

	if (msg->n_params < 1)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
		return;
	}

	assert(msg->params[0]);
	char *new_nick = msg->params[0];

	if (ht_contains(serv->nick_to_user_map, new_nick))
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NICKNAMEINUSE_MSG, msg->params[0]));
		return;
	}

	if (usr->registered)
	{
		ht_remove(serv->nick_to_user_map, usr->nick, NULL, NULL);
		ht_remove(serv->nick_to_serv_name_map, usr->nick, NULL, NULL);
		ht_set(serv->nick_to_user_map, new_nick, usr);
		ht_set(serv->nick_to_serv_name_map, new_nick, serv->name);
	}

	free(usr->nick);

	usr->nick = strdup(new_nick);
	usr->nick_changed = true;

	log_info("user %s updated nick", usr->nick);

	if (!usr->registered)
	{
		check_user_registration(serv, usr);
	}
	else
	{
		char *message = Server_create_message(serv, "NICK %s 1 %s %s 1 + :%s", usr->nick, usr->username, usr->hostname, usr->realname);
		Server_broadcast_message(serv, message);
		free(message);
	}
}

/**
 * The USER and NICK command should be the first messages sent by a new client to complete registration.
 * - Syntax: `USER <username> * * :<realname>`
 * - Description: USER message is used at the beginning to specify the username and realname of new user.
 * - A client will become registered after both USER and NICK have been received.
 */
void Server_handle_USER(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "USER"));

	if (msg->n_params < 3 || !msg->body)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
		return;
	}

	if (usr->registered)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_ALREADYREGISTRED_MSG, usr->nick));
		return;
	}

	assert(msg->params[0]);
	assert(msg->body);

	char *username = msg->params[0];
	char *realname = msg->body;

	usr->username = strdup(username);
	usr->realname = strdup(realname);

	log_debug("user %s set username to %s and realname to %s", usr->nick, username, realname);
	check_user_registration(serv, usr);
}

void Server_handle_MOTD(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "MOTD"));
	List_push_back(usr->msg_queue, Server_create_message(serv, "375 %s :- %s Message of the day - ", usr->nick, serv->name));

	char *motd = serv->motd_file ? get_motd(serv->motd_file) : NULL;

	if (motd)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, RPL_MOTD_MSG, usr->nick, motd));
		free(motd);
	}
	else
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOMOTD_MSG, usr->nick));
	}
}

/**
 * The PRIVMSG command is used to deliver a message from one client to
 * another client or channel in the IRC network.
 * - Sytnax: `[:prefix] PRIVMSG <receiver> :<text>`
 * - Example: `PRIVMSG Aarya :hello`
 * - Example: `PRIVMSG #somechannel :hello`
 */
void Server_handle_PRIVMSG(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "PRIVMSG"));

	if (!check_registered(serv, usr))
	{
		return;
	}

	if (msg->n_params == 0)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NORECIPIENT_MSG, usr->nick));
		return;
	}

	const char *target = msg->params[0];
	assert(target);

	char *message = User_create_message(usr, "%s", msg->message);

	if (target[0] == '#')
	{
		Channel *channel = ht_get(serv->name_to_channel_map, target + 1);

		if (!channel)
		{
			List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOSUCHCHANNEL_MSG, usr->nick, target));
			return;
		}

		if (!Channel_has_member(channel, usr))
		{
			List_push_back(usr->msg_queue, Server_create_message(serv, ERR_CANNOTSENDTOCHAN_MSG, usr->nick, target));
			return;
		}

		Server_message_channel(serv, serv->name, target + 1, message);
	}
	else
	{
		if (!ht_get(serv->nick_to_serv_name_map, target))
		{
			List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOSUCHNICK_MSG, usr->nick, target));
			return;
		}

		Server_message_user(serv, serv->name, target, message);
	}

	free(message);
}

/**
 * The QUIT command should be the final message sent by client to close the
 * connection.
 *
 * - Syntax: `QUIT :<message>`
 * - Example: `QUIT :Gone to have lunch.`
 */
void Server_handle_QUIT(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "QUIT"));
	char *reason = (msg->body ? msg->body : "Client Quit");
	List_push_back(usr->msg_queue, Server_create_message(serv, "ERROR :Closing Link: %s (%s)", usr->hostname, reason));
	usr->quit = true;
}

/**
 * This command is used to query a list of users who match the provided mask.
 * The server will answer this command with zero, one or more RPL_WHOREPLY, and
 * end the list with RPL_ENDOFWHO.
 */
void Server_handle_WHO(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "WHO"));

	if (msg->n_params == 0)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, RPL_ENDOFWHO_MSG, usr->nick, ""));
		return;
	}

	char *mask = msg->params[0];
	assert(mask);
	char *tok = strtok(mask, ",");

	while (tok)
	{
		if (mask[0] == '#')
		{
			if (ht_contains(serv->name_to_channel_map, mask + 1))
			{
				// Return who reply for each user in channel
				Channel *channel = ht_get(serv->name_to_channel_map, mask + 1);
				HashtableIter itr;
				ht_iter_init(&itr, channel->members);
				User *member = NULL;
				while (ht_iter_next(&itr, NULL, (void **)&member))
				{
					send_who_reply(serv, usr, channel, member);
				}
			}
		}
		else
		{
			User *other_user = ht_get(serv->nick_to_user_map, mask);

			// Return who reply for channel given user is member of
			if (other_user)
			{
				for (size_t i = 0; i < Vector_size(other_user->channels); i++)
				{
					Channel *channel = ht_get(serv->name_to_channel_map, Vector_get_at(other_user->channels, i));
					if (channel)
					{
						send_who_reply(serv, usr, channel, other_user);
					}
				}
			}
		}
		tok = strtok(NULL, ",");
	}

	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_ENDOFWHO_MSG, usr->nick, ""));
}

/**
 * Join a channel
 *
 * TODO: check user permissions
 *
 */
void Server_handle_JOIN(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "JOIN"));

	if (!check_registered(serv, usr))
	{
		return;
	}

	char *channel_name = msg->params[0] + 1; // skip #

	if (Vector_size(usr->channels) > MAX_CHANNEL_COUNT)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_TOOMANYCHANNELS_MSG, usr->nick, channel_name));
		return;
	}

	Channel *channel = ht_get(serv->name_to_channel_map, channel_name);

	if (!channel)
	{
		// Create channel
		channel = Channel_alloc(channel_name);
		ht_set(serv->name_to_channel_map, channel_name, channel);
		log_info("New channel %s created by user %s", channel_name, usr->nick);
	}

	// Add user to channel
	Channel_add_member(channel, usr);
	User_add_channel(usr, channel->name);

	// Broadcast JOIN to every client on channel
	char *join_message = User_create_message(usr, "%s", msg->message);
	Server_message_channel(serv, serv->name, channel_name, join_message);
	free(join_message);

	send_topic_reply(serv, usr, channel);
	send_names_reply(serv, usr, channel);
}

/**
 * The LIST command is used to get a list of channels along with some
 * information about each channel. Both parameters to this command are optional
 * as they have different syntaxes.
 *
 */
void Server_handle_LIST(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "LIST"));

	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_LISTSTART_MSG, usr->nick));

	if (msg->n_params == 0) // List all channels
	{
		HashtableIter itr;
		ht_iter_init(&itr, serv->name_to_channel_map);
		Channel *channel = NULL;
		while (ht_iter_next(&itr, NULL, (void **)&channel))
		{
			assert(channel);
			List_push_back(usr->msg_queue, Server_create_message(serv, RPL_LIST_MSG, usr->nick, channel->name, ht_size(channel->members), channel->topic));
		}
	}
	else // List specified channels
	{
		char *targets = msg->params[0];
		assert(targets);
		// get channel names separated by commas
		char *tok = strtok(targets, ",");
		while (tok)
		{
			if (tok[0] != '#')
			{
				continue;
			}

			char *target = tok + 1;

			if (ht_contains(serv->name_to_channel_map, target))
			{
				Channel *channel = ht_get(serv->name_to_channel_map, target);
				List_push_back(usr->msg_queue, Server_create_message(serv, RPL_LIST_MSG, usr->nick, channel->name, ht_size(channel->members), channel->topic));
			}

			tok = strtok(NULL, ",");
		}
	}

	List_push_back(usr->msg_queue, Server_create_message(serv, RPL_LISTEND_MSG, usr->nick));
}

/**
 * The NAMES command is used to view the nicknames joined to a channel and
 * their channel membership prefixes. The param of this command is a list of
 * channel names, delimited by a comma.
 */
void Server_handle_NAMES(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "NAMES"));

	if (!check_registered(serv, usr))
	{
		return;
	}

	// send reply for all channels on server
	if (msg->n_params == 0)
	{
		HashtableIter itr;
		ht_iter_init(&itr, serv->name_to_channel_map);
		Channel *channel = NULL;
		while (ht_iter_next(&itr, NULL, (void **)&channel))
		{
			send_names_reply(serv, usr, channel);
		}

		return;
	}

	char *targets = msg->params[0]; // comma separated list
	assert(targets);

	// send reply for each channel
	for (char *tok = strtok(targets, ","); tok != NULL; tok = strtok(NULL, ","))
	{
		if (tok[0] != '#')
		{
			continue;
		}

		Channel *channel = ht_get(serv->name_to_channel_map, tok + 1);

		if (!channel)
		{
			log_debug("channel %s not found", tok);
			continue;
		}

		send_names_reply(serv, usr, channel);
	}
}

/**
 * update or view channel topic
 */
void Server_handle_TOPIC(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "TOPIC"));

	if (!check_registered(serv, usr))
	{
		return;
	}

	if (msg->n_params == 0)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
		return;
	}

	assert(msg->params[0]);

	// check if channel exists
	Channel *channel = NULL;
	char *channel_name = msg->params[0] + 1;

	if (*msg->params[0] != '#' || !(channel = ht_get(serv->name_to_channel_map, channel_name)))
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOSUCHCHANNEL_MSG, usr->nick, msg->params[0]));
		return;
	}

	assert(channel);

	if (msg->body)
	{
		channel->topic = strdup(msg->body);
		log_info("user %s set topic for channel %s", usr->nick, channel->name);
	}
	else
	{
		send_topic_reply(serv, usr, channel);
	}
}

/**
 * To leave channel
 */
void Server_handle_PART(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "PART"));

	if (!check_registered(serv, usr))
	{
		return;
	}

	if (msg->n_params == 0)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
		return;
	}

	// check if channel exists
	Channel *channel = NULL;
	char *channel_name = msg->params[0] + 1;

	if (*msg->params[0] != '#' || !(channel = ht_get(serv->name_to_channel_map, channel_name)))
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOSUCHCHANNEL_MSG, usr->nick, msg->params[0]));
		return;
	}

	if (!Channel_has_member(channel, usr))
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOTONCHANNEL_MSG, usr->nick, channel->name));
		return;
	}

	char *reason = msg->body ? strdup(msg->body)
							 : make_string("%s is leaving channel %s",
										   usr->nick, channel->name);

	char *broadcast_message = User_create_message(usr, "PART #%s :%s", channel->name, reason);

	Channel_remove_member(channel, usr); // Remove user from channel's list
	User_remove_channel(usr, channel->name);

	Server_message_channel(serv, serv->name, channel_name, broadcast_message);
	free(broadcast_message);
	free(reason);

	log_info("user %s has left channel %s", usr->nick, channel->name);

	if (ht_size(channel->members) == 0)
	{
		log_info("removing channel %s from server", channel->name);
		ht_remove(serv->name_to_channel_map, channel->name, NULL, NULL);
	}
}

/**
 * Returns statistics about local and global users, as numeric replies.
 *
 * TODO: Implement this
 */
void Server_handle_LUSERS(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "LUSERS"));

	// List_push_back(usr->msg_queue, make_reply(":%s "
	// RPL_LUSERCLIENT_MSG,
	// serv->hostname,
	// usr->nick,
	// ht_size(serv->username_to_user_map),
	// 0, 1));

	// List_push_back(usr->msg_queue, make_reply(":%s " RPL_LUSEROP_MSG,
	// serv->hostname,
	// usr->nick, 0));

	// List_push_back(usr->msg_queue, make_reply(":%s "
	// RPL_LUSERUNKNOWN_MSG,
	// serv->hostname,
	// usr->nick, 0));

	// List_push_back(usr->msg_queue, make_reply(":%s "
	// RPL_LUSERCHANNELS_MSG,
	// serv->hostname,
	// usr->nick,
	// ht_size(serv->name_to_channel_map)));

	// List_push_back(usr->msg_queue, make_reply(":%s " RPL_LUSERME_MSG,
	// serv->hostname,
	// usr->nick,
	// ht_size(serv->username_to_user_map),
	// 0, 0));
}

/**
 * The HELP command is used to return documentation about the IRC server and
 * the IRC commands it implements.
 */
void Server_handle_HELP(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "HELP"));

	const struct help_t *help = NULL;
	const char *subject = NULL;

	if (msg->n_params == 0 && !msg->body)
	{
		subject = "*";
		help = get_help_text("HELP");
	}
	else
	{
		subject = msg->n_params > 0 ? msg->params[0] : msg->body;
		help = get_help_text(subject);
	}

	if (help)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, "704 %s %s :%s", usr->nick, subject, help->title));
		List_push_back(usr->msg_queue, Server_create_message(serv, "705 %s %s :", usr->nick, subject));

		// Send help text as multipart messages and break long lines into
		// multiple messages.
		Vector *lines = text_wrap(help->body, 200);

		for (size_t i = 0; i < Vector_size(lines); i++)
		{
			List_push_back(usr->msg_queue, Server_create_message(serv, "705 %s %s :%s", usr->nick, subject, Vector_get_at(lines, i)));
		}

		Vector_free(lines);

		List_push_back(usr->msg_queue, Server_create_message(serv, "706 %s %s :End of help", usr->nick, subject));
		return;
	}

	List_push_back(usr->msg_queue, Server_create_message(serv, "524 %s %s :No help available on this topic", usr->nick, subject));
}

/**
 * Command: NOTICE
 * Parameters: <target>{,<target>} <text to be sent>
 *
 * The NOTICE command is used to send notices between users, as well as to send
 * notices to channels. <target> is interpreted the same way as it is for the
 * PRIVMSG command. The difference between NOTICE and PRIVMSG is that automatic
 * replies must never be sent in response to a NOTICE message.
 */
void Server_handle_NOTICE(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "NOTICE"));

	if (msg->n_params == 0)
	{
		return;
	}

	char *targets = msg->params[0];

	if (!targets)
	{
		return;
	}

	char *target = strtok(targets, ",");
	char *message = User_create_message(usr, "%s", msg->message);

	while (target)
	{
		if (target[0] == '#')
		{
			Server_message_channel(serv, serv->name, target + 1, message);
		}
		else
		{
			Server_message_user(serv, serv->name, target, message);
		}

		target = strtok(NULL, ",");
	}

	free(message);
}

void Server_handle_INFO(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "INFO"));
	List_push_back(usr->msg_queue, Server_create_message(serv, "371 %s :%s", usr->nick, serv->info));
	List_push_back(usr->msg_queue, Server_create_message(serv, "374 %s :End of INFO list", usr->nick));
}

/**
 * Command: CONNECT
 * Parameters: <target server> [<port> [<remote server>]]
 *
 * The CONNECT command forces a server to try to establish a new connection to
 * another server. CONNECT is a privileged command and is available only to IRC
 * Operators. If a remote server is given, the connection is attempted by that
 * remote server to <target server> using <port>.
 *
 * Note: Currently, <remote server> connection is not supported.
 */
void Server_handle_CONNECT(Server *serv, User *usr, Message *msg)
{
	assert(!strcmp(msg->command, "CONNECT"));

	if (msg->n_params == 0)
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NEEDMOREPARAMS_MSG, usr->nick, msg->command));
		return;
	}

	char *target_server = msg->params[0];
	assert(target_server);

	if (!strcmp(serv->hostname, target_server))
	{
		return;
	}

	struct peer_info_t target_info;
	memset(&target_info, 0, sizeof target_info);

	if (!get_peer_info(serv->config_file, target_server, &target_info))
	{
		List_push_back(usr->msg_queue, Server_create_message(serv, ERR_NOSUCHSERVER_MSG, usr->nick, target_server));
		free(target_info.peer_host);
		free(target_info.peer_name);
		free(target_info.peer_passwd);
		free(target_info.peer_port);
		return;
	}

	Connection *conn = Connection_create_and_connect(target_info.peer_host, target_info.peer_port);

	if (!conn)
	{
		return;
	}

	Server_add_connection(serv, conn);

	Peer *peer = Peer_alloc(PASSIVE_SERVER);
	peer->name = target_info.peer_name;

	conn->conn_type = PEER_CONNECTION;
	conn->data = peer;

	List_push_back(conn->outgoing_messages, make_string("PASS %s * *\r\n", target_info.peer_passwd));
	List_push_back(conn->outgoing_messages, make_string("SERVER %s\r\n", serv->name));

	free(target_info.peer_passwd);
	free(target_info.peer_host);
	free(target_info.peer_port);

	log_info("server %s initiated request with peer %s", serv->name, peer->name);
	log_debug("active server: %s, passive server: %s", serv->name, peer->name);
}

/**
 *  The SERVER command is used to register a new server.
 */
void Server_handle_SERVER(Server *serv, Peer *peer, Message *msg)
{
	assert(!strcmp(msg->command, "SERVER"));

	if (peer->name)
	{
		return;
	}

	if (peer->registered)
	{
		List_push_back(peer->msg_queue, Server_create_message(serv, "462 %s :You may not reregister", peer->name));
		return;
	}

	if (msg->n_params == 0)
	{
		List_push_back(peer->msg_queue, Server_create_message(serv, "461 %s %s :Not enough parameters", peer->name, msg->command));
		return;
	}

	peer->name = strdup(msg->params[0]);

	check_peer_registration(serv, peer);
}

/**
 * Command: PASS
 * Parameters: <password>
 */
void Server_handle_PASS(Server *serv, Peer *peer, Message *msg)
{
	assert(!strcmp(msg->command, "PASS"));

	if (peer->passwd)
	{
		return;
	}

	if (peer->registered)
	{
		List_push_back(peer->msg_queue, Server_create_message(serv, "462 %s :You may not reregister", peer->name));
		return;
	}

	if (msg->n_params == 0)
	{
		List_push_back(peer->msg_queue, Server_create_message(serv, "461 %s %s :Not enough parameters", peer->name, msg->command));
		return;
	}

	peer->passwd = strdup(msg->params[0]);

	check_peer_registration(serv, peer);
}

void check_peer_registration(Server *serv, Peer *peer)
{
	if (peer->registered || peer->quit || !peer->name || !peer->passwd)
	{
		return;
	}

	if (ht_contains(serv->name_to_peer_map, peer->name))
	{
		List_push_back(peer->msg_queue, make_string("ERROR :ID \"%s\" already registered\r\n", peer->name));
		peer->quit = true;
		return;
	}

	if (strcmp(peer->passwd, serv->passwd) != 0)
	{
		List_push_back(peer->msg_queue, make_string("ERROR :Bad password\r\n"));
		peer->quit = true;
		return;
	}

	if (peer->server_type == ACTIVE_SERVER)
	{
		char *other_passwd = get_server_passwd(serv->config_file, peer->name);

		if (!other_passwd)
		{
			List_push_back(peer->msg_queue, make_string("ERROR :Server not configured here\r\n"));
			peer->quit = true;
			return;
		}

		List_push_back(peer->msg_queue, Server_create_message(serv, "PASS %s 0210 |", other_passwd));
		List_push_back(peer->msg_queue, Server_create_message(serv, "SERVER %s :%s", serv->hostname, serv->info));
		free(other_passwd);
	}

	if (peer->server_type == ACTIVE_SERVER)
	{
		log_debug("active server: %s, passive server: %s", peer->name, serv->name);
	}
	else
	{
		log_debug("active server: %s, passive server: %s", serv->name, peer->name);
	}

	peer->registered = true;

	// State information exchange

	HashtableIter itr;
	ht_iter_init(&itr, serv->nick_to_user_map);
	User *other_user = NULL;
	while (ht_iter_next(&itr, NULL, (void **)&other_user))
	{
		if (other_user->registered && !other_user->quit)
		{
			List_push_back(peer->msg_queue, Server_create_message(serv, "NICK %s 1 %s %s 1 + :%s", other_user->nick, other_user->username, other_user->hostname, other_user->realname));
		}
	}

	ht_iter_init(&itr, serv->name_to_peer_map);
	Peer *other_peer = NULL;
	while (ht_iter_next(&itr, NULL, (void **)&other_peer))
	{
		if (other_peer->registered && !other_peer->quit)
		{
			List_push_back(peer->msg_queue, Server_create_message(serv, "SERVER %s", other_peer->name));
		}
	}

	ht_iter_init(&itr, serv->name_to_channel_map);
	Channel *other_channel = NULL;
	while (ht_iter_next(&itr, NULL, (void **)&other_channel))
	{
		List_push_back(peer->msg_queue, Server_create_message(serv, "MODE #%s", other_channel->name));
	}

	log_info("Server %s has registered", peer->name);
	ht_set(serv->name_to_peer_map, peer->name, peer);
}
