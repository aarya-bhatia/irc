#include "net.h"

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

char *addr_to_string(struct sockaddr *addr, socklen_t len)
{
	static char s[100];
	inet_ntop(addr->sa_family, get_in_addr(addr), s, len);
	return s;
}

ssize_t read_all(int fd, void *buf, size_t len)
{
	size_t bytes_read = 0;

	while(bytes_read < len)
	{
		ssize_t ret = read(fd, buf + bytes_read, len - bytes_read);

		if(ret == 0) { break; }
		else if(ret == -1) {
			if(errno == EINTR) { continue; }
			else { perror("read"); break; }
		}
		else { bytes_read += ret; }
	}
}

ssize_t write_all(int fd, void *buf, size_t len)
{	
	size_t bytes_written = 0;

	while(bytes_written < len)
	{
		ssize_t ret = write(fd, buf + bytes_written, len - bytes_written);

		if(ret == 0) { break; }
		else if(ret == -1) {
			if(errno == EINTR) { continue; }
			else { perror("write"); break; }
		}
		else { bytes_written += ret; }
	}

}
