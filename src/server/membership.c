#include "include/membership.h"

Membership *Membership_alloc(const char *channel, const char *username, int mode)
{
	Membership *m = calloc(1, sizeof *m);
	m->channel = strdup(channel);
	m->username = strdup(username);
	m->mode = mode;

	return m;
}

void Membership_free(Membership *membership)
{
	if(!membership)
	{
		return;
	}

	free(membership->channel);
	free(membership->username);
	free(membership);
}
