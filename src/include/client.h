#pragma once

#include "include/common.h"
#include "include/message.h"
#include "include/client.h"
#include "include/collectc/cc_list.h"

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>

#define DEBUG
#define PING_INTERVAL_SEC 3
#define EPOLL_TIMEOUT 1000
#define MAX_QUEUE_SIZE 100
#define MAX_EPOLL_EVENTS 3 // stdin, socket, timer

void read_user_input();
void read_from_socket(int sock, char read_buf[], size_t *read_buf_len);
void write_to_socket(int sock, char write_buf[], size_t *write_buf_len, size_t *write_buf_off);
