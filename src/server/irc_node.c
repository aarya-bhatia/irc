#include "include/common.h"
#include "include/server.h"
#include "include/types.h"

/**
 * Create and connect to a an irc server
 *
 * Returns a new irc node
 *
 * - PASS
 * - SERVER
 *
 */
IRCNode *IRCNode_alloc(const char *name, const char *hostname, const char *port, const char *passwd) {
    struct addrinfo hints, *info = NULL;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret;

    if ((ret = getaddrinfo(hostname, port, &hints, &info)) == -1) {
        log_error("getaddrinfo() failed: %s", gai_strerror(ret));
        return NULL;
    }

    int fd;

    struct addrinfo *itr = NULL;
    for (; itr != NULL; itr = itr->ai_next) {
        fd = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol);

        if (fd == -1) {
            continue;
        }

        if (connect(fd, itr->ai_addr, itr->ai_addrlen) != -1) {
            break; /* Success */
        }

        close(fd);
    }

    if (!itr) {
        log_error("Failed to connect to server %s", name);
        return NULL;
    }

   	log_info("connected to server: %s", name);

    IRCNode *this = calloc(1, sizeof *this);

    this->fd = fd;
    this->name = strdup(name);
    this->hostname = strdup(addr_to_string(addr, addrlen));
	this->msg_queue = List_alloc(NULL, free);
	
	return this;
}

void IRCNode_free(IRCNode *this)
{
    free(this->name);

    shutdown(this->fd, SHUT_RDWR);
    close(this->fd);

	List_free(this->msg_queue);

    free(this);
}

bool Server_connect_to_remote(Server *serv, const char *name) {
	// Load server info from file
	FILE *fp = fopen(serv->config_file, "r");

	if(!fp) {
		log_error("failed to open config file %s", serv->config_file);
		return false;
	}

	char *remote_name = NULL;
	char *remote_host = NULL;
	char *remote_port = NULL;
	char *remote_passwd = NULL;

    ssize_t nread = 0;

    while ((nread = getline(&line, &len, file)) > 0) {
        if (line[nread - 1] == '\n') {
            line[nread - 1] = 0;
        }

		remote_name = strtok(line, ",");
		remote_host = strtok(NULL, ",");
		remote_port = strtok(NULL, ",");
		remote_passwd = strtok(NULL, ",");

		assert(remote_name);
		assert(remote_host);
		assert(remote_port);
		assert(remote_passwd);

		if(!strcmp(remote_name, name)) {
			break;
		}
	}

	fclose(fp);
	free(line);

	if(strcmp(remote_name, name) != 0) {
		log_error("Server not configured in file %s", serv->config_file);
		return false;
	}

	IRCNode *node = IRCNode_alloc(remote_name, remote_host, remote_port, remote_passwd);

	if(!node) {
		return false;
	}

    ht_set(serv->server_name_to_node_map, node->name, node);

	List_push_back(msg_queue, make_string("PASS %s * *\r\n", remote_passwd));
	List_push_back(msg_queue, make_string("SERVER %s :irc server\r\n", serv->name));

	return true;
}
