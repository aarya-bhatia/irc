#include "include/common.h"

#include <ctype.h>
#include <stdarg.h>

/**
 * Use this utility function to allocate a string with given format and args.
 * The purpose of this function is to check the size of the resultant string
 * after all substitutions and allocate only those many bytes.
 */
char *make_string(char *format, ...) {
    va_list args;

    // Find the length of the output string

    va_start(args, format);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);

    // Create the output string

    char *s = calloc(1, n + 1);

    va_start(args, format);
    vsprintf(s, format, args);
    va_end(args);

    return s;
}

/**
 * Note: This function returns a pointer to a substring of the original string.
 * If the given string was allocated dynamically, the caller must not overwrite
 * that pointer with the returned value, since the original pointer must be
 * deallocated using the same allocator with which it was allocated.  The return
 * value must NOT be deallocated using free() etc.
 *
 */
char *trimwhitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str))
        str++;

    if (*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

/**
 * Return a pointer to the last occurrence of substring "pattern" in
 * given string "string". Returns NULL if pattern not found.
 */
char *rstrstr(char *string, char *pattern) {
    char *next = strstr(string, pattern);
    char *prev = next;

    while (next) {
        next = strstr(prev + strlen(pattern), pattern);

        if (next) {
            prev = next;
        }
    }

    return prev;
}

int connect_to_host(char *hostname, char *port) {
    struct addrinfo hints, *info = NULL;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret;

    if ((ret = getaddrinfo(hostname, port, &hints, &info)) == -1) {
        log_error("getaddrinfo() failed: %s", gai_strerror(ret));
        return -1;
    }

    int fd;

    struct addrinfo *itr = NULL;
    for (; itr != NULL; itr = itr->ai_next) {
        fd = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol);

        if (fd == -1) {
            continue;
        }

        if (connect(fd, itr->ai_addr, itr->ai_addrlen) != -1) {
            break; /* Success */
        }

        close(fd);
    }

    freeaddrinfo(info);

    if (!itr) {
        log_error("Failed to connect to server %s:%s", hostname, port);
        return -1;
    }

    log_info("connected to server %s on port %s", hostname, port);

    return fd;
}


/**
 *
 * This function will create and bind a TCP socket to give hostname and port.
 * Returns the socket fd if succeeded.
 * Kills the process if failure.
 */
int create_and_bind_socket(char *hostname, char *port) {
    struct addrinfo hints, *servinfo = NULL;

    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, port, &hints, &servinfo) != 0)
        die("getaddrinfo");

    int sock = socket(servinfo->ai_family,
                      servinfo->ai_socktype,
                      servinfo->ai_protocol);

    if (sock < 0)
        die("socket");

    if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
        die("connect");

    freeaddrinfo(servinfo);

    log_info("Connection established");

    return sock;
}

/**
 * Simple comparison function to compare two integers
 * given by pointers key1 and key2.
 * The return values are similar to strcmp.
 */
int int_compare(const void *key1, const void *key2) {
    return *(int *)key1 - *(int *)key2;
}

/**
 * integer copy constructor
 */
void *int_copy(void *other_int) {
    int *this = calloc(1, sizeof *this);
    *this = *(int *)other_int;
    return this;
}

/**
 * Returns the in_addr part of a sockaddr struct of either ipv4 or ipv6 type.
 */
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int get_port(struct sockaddr *sa, socklen_t len) {
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)sa;
        return ntohs(sin->sin_port);
    } else {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
        return ntohs(sin6->sin6_port);
    }
}

/**
 * Returns a pointer to a static string containing the IP address
 * of the given sockaddr.
 */
char *addr_to_string(struct sockaddr *addr, socklen_t len) {
    static char s[100];
    inet_ntop(addr->sa_family, get_in_addr(addr), s, len);
    return s;
}

/**
 * Used to read at most len bytes from fd into buffer.
 */
ssize_t read_all(int fd, char *buf, size_t len) {
    size_t bytes_read = 0;

    while (bytes_read < len) {
        errno = 0;
        ssize_t ret = read(fd, buf + bytes_read, len - bytes_read);

        if (ret == 0) {
            break;
        } else if (ret == -1) {
            // if (errno == EINTR)
            // {
            //      continue;
            // }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return bytes_read;
            }

            perror("read");
            break;
        } else {
            bytes_read += ret;
            buf[bytes_read] = 0;
        }

        if (strstr(buf, "\r\n")) {
            return bytes_read;
        }
    }

    return bytes_read;
}

/**
 * Used to write at most len bytes of buf to fd.
 */
ssize_t write_all(int fd, char *buf, size_t len) {
    size_t bytes_written = 0;

    while (bytes_written < len) {
        errno = 0;
        ssize_t ret =
            write(fd, buf + bytes_written, len - bytes_written);

        if (ret == 0) {
            break;
        } else if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return bytes_written;
            }

            perror("write");
            break;
        } else {
            bytes_written += ret;
        }
    }

    return bytes_written;
}

Vector *readlines(const char *filename) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror("fopen");
        log_error("Failed to open file %s", filename);
        return NULL;
    }

    Vector *lines = Vector_alloc(10, (elem_copy_type)strdup, free);

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    // Second line contains channel topic
    while ((nread = getline(&line, &len, file)) > 0) {
        assert(line);

        if (strlen(line) == 0) {
            continue;
        }

        size_t n = strlen(line);

        if (line[n - 1] == '\n') {
            line[n - 1] = 0;
        }

        Vector_push(lines, line);
    }

    free(line);
    fclose(file);

    return lines;
}

/**
 * Find the length of word in string at the given position.
 */
size_t word_len(const char *str) {
    char *tok = strstr(str, " ");

    if (!tok) {
        return strlen(str);
    } else {
        return tok - str;
    }
}

/**
 * Break given string into multiple lines at given line width.
 * Return a vector containing lines of the wrapped text.
 */
Vector *text_wrap(const char *str, const size_t line_width) {
    Vector *lines = Vector_alloc(4, NULL, free);
    size_t line_len = 0;
    const char *line = str;

    for (size_t i = 0; str[i] != 0; i++) {
        if (str[i] == '\n') {
            if (line && line_len > 0) {
                char *line_copy = strndup(line, line_len);
                Vector_push(lines, line_copy);
                line_len = 0;
                line = &str[i + 1];
                continue;
            }
        }

        if (isspace(str[i])) {
            if (line_len + word_len(str + i + 1) + 1 >= line_width) {
                char *line_copy = strndup(line, line_len);
                Vector_push(lines, line_copy);
                line_len = 0;
                line = &str[i + 1];
                continue;
            }
        }

        line_len++;
    }

    if (line != NULL && line_len > 0) {
        char *line_copy = strndup(line, line_len);
        Vector_push(lines, line_copy);
    }

    return lines;
}