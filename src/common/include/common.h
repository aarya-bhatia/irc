#pragma once

// Standard Libraries
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Networking
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "hashtable.h"
#include "log.h"

#define MAX_EVENTS 10
#define MAX_MSG_LEN 512
#define MAX_MSG_PARAM 15
#define CRLF "\r\n"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef int (*compare_type)(const void *, const void *);
typedef void *(*elem_copy_type)(void *elem);
typedef void (*elem_free_type)(void *elem);

#define _CHECK(status, msg)                         \
    if (status < 0) {                               \
        log_error("Failed with status %d", status); \
        perror(msg);                                \
        exit(1);                                    \
    }

// Kill program if status code is less than 0 (error) with message
#define CHECK(status, msg) _CHECK(status, msg)

// Kill program with given error message instantly
#define die(msg)     \
    {                \
        perror(msg); \
        exit(1);     \
    }

#define make_reply(format, ...) make_string(format "\r\n", __VA_ARGS__) /* suffix each string with delimiter <cr><lf> */

char *trimwhitespace(char *str);

char *make_string(char *format, ...); /* allocates a string from format string and args with exact size */

char *rstrstr(char *string, char *pattern); /* reverse strstr: returns pointer to last occurrence of pattern in string */

/* wrap a block of code with a mutex lock */
#define SAFE(mutex, callback)   \
    pthread_mutex_lock(&mutex); \
    callback;                   \
    pthread_mutex_unlock(&mutex);

int int_compare(const void *key1, const void *key2); /* integer comparator */
void *int_copy(void *other_int);                     /* integer copy constructor */

// Networking helper functions
int create_and_bind_socket(char *hostname, char *port);     /* creates tcp socket to connect to given host and port  */
void *get_in_addr(struct sockaddr *sa);                     /* returns the in_addr of ivp4 and ipv6 addresses */
char *addr_to_string(struct sockaddr *addr, socklen_t len); /* get ip address from sockaddr */

// Note: For non blocking sockets, these fuctions may read/write fewer bytes than requested.
ssize_t read_all(int fd, char *buf, size_t len);  /* read all bytes from fd to buffer */
ssize_t write_all(int fd, char *buf, size_t len); /* write all bytes from fd to buffer */
