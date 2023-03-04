#pragma once

#include "types.h"

Membership *Membership_alloc(const char *channel, const char *username, int mode);
void Membership_free(Membership *membership);