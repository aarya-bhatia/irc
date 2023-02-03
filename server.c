#include "server.h"

static CC_HashTable *connections = NULL;

static volatile bool running = true;
static int port = 0;
static int server_fd = -1;
static int *client_fds = NULL;
static int num_clients = 0;

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

typedef struct Server {
	epoll_event events[MAX_CLIENTS];
} Server;

void start(Server *serv)
{

}

/*
void do_NICK(int fd, Message *msg);
void do_USER(int fd, Message *msg);
void do_PRIVMSG(int fd, Message *msg);
*/

void stop()
{
	running = false;

	if(client_fds != NULL)
	{
		for(size_t i = 0; i < MAX_CLIENTS; i++)
		{
			if(client_fds[i] != -1)
			{
				if(close(client_fds[i]) == 0)
				{
					num_clients--;
				}
			}
		}
	}

	if(server_fd != -1) {
		close(server_fd);
	}

	exit(0);
}


void *routine(void *args)
{
	int *index = (int *) args;

	pthread_mutex_lock(&m);
	int client_fd = client_fds[*index];
	pthread_mutex_unlock(&m);

	log_info("idx: %d; thread %d; client fd: %d", *index, (int) pthread_self(), client_fd);

	char *msg = calloc(2 * MAX_MSG_LEN, 1);
	size_t msglen = 0;

	while(running) 
	{
		ssize_t nread = read_all(client_fd, msg, MAX_MSG_LEN);

		if(nread == 0) {
			break;
		}

		if(nread == -1) {
			perror("read_all");
			break;
		}

		msglen = nread;
		msg[msglen] = 0;
		
		log_debug("Msg recv: %s", msg);

		if(write_all(client_fd, "OK\r\n", 4) == -1) {
			perror("write_all");
			break;
		}
	}

	close(client_fd);

	pthread_mutex_lock(&m);
	client_fds[*index] = -1;
	num_clients--;
	log_info("num clients: %zu", num_clients);
	pthread_cond_broadcast(&cv);
	pthread_mutex_unlock(&m);

	free(msg);
	free(index);

	return NULL;
}

void main_loop()
{
	pthread_t tid;

	while(1) 
	{
		log_info("waiting for connections");
		sleep(1);

		pthread_mutex_lock(&m);

		while(num_clients >= MAX_CLIENTS) 
		{
			pthread_cond_wait(&cv,&m);
		}
		
		pthread_mutex_unlock(&m);

		int client_fd = accept(server_fd, NULL, NULL);

		if(client_fd == -1) {
			perror("accept");
			continue;
		}

		int *index = calloc(1, sizeof(int));

		log_info("Got connection %d", client_fd);

		pthread_mutex_lock(&m);

		for(size_t i = 0; i < MAX_CLIENTS; i++) {
			if(client_fds[i] == AVAIL) {
				client_fds[i] = client_fd;
				num_clients++;
				*index = i;
				break;
			}
		}

		pthread_mutex_unlock(&m);

		pthread_create(&tid, NULL, routine, (void *) index); 
		pthread_detach(tid);
	}
}


int main(int argc, char *argv[])
{
	if(argc == 2) 
	{
		if(!strcmp(argv[1],"help")||!strcmp(argv[1],"usage")){
			fprintf(stderr, "Usage: %s [PORT]\n", *argv);
			return 0;
		}

		port = atoi(argv[1]);
	}
	else
	{
		port = SERVPORT;
	}

	if(!(client_fds = calloc(MAX_CLIENTS, sizeof(int))))
		die("calloc");

	for(size_t i = 0; i < MAX_CLIENTS; i++) client_fds[i] = AVAIL;

	num_clients = 0;

	if((server_fd = socket(PF_INET, SOCK_STREAM, 0))<0) 
		die("socket");

	struct sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(port);
	servAddr.sin_addr.s_addr = INADDR_ANY;

	if((bind(server_fd, (struct sockaddr *) &servAddr, sizeof servAddr))<0) 
		die("bind");

	if((listen(server_fd, MAX_CLIENTS))<0)
		die("listen");

	log_info("server running on port %d", port);

	signal(SIGINT,stop);

	main_loop();

	stop();

	return 0;
}
