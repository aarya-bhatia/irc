#pragma once

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <log.h>

#define MAX_MSG_LEN 512
#define MAX_MSG_PARAM 15
#define CRLF "\r\n"

int CHECK(int status, char *msg);
