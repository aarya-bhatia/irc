#include "common.h"
#include "net.h"

int main(int argc, char *argv[])
{
	if(argc != 3) {
		fprintf(stderr, "Usage: %s hostname port\n", *argv);
		return 1;
	}

	char *hostname = argv[1];
	char *port = argv[2];

	struct addrinfo hints, *servinfo = NULL;

	memset(&hints,0,sizeof hints);
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;

	if(getaddrinfo(hostname, port, &hints, &servinfo) != 0)
		die("getaddrinfo");

	int sock = socket(servinfo->ai_family,
			servinfo->ai_socktype,
			servinfo->ai_protocol);

	if(sock < 0) die("socket");

	if(connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
		die("connect");

	freeaddrinfo(servinfo);

	log_info("Connection with server is established...\n");

	////////////////////////////////////////////////////////

	char prompt[1024], servmsg[1024], usermsg[1024];
	int write_status, read_status;

	// Get user nickname from stdin
	sprintf(prompt, "Enter Nickname >");
	write_all(1, prompt, strlen(prompt));
	scanf("%s",usermsg);

	// Send user nickname to server
	char *nick = strdup(usermsg);
	sprintf(servmsg, "NICK %s\r\n", nick); 
	log_debug("Sending message: ", servmsg);
	write_all(sock, servmsg, strlen(servmsg));

	// Get server reply
	read_all(sock, servmsg, sizeof(servmsg));
	write(1, "Server > ", strlen("Server > "));
	puts(servmsg);

	// Get user name from stdin
	sprintf(prompt, "Enter Name >");
	write_all(1, prompt, strlen(prompt));
	scanf("%s",usermsg);

	// Send user name to server
	char *name = strdup(usermsg);
	sprintf(servmsg, "USER %s * * :%s\r\n", nick, name);
	log_debug("Sending message: ", servmsg);
	write_all(sock, servmsg, strlen(servmsg));
	
	// Get server reply
	read_all(sock, servmsg, sizeof(servmsg));
	write(1, "Server > ", strlen("Server >"));
	puts(servmsg);

	// Exit
	close(sock);
	log_info("Goodbye!");
	return 0;
}