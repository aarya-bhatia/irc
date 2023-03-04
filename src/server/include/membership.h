#pragma once

#include "types.h"

Membership *Membership_alloc(char *channel, char *username, int mode);
void Membership_free(Membership *membership);