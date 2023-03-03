#pragma once

#include "types.h"

bool check_nick_available(Server *serv, User *usr, const char *nick);
CC_Array *get_current_nicks(Server *serv, User *usr);
bool update_nick_map(Server *serv, User *usr);
bool check_registration_complete(Server *serv, User *usr);
