#include "include/client.h"

void Client_init(Client *client)
{
	assert(client);

	*(client->nick) = 0;
	*(client->username) = 0;
	*(client->realname) = 0;
	client->sock = -1;
}

void Client_connect(Client *client, char *hostname, char *port)
{
	assert(client);
	assert(hostname);
	assert(port);

	if (client->sock != -1)
	{
		Client_disconnect(client);
	}

	client->sock = create_and_bind_socket(hostname, port);
}

void Client_disconnect(Client *client)
{
	assert(client);

	if (client->sock != -1)
	{
		shutdown(client->sock, SHUT_RDWR);
		close(client->sock);
	}
}

void Client_destroy(Client *client)
{
	if (!client)
	{
		return;
	}

	Client_disconnect(client);
}