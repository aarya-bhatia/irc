#include "include/membership.h"

Membership *Membership_create(char *channel, char *username, int mode)
{
	Membership *m = calloc(1, sizeof *m);
	m->channel = strdup(channel);
	m->username = strdup(username);
	m->mode = mode;

	return m;
}

void Membership_destroy(Membership *membership)
{
	if(!membership)
	{
		return;
	}

	free(membership->channel);
	free(membership->username);
	free(membership);
}