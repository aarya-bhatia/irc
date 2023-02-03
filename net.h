#pragma once

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

#include "common.h"

#define die(msg) { perror(msg); exit(1); }

void *get_in_addr(struct sockaddr *sa);
char *addr_to_string(struct sockaddr *addr, socklen_t len);
ssize_t read_all(int fd, void *buf, size_t len);
ssize_t write_all(int fd, void *buf, size_t len);
