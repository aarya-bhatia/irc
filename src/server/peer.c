#include "include/server.h"

Peer *Peer_alloc()
{
	Peer *this = calloc(1, sizeof *this);
	this->msg_queue = List_alloc(NULL, free);
	this->nicks = Vector_alloc(1, (elem_copy_type) strcpy, free);
	this->channels = Vector_alloc(1, (elem_copy_type) strcpy, free);
	return this;
}

void Peer_free(Peer * this)
{
	List_free(this->msg_queue);
	Vector_free(this->nicks);
	Vector_free(this->channels);
	free(this->name);
	free(this->passwd);
	free(this);
}

bool
get_peer_info(const char *filename, const char *name, struct peer_info_t *info)
{
	FILE *file = fopen(filename, "r");

	if (!file) {
		return false;
	}

	char *line = NULL;
	size_t capacity = 0;
	ssize_t nread = 0;

	while ((nread = getline(&line, &capacity, file)) > 0) {
		if (line[nread - 1] == '\n') {
			line[nread - 1] = 0;
		}

		char *remote_name = strtok(line, ",");
		char *remote_host = strtok(NULL, ",");
		char *remote_port = strtok(NULL, ",");
		char *remote_passwd = strtok(NULL, ",");

		assert(remote_name);
		assert(remote_host);
		assert(remote_port);
		assert(remote_passwd);

		if (!strcmp(remote_name, name)) {
			info->peer_name = strdup(remote_name);
			info->peer_host = strdup(remote_host);
			info->peer_port = strdup(remote_port);
			info->peer_passwd = strdup(remote_passwd);
			fclose(file);
			free(line);

			return true;
		}
	}

	fclose(file);
	free(line);

	return false;
}

char *get_server_passwd(const char *config_filename, const char *name)
{
	// Load server info from file
	FILE *file = fopen(config_filename, "r");

	if (!file) {
		log_error("failed to open config file %s", config_filename);
		return false;
	}

	char *remote_name = NULL;
	char *remote_host = NULL;
	char *remote_port = NULL;
	char *remote_passwd = NULL;

	char *line = NULL;
	size_t capacity = 0;
	ssize_t nread = 0;

	while ((nread = getline(&line, &capacity, file)) > 0) {
		assert(line);

		remote_name = strtok(line, ",");
		remote_host = strtok(NULL, ",");
		remote_port = strtok(NULL, ",");
		remote_passwd = strtok(NULL, "\n");

		assert(remote_name);
		assert(remote_host);
		assert(remote_port);
		assert(remote_passwd);

		if (!strcmp(remote_name, name)) {
			break;
		}
	}

	fclose(file);

	if (!remote_passwd || strcmp(remote_name, name) != 0) {
		log_error("Server not configured in file %s", config_filename);
		free(line);
		return NULL;
	}

	log_debug("Password found for server %s: %s", name, remote_passwd);

	char *passwd = strdup(remote_passwd);
	free(line);
	return passwd;
}
