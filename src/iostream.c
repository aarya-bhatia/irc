#include "include/iostream.h"

void IOStream_open(IOStream *stream, int sock)
{
    stream->fd = sock; 
    stream->last_write_time = time(NULL);
    stream->last_read_time = time(NULL);
    stream->read_buf_len = 0;
    stream->write_buf_len = 0;
    stream->write_buf_off = 0;

    cc_list_new(&stream->queue);
}

void IOStream_enqueue(IOStream *stream, char *message)
{
	cc_list_add_last(stream->queue, message);
}

void IOStream_read(IOStream *stream)
{
    ssize_t nread = read(stream->fd, stream->read_buf + stream->read_buf_len, MAX_MSG_LEN - stream->read_buf_len);

    if (nread == -1)
        die("read");

    stream->read_buf[nread] = 0;

    // Check if entire message was recieved
    char *msg_end = strstr(stream->read_buf + stream->read_buf_len, "\r\n");

    if (msg_end)
    {
        // Print message from server to stdout
        log_info("Server: %s", stream->read_buf);

        size_t pending_bytes = strlen(msg_end + 2);

        // check if there are more messages in buffer
        if (pending_bytes > 0)
        {
            // copy partial message to beginning of buffer
            memmove(stream->read_buf, msg_end + 2, pending_bytes);
            stream->read_buf[pending_bytes] = 0;
            stream->read_buf_len = strlen(stream->read_buf);
        }
        else
        {
            // Reset buffer to be empty
            stream->read_buf_len = 0;
            stream->last_read_time = time(NULL);
        }
    }
    else
    {
        stream->read_buf_len += nread;
    }
}

void IOStream_write(IOStream *stream)
{
    // Some bytes of request remain to be sent
    if (stream->write_buf_len > 0 && stream->write_buf_off < stream->write_buf_len)
    {
        ssize_t nwrite = write(stream->fd, stream->write_buf + stream->write_buf_off, stream->write_buf_len - stream->write_buf_off);

        if (nwrite == -1)
            die("write");

        stream->write_buf_off += nwrite;

        // All bytes of request were sent to server
        if (stream->write_buf_off >= stream->write_buf_len)
        {
            stream->write_buf_off = stream->write_buf_len = 0;
            stream->last_write_time = time(NULL);
        }

        return;
    }

    // No more messages
    if (cc_list_size(stream->queue) == 0)
    {
        return;
    }

    // Pop a message from queue
    char *message;
    cc_list_remove_first(stream->queue, (void **)&message);

    assert(message);
    assert(strlen(message) <= MAX_MSG_LEN);

    // Copy the message to request buffer
    strcpy(stream->write_buf, message);
    stream->write_buf_len = strlen(message);
    stream->write_buf_off = 0;
    free(message);
}

void _free_callback(void *msg)
{
    free(msg);
}

void IOStream_close(IOStream *stream)
{
    cc_list_destroy_cb(stream->queue, _free_callback);
    shutdown(stream->fd, SHUT_RDWR);
    close(stream->fd);
}