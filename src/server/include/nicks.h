#pragma once

#include "include/message.h"
#include "types.h"

Hashtable *load_nicks(const char *filename);
void save_nicks(Hashtable *, const char *filename);
