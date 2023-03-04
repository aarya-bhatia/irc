#include "include/common.h"

#include <stdarg.h>
#include <ctype.h>

/**
 * Use this utility function to allocate a string with given format and args.
 * The purpose of this function is to check the size of the resultant string
 * after all substitutions and allocate only those many bytes.
 */
char *make_string(char *format, ...)
{
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
char *trimwhitespace(char *str)
{
	char *end;

	// Trim leading space
	while (isspace((unsigned char)*str))
		str++;

	if (*str == 0) // All spaces?
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
char *rstrstr(char *string, char *pattern)
{
	char *next = strstr(string, pattern);
	char *prev = next;

	while (next)
	{
		next = strstr(prev + strlen(pattern), pattern);

		if (next)
		{
			prev = next;
		}
	}

	return prev;
}

/**
 *
 * This function will create and bind a TCP socket to give hostname and port.
 * Returns the socket fd if succeeded.
 * Kills the process if failure.
 */
int create_and_bind_socket(char *hostname, char *port)
{
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
int int_compare(const void *key1, const void *key2)
{
	return *(int *)key1 - *(int *)key2;
}

/**
 * integer copy constructor
*/
void *int_copy(void *other_int)
{
	int *this = calloc(1, sizeof *this);
	*this = *(int *) other_int;
	return this;
}

/**
 * Returns the in_addr part of a sockaddr struct of either ipv4 or ipv6 type.
 */
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

/**
 * Returns a pointer to a static string containing the IP address
 * of the given sockaddr.
 */
char *addr_to_string(struct sockaddr *addr, socklen_t len)
{
	static char s[100];
	inet_ntop(addr->sa_family, get_in_addr(addr), s, len);
	return s;
}

/**
 * Used to read at most len bytes from fd into buffer.
 */
ssize_t read_all(int fd, char *buf, size_t len)
{
	size_t bytes_read = 0;

	while (bytes_read < len)
	{
		errno = 0;
		ssize_t ret = read(fd, buf + bytes_read, len - bytes_read);

		if (ret == 0)
		{
			break;
		}
		else if (ret == -1)
		{
			// if (errno == EINTR)
			// {
			//      continue;
			// }

			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return bytes_read;
			}

			perror("read");
			break;
		}
		else
		{
			bytes_read += ret;
			buf[bytes_read] = 0;
		}

		if (strstr(buf, "\r\n"))
		{
			return bytes_read;
		}
	}

	return bytes_read;
}

/**
 * Used to write at most len bytes of buf to fd.
 */
ssize_t write_all(int fd, char *buf, size_t len)
{
	size_t bytes_written = 0;

	while (bytes_written < len)
	{
		errno = 0;
		ssize_t ret =
			write(fd, buf + bytes_written, len - bytes_written);

		if (ret == 0)
		{
			break;
		}
		else if (ret == -1)
		{
			if (errno == EINTR)
			{
				continue;
			}

			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return bytes_written;
			}

			perror("write");
			break;
		}
		else
		{
			bytes_written += ret;
		}
	}

	return bytes_written;
}
