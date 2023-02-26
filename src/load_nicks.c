#include "include/common.h"

/**
 * Load all nicks from file and create a disoint set where nicks owned by same user are linked.
 */
void *load_nicks()
{
	FILE *file = fopen("data/nicks", "r");

	char *line = NULL;
	size_t len = 0;
	ssize_t nread;

	while(1)
	{
		nread = getline(&line, &len, file);

		if(nread == -1)
		{
			perror("getline");
			fclose(file);
			return;
		}

		if(nread == 0)
		{
			break;
		}

		if(line[nread-1] == '\n')
		{
			line[nread-1] = 0;
		}

		if(strlen(line) == 0)
		{
			continue;
		}

		char *saveptr = NULL;
		char *token = strtok_r(line, ",", &saveptr);

		while(token)
		{
			token = trimwhitespace(token);

			if(token && token[0] != 0)
			{
				char *nick = strdup(token);
			}

			token = strtok_r(NULL, ",", &saveptr);
		}
	}

	free(line);
	fclose(file);
}
