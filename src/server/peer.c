#include "include/server.h"

/**
 * Creates and connects to an irc server
 */
Peer *Peer_alloc() {
    Peer *this = calloc(1, sizeof *this);
    this->msg_queue = List_alloc(NULL, free);
    return this;
}

void Peer_free(Peer *this) {
    List_free(this->msg_queue);
    free(this->name);
    free(this);
}

bool Server_add_peer(Server *serv, const char *name) {
    // Load server info from file
    FILE *file = fopen(serv->config_file, "r");

    if (!file) {
        log_error("failed to open config file %s", serv->config_file);
        return false;
    }

    char *remote_name = NULL;
    char *remote_host = NULL;
    char *remote_port = NULL;
    char *remote_passwd = NULL;

    char *line = NULL;
    size_t capacity = 0;
    ssize_t nread = 0;

    while ((nread = getline(&line, &capacity, file)) > 0) {
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

        if (!strcmp(remote_name, name)) {
            break;
        }
    }

    fclose(file);
    free(line);

    if (strcmp(remote_name, name) != 0) {
        log_error("Server not configured in file %s", serv->config_file);
        return false;
    }

    int fd = connect_to_host(remote_host, remote_port);

    if (fd == -1) {
        return false;
    }

    Connection *conn = calloc(1, sizeof *conn);
    conn->fd = fd;
    conn->hostname = strdup(remote_host);
    conn->port = atoi(remote_port);
    conn->incoming_messages = List_alloc(NULL, free);
    conn->outgoing_messages = List_alloc(NULL, free);

    Peer *peer = Peer_alloc();
    peer->name = strdup(remote_name);

    conn->conn_type = PEER_CONNECTION;
    conn->data = peer;

    ht_set(serv->connections, &fd, conn);
    ht_set(serv->name_to_peer_map, remote_name, peer);

    List_push_back(conn->outgoing_messages, make_string("PASS %s * *\r\n", remote_passwd));
    List_push_back(conn->outgoing_messages, make_string("SERVER %s :\r\n", serv->name));

    return true;
}
