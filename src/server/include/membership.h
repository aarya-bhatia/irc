#pragma once

#include "types.h"

Membership *Membership_create(char *channel, char *username, int mode);
void Membership_destroy(Membership *membership);