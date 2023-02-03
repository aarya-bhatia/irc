#pragma once

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>

// External Libraries
#include <log.h>
#include <collectc/cc_array.h>
#include <collectc/cc_hashtable.h>
#include <collectc/cc_common.h>

#define MAX_EVENTS 10
#define MAX_MSG_LEN 512
#define MAX_MSG_PARAM 15
#define CRLF "\r\n"

#define _CHECK(status, msg)                              \
        if (status < 0)                                 \
        {                                               \
            log_error("Failed with status %d", status); \
            perror(msg);                                \
            exit(1);                                    \
        }                                               \

#define CHECK(status, msg) _CHECK(status, msg)

int int_compare(const void *key1, const void *key2);