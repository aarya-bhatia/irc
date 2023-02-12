#include "include/server.h"

/**
 * User is available to send data to server
 */
ssize_t User_Read_Event(Server *serv, User *usr)
{
	assert(usr);
	assert(serv);

	// Read at most MAX_MSG_LEN bytes into the buffer
	ssize_t nread = read_all(usr->fd, usr->req_buf + usr->req_len, MAX_MSG_LEN - usr->req_len);

	// If no bytes read and no messages sent
	if (nread <= 0 && !strstr(usr->req_buf, "\r\n"))
	{
		User_Disconnect(serv, usr);
		return -1;
	}

	usr->req_len += nread;
	usr->req_buf[usr->req_len] = 0; // Null terminate the buffer

	log_debug("Read %zd bytes from fd %d", nread, usr->fd);

	if (strstr(usr->req_buf, "\r\n"))
	{
		// Buffer to store partial message
		char tmp[MAX_MSG_LEN + 1];
		tmp[0] = 0;

		// Number of avail messages
		int count = 0;

		char *s = usr->req_buf, *t = NULL;

		// Find last "\r\n" and store in t
		while ((s = strstr(s, "\r\n")) != NULL)
		{
			count++;
			t = s;
			s += 2;
		}

		// Check if there is a partial message
		if (t - usr->req_buf > 2)
		{
			strcpy(tmp, t + 2); // Copy partial message to temp buffer
			t[2] = 0;			// Shorten the request buffer to last complete message
		}

		log_info("Processing %d messages from user %d", count, usr->fd);

		// Process all CRLF-terminated messages from request buffer
		Server_process_request(serv, usr);

		// Copy pending message to request buffer
		strcpy(usr->req_buf, tmp);
		usr->req_len = strlen(tmp);

		log_debug("Request buffer size for user %d: %zu", usr->fd, usr->req_len);
	}

	return nread;
}

/**
 * User is available to recieve data from server
 */
ssize_t User_Write_Event(Server *serv, User *usr)
{
	assert(usr);
	assert(serv);

	// A message is still being sent
	if (usr->res_len && usr->res_off < usr->res_len)
	{
		// Send remaining message to user
		ssize_t nsent = write_all(usr->fd, usr->res_buf + usr->res_off, usr->res_len - usr->res_off);

		log_debug("Sent %zd bytes to user %d", nsent, usr->fd);

		if (nsent <= 0)
		{
			User_Disconnect(serv, usr);
			return -1;
		}

		usr->res_off += nsent;

		// Entire message was sent
		if (usr->res_off >= usr->res_len)
		{
			// Mark response buffer as empty
			usr->res_off = usr->res_len = 0;
		}

		return nsent;
	}

	// Check for pending messages
	if (cc_list_size(usr->msg_queue) > 0)
	{
		char *msg = NULL;

		// Dequeue one message and fill response buffer with message contents
		if (cc_list_remove_first(usr->msg_queue, (void **)&msg) == CC_OK)
		{
			strcpy(usr->res_buf, msg);
			usr->res_len = strlen(msg);
			usr->res_off = 0;
			free(msg);
		}
	}

	return 0;
}

void User_Disconnect(Server *serv, User *usr)
{
	assert(serv);
	assert(usr);

	log_info("Closing connection with user (%d): %s", usr->fd, usr->nick);
	epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, usr->fd, NULL);
	cc_hashtable_remove(serv->connections, (void *)&usr->fd, NULL);
	User_Destroy(usr);
}

void User_add_msg(User *usr, char *msg)
{
	assert(usr);
	assert(msg);
	assert(usr->msg_queue);

	cc_list_add_last(usr->msg_queue, msg);
}

/**
 * Close connection with user and free all memory associated with them.
 */
void User_Destroy(User *usr)
{
	if (!usr)
	{
		return;
	}

	free(usr->nick);
	free(usr->username);
	free(usr->realname);
	free(usr->hostname);

	CC_ListIter iter;
	cc_list_iter_init(&iter, usr->msg_queue);
	char *value;
	while (cc_list_iter_next(&iter, (void **)&value) != CC_ITER_END)
	{
		free(value);
	}
	cc_list_destroy(usr->msg_queue);

	shutdown(usr->fd, SHUT_RDWR);
	close(usr->fd);
}