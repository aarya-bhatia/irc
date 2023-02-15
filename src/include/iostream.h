#pragma once

#include "common.h"
#include "collectc/cc_list.h"

typedef struct IOStream
{
    int fd;                          // socket to talk to
    CC_List *queue;                  // outgoing message queue
    char read_buf[MAX_MSG_LEN + 1];  // buffer to store bytes being read from the server
    char write_buf[MAX_MSG_LEN + 1]; // buffer to store bytes being sent to the server
    size_t read_buf_len;             // size of the read buffer
    size_t write_buf_len;            // size of the write buffer
    size_t write_buf_off;            // position in the write buffer
    time_t last_write_time;          // time that message was last sent
    time_t last_read_time;           // time that message was last read
} IOStream;

void IOStream_open(IOStream *, int);
void IOStream_read(IOStream *);
void IOStream_write(IOStream *);
char *IOStream_dequeue(IOStream *stream);
void IOStream_enqueue(IOStream *, char *);
void IOStream_close(IOStream *);