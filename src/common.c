#include "include/common.h"

#include <stdarg.h>

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

char *rstrstr(char *string, char *pattern)
{
	char *next = strstr(string, pattern);
	char *prev = next;

	while (next)
	{
		next = strstr(prev + 2, "\r\n");

		if (next)
		{
			prev = next;
		}
	}

	return prev;
}

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

int int_compare(const void *key1, const void *key2)
{
	return *(int *)key1 - *(int *)key2;
}

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

char *addr_to_string(struct sockaddr *addr, socklen_t len)
{
	static char s[100];
	inet_ntop(addr->sa_family, get_in_addr(addr), s, len);
	return s;
}

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
			// 	continue;
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

		if(strstr(buf, "\r\n")) 
		{
			return bytes_read;
		}
	}

	return bytes_read;
}

ssize_t write_all(int fd, char *buf, size_t len)
{
	size_t bytes_written = 0;

	while (bytes_written < len)
	{
		errno = 0;
		ssize_t ret = write(fd, buf + bytes_written, len - bytes_written);

		// log_debug("%zd bytes sent", ret);

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
