#include "common.h"

int CHECK(int status, char *msg)
{
	if(status < 0)
	{
		perror(msg);
		exit(1);
	}

	return status;
}

