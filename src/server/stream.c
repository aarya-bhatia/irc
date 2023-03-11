/**
 * Functions to read/write from a socket byte stream
 */

#include "include/common.h"
#include "include/server.h"
#include "include/types.h"

typedef struct DataStream {
    int fd;
    size_t req_len;                 // length of request buffer
    size_t res_len;                 // length of response buffer
    size_t res_off;                 // no of bytes of the response sent
    char req_buf[MAX_MSG_LEN + 1];  // the request message
    char res_buf[MAX_MSG_LEN + 1];  // the response message
    List *incoming_messages;
    List *outgoing_messages;
} DataStream;

/**
 * Returns number of bytes read
 */
ssize_t DataStream_read(DataStream *this) {
    assert(this);

    // invalid message
    if (this->req_len == MAX_MSG_LEN && !strstr(this->req_buf, "\r\n")) {
        return -1;
    }

    // Read available bytes into request buffer
    ssize_t nread = read_all(this->fd, this->req_buf + this->req_len, MAX_MSG_LEN - this->req_len);

    if (nread <= 0) {
        log_error("read_all(): %s", strerror(errno));
        return -1;
    }

    this->req_len += nread;
    this->req_buf[this->req_len] = 0;

    int num = 0;
    char *start_msg = this->req_buf;
    char *end_msg = NULL;

    // parse all messages and add them to incoming list
    for (end_msg = strstr(start_msg, "\r\n"); end_msg; end_msg = strstr(start_msg, "\r\n")) {
        char *message = strndup(start_msg, end_msg - start_msg + 2);
        List_push_back(this->incoming_messages, message);
        start_msg = end_msg + 2;
        num++;
    }

    // move partial message to front
    if (start_msg > this->req_buf && start_msg < this->req_buf + this->req_len) {
        size_t new_len = strlen(start_msg);
        memmove(start_msg, this->req_buf, new_len);
        this->req_buf[new_len] = 0;
        this->req_len = new_len;
    }

    return nread;
}

/**
 * Returns number of bytes written
 */
ssize_t DataStream_write(DataStream *this) {
    assert(this);

    // Send all available bytes from response buffer
    if (this->res_len > 0 && this->res_off < this->res_len) {
        ssize_t nsent = write_all(this->fd, this->res_buf + this->res_off, this->res_len - this->res_off);

        if (nsent <= 0) {
            log_error("read_all(): %s", strerror(errno));
            return -1;
        }

        this->res_off += nsent;

        // Entire message was sent
        if (this->res_off >= this->res_len) {
            // Mark response buffer as empty
            this->res_off = this->res_len = 0;
        }

        return nsent;
    }

    // Check for pending messages in outgoing list
    if (List_size(this->outgoing_messages) > 0) {
        char *msg = List_peek_front(this->outgoing_messages);

        // Add next message to response buffer
        strncpy(this->res_buf, msg, strlen(msg));

        this->res_len = strlen(msg);
        this->res_off = 0;

        List_pop_front(this->outgoing_messages);
    }

    return 0;
}
