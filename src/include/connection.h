#pragma once

#include "common.h"

typedef enum _conn_type_t {
	UNKNOWN_CONNECTION,
	USER_CONNECTION,
	PEER_CONNECTION,
	CLIENT_CONNECTION,
	SERVICE_CONNECTION
} conn_type_t;

typedef struct _Connection {
	int fd;
	conn_type_t conn_type;
	char *hostname;
	int port;
	bool quit;
	size_t req_len;					// request buffer length
	size_t res_len;					// response buffer length
	size_t res_off;					// num bytes sent from response buffer
	char req_buf[MAX_MSG_LEN + 1];	// request buffer
	char res_buf[MAX_MSG_LEN + 1];	// response buffer
	List *incoming_messages;		// queue of received messages
	List *outgoing_messages;		// queue of messages to deliver
	void *data;						// additional data for users and peers
} Connection;

Connection *Connection_alloc(int fd, struct sockaddr *addr, socklen_t addrlen);
Connection *Connection_create_and_connect(const char *hostname,
										  const char *port);
void Connection_free(Connection *);
ssize_t Connection_read(Connection *);
ssize_t Connection_write(Connection *);
