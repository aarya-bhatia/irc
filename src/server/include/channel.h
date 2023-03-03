#pragma once

#include "types.h"

Channel *Channel_create(const char *name);
void Channel_destroy(Channel *this);

void Channel_save_to_file(Channel *this, const char *filename);
Channel *Channel_load_from_file(const char *filename);
void Channel_add_member(Channel *this, const char *username);
bool Channel_remove_member(Channel *this, const char *username);
bool Channel_has_member(Channel *this, const char *username);

