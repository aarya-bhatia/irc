#include "include/common.h"
#include "include/server.h"
#include <time.h>

/**
 * Find channel by name if exists and return it. Returns NULL if not found
 */
Channel *Server_get_channel(Server *serv, const char *name)
{
    assert(serv);
    assert(name);
    assert(serv->channels);

    for (size_t i = 0; i < cc_array_size(serv->channels); i++)
    {
        Channel *channel = NULL;

        if (cc_array_get_at(serv->channels, i, (void **)&channel) == CC_OK)
        {
            assert(channel);

            if (!strcmp(channel->name, name))
            {
                return channel;
            }
        }
    }

    return NULL;
}

/**
 * Create new channel if does not exist and return it.
 */
Channel *Server_add_channel(Server *serv, const char *name)
{
	Channel *channel = NULL;

	if((channel = Server_get_channel(serv, name)) == NULL)
	{
		channel = calloc(1, sizeof *channel);
		channel->name = strdup(name);
		channel->time_created = time(NULL);
		channel->private = false;
		cc_array_new(&channel->members);
	}

	return channel;
}

/**
 * Removes channel and free its memory if it exists.
 * The channel is saved to file before removal.
 * Returns true on success and false on failure.
 */
bool Server_remove_channel(Server *serv, const char *name)
{
	Channel *channel = Server_get_channel(serv, name);

	if(!channel)
	{
		return false;
	}
	
	char *filename = NULL;
	asprintf(&filename, "channels/%s", channel->name);
	Channel_save_to_file(channel, filename);
	free(filename);

	// Destroy array and free all username strings
	cc_array_destroy_cb(channel->members, free);
	free(channel->name);
	free(channel);

	return true;
}

/**
 * Save the channel information to given file.
 * Note: The given file will be overwritten.
 * 
 * File format
 * Line 1: name,time_created,private\n
 * Line 2-n: username\n
 *
 */
void Channel_save_to_file(Channel *this, const char *filename)
{
	assert(this);
	assert(filename);

	FILE *file = fopen(filename, "w");

	if(!file)
	{
		perror("fopen");
		log_error("Failed to open file %s", filename);
	}

	fprintf(file, "%s,%ld,%d\n", this->name, (unsigned long) this->time_created, this->private);

	for(size_t i = 0; i < cc_array_size(this->members); i++)
	{
		char *username = NULL;

		if(cc_array_get_at(this->members, i, (void **) &username) == CC_OK)
		{
			assert(username);

			fprintf(file, "%s\n", username);
		}
	}

	fclose(file);
}

/**
 * Load channel information into struct from given file.
 * Returns NULL if file not found or file has invalid format.
 * On success, it will allocate a new channel struct and return it.
 */
Channel *Channel_load_from_file(const char *filename)
{
	assert(filename);

	FILE *file = fopen(filename, "r");

	if(!file)
	{
		perror("fopen");
		log_error("Failed to open file %s", filename);
		return NULL;
	}

	Channel *this = calloc(1, sizeof *this);

	if(fscanf(file, "%s,%ld,%d", this->name, &this->time_created, &this->private) != 3)
	{
		perror("fscanf");
		log_error("Invalid file format");
		free(this);
		fclose(file);
		return NULL;
	}

	if(cc_array_new(&this->members) != CC_OK)
	{
		perror("cc_array_new");
		log_error("Failed to create array");
		free(this);
		fclose(file);
		return NULL;
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t nread;

	while((nread = getline(&line, &len, file)) > 0)
	{
		if(line[len-1]=='\n') { line[len-1] = 0; }
		else { line[len] = 0; }

		if(strlen(line) == 0)
		{
			continue;
		}

		char *username = strdup(line);	

		if(cc_array_add(this->members, username) != CC_OK)
		{
			perror("cc_array_add");
			log_error("Failed to insert into array");
			break;
		}
	}

	if(nread == -1)
	{
		perror("getline");
	}

	free(line);
	fclose(file);

	return this;
}

bool Channel_add_member(Channel *this, const char *username)
{
	assert(this);
	assert(username);

	if(cc_array_add(this->members, strdup(username)) != CC_OK)
	{
		perror("cc_array_add");
		return false;
	}

	log_info("User %s added to channel %s", username, this->name);

	return true;
}

bool Channel_remove_member(Channel *this, const char *username)
{
	assert(this);
	assert(username);

	for(size_t i = 0; i < cc_array_size(this->members); i++)
	{
		char *current = NULL;

		if(cc_array_get_at(this->members, i, (void **) &current) == CC_OK)
		{
			assert(current);

			// Found user
			if(!strcmp(current, username))
			{
				if(cc_array_remove_at(this->members, i, NULL) == CC_OK)
				{
					free(current);
					return true;
				}
				else
				{
					perror("cc_array_remove_at");
					return false;
				}
			}
		}
		else
		{
			perror("cc_array_get_at");
			return false;
		}
	}

	log_error("User %s not found in channel %s", username, this->name);
	return false;
}


