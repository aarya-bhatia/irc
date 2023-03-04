#include "include/types.h"

/**
 * Load all nicks from file into a hashmap with the key being the username and value begin an array of every associated nick.
 *
 * Each line of file should start with "username:" followed by a comma separated list of nicks for that username.
 */
void load_nicks(Hashtable *nick_map, const char *filename)
{
	if (access(filename, F_OK) != 0) 
	{
		fclose(fopen(filename, "w"));
		log_error("Created nicks file: %s", filename);
		return;
	}

	FILE *file = fopen(filename, "r");

	if(!file)
	{
		perror("fopen");
		return;
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t nread = 0;

	while ((nread = getline(&line, &len, file)) > 0) {
		if (line[nread - 1] == '\n') {
			line[nread - 1] = 0;
		}

		if (strlen(line) == 0) {
			continue;
		}

		char *saveptr1 = NULL;
		char *username = strtok_r(line, ":", &saveptr1);

		username = trimwhitespace(username);

		if (!username || username[0] == 0) {
			log_error("invalid line: %s", line);
			break;
		}

		char *nicks = strtok_r(NULL, "", &saveptr1);

		// skip this user
		if (!nicks || nicks[0] == 0) {
			log_warn("username %s has no nicks", username);
			continue;
		}

		// Add all nicks on each line into one array

		Vector *linked = Vector_alloc(16, (elem_copy_type) strdup, free);

		char *saveptr = NULL;
		char *token = strtok_r(nicks, ",", &saveptr);

		while (token) {
			token = trimwhitespace(token);

			if (token && token[0] != 0) {
				char *nick = strdup(token);
				Vector_push(linked, nick);
			}

			token = strtok_r(NULL, ",", &saveptr);
		}

		log_debug("Added %d nicks for username %s", Vector_size(linked), username);
		ht_set(nick_map, username, linked);
	}

	free(line);
	fclose(file);
}
