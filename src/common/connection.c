/**
 * Functions to read/write from a socket byte stream
 */

#include "include/connection.h"

#include "include/server.h"

Connection *Connection_alloc(int fd, struct sockaddr *addr, socklen_t addrlen) {
	Connection *this = calloc(1, sizeof *this);
	this->fd = fd;
	this->hostname = strdup(addr_to_string(addr, addrlen));
	this->port = get_port(addr);
	this->incoming_messages = List_alloc(NULL, free);
	this->outgoing_messages = List_alloc(NULL, free);
	return this;
}

Connection *Connection_create_and_connect(const char *hostname,
										  const char *port) {
	int fd = connect_to_host(hostname, port);

	if (fd == -1) {
		return NULL;
	}

	Connection *this = calloc(1, sizeof *this);
	this->fd = fd;
	this->hostname = strdup(hostname);
	this->port = atoi(port);
	this->incoming_messages = List_alloc(NULL, free);
	this->outgoing_messages = List_alloc(NULL, free);
	return this;
}

void Connection_free(Connection *this) {
	if (this->fd != -1) {
		shutdown(this->fd, SHUT_RDWR);
		close(this->fd);
	}
	List_free(this->incoming_messages);
	List_free(this->outgoing_messages);
	free(this->hostname);
	free(this);
}

/**
 * Returns number of bytes read
 */
ssize_t Connection_read(Connection *this) {
	assert(this);

	// invalid message
	if (this->req_len == MAX_MSG_LEN && !strstr(this->req_buf, "\r\n")) {
		return -1;
	}
	// Read available bytes into request buffer
	ssize_t nread = read_all(this->fd, this->req_buf + this->req_len,
							 MAX_MSG_LEN - this->req_len);

	if (nread <= 0) {
		log_error("read_all(): %s", strerror(errno));
		return -1;
	}

	this->req_len += nread;
	this->req_buf[this->req_len] = 0;

	char *start_msg = this->req_buf;
	char *end_msg = NULL;

	// Add all available messages to inbox
	while ((end_msg = strstr(start_msg, "\r\n")) != NULL) {
		char *message = strndup(start_msg, end_msg - start_msg);
		// log_debug("Message: %s", message);
		List_push_back(this->incoming_messages, message);
		start_msg = end_msg + 2;
	}

	// move partial message to front
	if (start_msg > this->req_buf &&
		start_msg < this->req_buf + this->req_len) {
		size_t new_len = strlen(start_msg);
		memmove(start_msg, this->req_buf, new_len);
		this->req_buf[new_len] = 0;
		this->req_len = new_len;
	} else {
		this->req_buf[0] = 0;
		this->req_len = 0;
	}

	return nread;
}

/**
 * Returns number of bytes written
 */
ssize_t Connection_write(Connection *this) {
	assert(this);

	// Send all available bytes from response buffer
	if (this->res_len > 0 && this->res_off < this->res_len) {
		ssize_t nsent = write_all(this->fd, this->res_buf + this->res_off,
								  this->res_len - this->res_off);
		// log_debug("Sent %zd bytes to fd %d", nsent, this->fd);

		if (nsent <= 0) {
			log_error("read_all(): %s", strerror(errno));
			return -1;
		}

		this->res_off += nsent;

		// Entire message was sent
		if (this->res_off >= this->res_len) {
			// Mark response buffer as empty
			this->res_off = this->res_len = 0;
		}

		return nsent;
	}

	List *queue = NULL;

	if (List_size(this->outgoing_messages)) {
		queue = this->outgoing_messages;
	} else if (this->conn_type == USER_CONNECTION) {
		queue = ((User *)this->data)->msg_queue;
	} else if (this->conn_type == PEER_CONNECTION) {
		queue = ((Peer *)this->data)->msg_queue;
	}

	// Check for pending messages in outgoing list
	if (queue && List_size(queue) > 0) {
		char *msg = List_peek_front(queue);

		// Add next message to response buffer
		strncpy(this->res_buf, msg, strlen(msg));

		this->res_len = strlen(msg);
		this->res_off = 0;

		List_pop_front(queue);
	}

	return 0;
}
