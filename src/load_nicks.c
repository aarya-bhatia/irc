#include "include/common.h"
#include "include/collectc/cc_array.h"

/**
 * Load all nicks from file into a hashmap with the key being the username and value begin an array of every associated nick.
 *
 * Each line of file should start with "username:" followed by a comma separated list of nicks for that username.
 */
CC_Array *load_nicks(const char *filename)
{
	CC_Array *nicks;

	cc_array_new(&nicks);

	FILE *file = fopen(filename, "r");

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

		// Add all nicks on each line into one array

		CC_Array *linked;
		cc_array_new(&linked);
	
		char *saveptr = NULL;
		char *token = strtok_r(line, ",", &saveptr);

		while(token)
		{
			token = trimwhitespace(token);

			if(token && token[0] != 0)
			{
				char *nick = strdup(token);
				cc_array_add(linked, nick);
			}

			token = strtok_r(NULL, ",", &saveptr);
		}

		cc_array_add(nicks, linked);
	}

	free(line);
	fclose(file);

	return nicks;
}
