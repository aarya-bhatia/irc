#include "include/types.h"

/**
 * Load all nicks from file into a hashmap with the key being the username and value begin an array of every associated nick.
 *
 * Each line of file should start with "username:" followed by a comma separated list of nicks for that username.
 */
CC_HashTable *load_nicks(const char *filename)
{
	CC_HashTable *nick_map = NULL;

	// Create a new HashTable with integer keys
	if (cc_hashtable_new(&nick_map) != CC_OK) {
		log_error("Failed to create hashtable");
		return NULL;
	}

	FILE *file = fopen(filename, "r");

	if (!file) {
		fclose(fopen(filename, "w"));
		log_error("Created nicks file: %s", filename);
		return nick_map;
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

		CC_Array *linked;

		if (cc_array_new(&linked) != CC_OK) {
			log_error("Failed to create array");
			break;
		}

		char *saveptr = NULL;
		char *token = strtok_r(nicks, ",", &saveptr);

		while (token) {
			token = trimwhitespace(token);

			if (token && token[0] != 0) {
				char *nick = strdup(token);
				cc_array_add(linked, nick);
			}

			token = strtok_r(NULL, ",", &saveptr);
		}

		log_debug("Added %d nicks for username %s",
			  cc_array_size(linked), username);
		cc_hashtable_add(nick_map, strdup(username), linked);
	}

	free(line);
	fclose(file);

	return nick_map;
}
