#include "include/K.h"
#include "include/types.h"
#include "include/channel.h"
#include "include/membership.h"
#include <time.h>

/**
 * Create and initialise new channel with given name and return it.
 */
Channel *Channel_create(const char *name)
{
	Channel *channel = calloc(1, sizeof *channel);
	channel->name = strdup(name);
	channel->time_created = time(NULL);
	channel->user_limit = MAX_CHANNEL_USERS;
	cc_array_new(&channel->members);

	return channel;
}

/**
 * Destroy all memory associated with channel
 */
void Channel_destroy(Channel *this)
{
	cc_array_destroy_cb(this->members, Membership_destroy);
	free(this->topic);
	free(this->name);
	free(this);
}

bool _find_member(Membership *m, char *username)
{
	return m && m->username && strcmp(m->username, username) == 0;
}

/**
 * Check if given user is part of channel.
 */
bool Channel_has_member(Channel *this, const char *username)
{
	assert(this);
	assert(username);

	return cc_array_find_element(this->members, _find_member, username) != NULL;
}

/**
 * Create membership for given user and add them to channel.
 */
void Channel_add_member(Channel *this, const char *username)
{
	assert(this);
	assert(username);

	if (Channel_has_member(this, username))
	{
		return;
	}

	Membership *membership = calloc(1, sizeof *membership);
	membership->username = strdup(username);
	membership->channel = strdup(this->name);

	cc_array_add(this->members, membership);

	log_info("User %s added to channel %s", username, this->name);
}

/**
 * Removes member from channel if they exist.
 * Returns true on success, false if they did not exist.
 */
bool Channel_remove_member(Channel *this, const char *username)
{
	assert(this);
	assert(username);

	for (size_t i = 0; i < cc_array_size(this->members); i++)
	{
		Membership *m = NULL;
		cc_array_get_at(this->members, i, (void **)&m);

		assert(!strcmp(m->channel, this->name));

		if (!strcmp(m->username, username))
		{
			cc_array_remove_at(this->members, i, NULL);
			free(m->username);
			free(m->channel);
			free(m);
			return true;
		}
	}

	log_error("User %s not found in channel %s", username, this->name);
	return false;
}

/**
 * Save the channel information to given file.
 * Note: The given file will be overwritten.
 *
 * Line 1: name, time_created, mode, user_limit
 * Line 2: topic
 * Line 3-n: member name, member mode
 *
 */
void Channel_save_to_file(Channel *this, const char *filename)
{
	assert(this);
	assert(filename);

	FILE *file = fopen(filename, "w");

	if (!file)
	{
		perror("fopen");
		log_error("Failed to open file %s", filename);
	}

	fprintf(file, "%s,%ld,%d,%d\n", this->name,
			(unsigned long)this->time_created, this->mode, this->user_limit);

	fprintf(file, "%s\n", this->topic);

	for (size_t i = 0; i < cc_array_size(this->members); i++)
	{
		Membership *member = NULL;
		cc_array_get_at(this->members, i, (void **)&member);
		fprintf(file, "%s,%d\n", member->username, member->mode);
	}

	fclose(file);

	log_info("Saved channel %s to file %s", this->name, filename);
}

/**
 * Allocate a channel struct and loads its data from given file.
 * Returns NULL if file not found or file has invalid format.
 * On success, it will return the new struct.
 *
 * Line 1: name, time_created, mode, user_limit
 * Line 2: topic
 * Line 3-n: member name, member mode
 */
Channel *Channel_load_from_file(const char *filename)
{
	assert(filename);

	FILE *file = fopen(filename, "r");

	if (!file)
	{
		perror("fopen");
		log_error("Failed to open file %s", filename);
		return NULL;
	}

	Channel *this = calloc(1, sizeof *this);

	// First line
	if (fscanf(file, "%s,%ld,%d,%d", this->name, &this->time_created,
			   &this->mode, &this->user_limit) != 4)
	{
		perror("fscanf");
		log_error("Invalid file format");
		free(this);
		fclose(file);
		return NULL;
	}

	cc_array_new(&this->members);

	char *line = NULL;
	size_t len = 0;
	ssize_t nread;

	// Second line contains channel topic
	if ((nread = getline(&line, &len, file)) > 0)
	{
		this->topic = strdup(line);
	}

	// Following lines contain the members
	while (nread > 0 && (nread = getline(&line, &len, file)) > 0)
	{
		if (line[len - 1] == '\n')
		{
			line[len - 1] = 0;
		}
		else
		{
			line[len] = 0;
		}

		if (strlen(line) == 0)
		{
			continue;
		}

		char *save = NULL;

		char *username = strtok_r(line, ",", &save);

		if (!username)
		{
			log_error("invalid file format");
			break;
		}

		char *mode = strtok_r(NULL, ",", &save);

		if (!mode)
		{
			log_error("invalid file format");
			break;
		}

		username = trimwhitespace(username);
		mode = trimwhitespace(mode);

		Membership *member = calloc(1, sizeof *member);
		member->channel = strdup(this->name);
		member->username = strdup(username);
		member->mode = atoi(mode);

		cc_array_add(this->members, member);
	}

	free(line);
	fclose(file);

	log_info("Loaded channel %s from file %s", this->name, filename);

	return this;
}
