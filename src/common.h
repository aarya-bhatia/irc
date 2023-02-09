#pragma once

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

// Networking
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

// External Libraries
#include "include/log.h"
#include "include/collectc/cc_common.h"

#define MAX_EVENTS 10
#define MAX_MSG_LEN 512
#define MAX_MSG_PARAM 15
#define CRLF "\r\n"

#define _CHECK(status, msg)                         \
    if (status < 0)                                 \
    {                                               \
        log_error("Failed with status %d", status); \
        perror(msg);                                \
        exit(1);                                    \
    }

#define CHECK(status, msg) _CHECK(status, msg)

#define die(msg)     \
    {                \
        perror(msg); \
        exit(1);     \
    }


int create_and_bind_socket(char *hostname, char *port);

// integer comparator for hashtable
int int_compare(const void *key1, const void *key2);

void *get_in_addr(struct sockaddr *sa);
char *addr_to_string(struct sockaddr *addr, socklen_t len);

ssize_t read_all(int fd, char *buf, size_t len);
ssize_t write_all(int fd, char *buf, size_t len);
