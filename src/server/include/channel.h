#pragma once

#include "types.h"
#include "include/hashtable.h"

Hashtable *load_channels(const char *filename);
void save_channels(Hashtable *hashtable, const char *filename);

Channel *Channel_alloc(const char *name);
void Channel_free(Channel *this);
void Channel_add_member(Channel *this, const char *username);
bool Channel_remove_member(Channel *this, const char *username);
bool Channel_has_member(Channel *this, const char *username);

