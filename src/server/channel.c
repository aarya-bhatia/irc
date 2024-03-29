#include <time.h>

#include "include/server.h"

/**
 * Create and initialise new channel with given values and returns it.
 */
Channel *Channel_alloc(const char *name)
{
	Channel *channel = calloc(1, sizeof *channel);
	channel->name = strdup(name);
	channel->time_created = time(NULL);
	channel->members = ht_alloc_type(STRING_TYPE, SHALLOW_TYPE); /* Map<string,User*>
																  */
	return channel;
}

/**
 * Destroy all memory associated with channel
 */
void Channel_free(Channel *this)
{
	ht_free(this->members);
	free(this->topic);
	free(this->name);
	free(this);
}

/**
 * Check if given user is part of channel.
 */
bool Channel_has_member(Channel *this, User *user)
{
	return ht_contains(this->members, user->username);
}

/**
 * Create membership for given user and add them to channel.
 */
void Channel_add_member(Channel *this, User *user)
{
	ht_set(this->members, user->username, user);
}

/**
 * Removes member from channel if they exist.
 * Returns true on success, false if they did not exist.
 */
bool Channel_remove_member(Channel *this, User *user)
{
	return ht_remove(this->members, user->username, NULL, NULL);
}

/**
 * Loads channels from file into hashtable which maps channel name as string to Channel*.
 */
Hashtable *load_channels(const char *filename)
{
	Hashtable *hashtable = ht_alloc();
	hashtable->value_copy = NULL;
	hashtable->value_free = (elem_free_type)Channel_free;

	FILE *file = fopen(filename, "r");

	if (!file)
	{
		log_error("Failed to open file %s", filename);
		return hashtable;
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t nread = 0;

	while ((nread = getline(&line, &len, file)) > 0)
	{
		if (line[nread - 1] == '\n')
		{
			line[nread - 1] = 0;
		}

		char *saveptr = NULL;
		char *saveptr1 = NULL;

		char *info = strtok_r(line, ":", &saveptr);
		char *topic = strtok_r(NULL, ":", &saveptr);

		char *name = strtok_r(info, " ", &saveptr1);
		time_t time_created = atol(strtok_r(NULL, " ", &saveptr1));
		int mode = atoi(strtok_r(NULL, " ", &saveptr1));

		Channel *this = Channel_alloc(name);
		this->mode = mode;
		this->time_created = time_created;
		this->topic = topic ? strdup(topic) : NULL;

		if (!this->topic)
		{
			log_info("Added channel %s without topic", this->name);
		}
		else
		{
			log_info("Added channel %s with topic %s", this->name, this->topic);
		}

		ht_set(hashtable, name, this);
	}

	free(line);
	fclose(file);

	log_info("Loaded %zu channels from file %s", ht_size(hashtable),
			 filename);
	return hashtable;
}

/**
 * Write channels to give file with each line containing the (name, time_created, mode, user_limit, topic) of the channel.
 */
void save_channels(Hashtable *hashtable, const char *filename)
{
	FILE *file = fopen(filename, "w");
	if (!file)
	{
		log_error("Failed to open file %s", filename);
		return;
	}

	HashtableIter itr;
	ht_iter_init(&itr, hashtable);

	Channel *channel = NULL;

	while (ht_iter_next(&itr, NULL, (void **)&channel))
	{
		fprintf(file, "%s %ld %d", channel->name,
				(long)channel->time_created, channel->mode);

		if (channel->topic)
		{
			fprintf(file, " :%s", channel->topic);
		}

		fprintf(file, "\n");
	}

	fclose(file);
	log_info("Saved %zu channels to file %s", ht_size(hashtable), filename);
}
