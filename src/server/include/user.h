#pragma once

#include "types.h"

User *User_alloc(int fd, struct sockaddr *addr, socklen_t addrlen);
void User_free(User *this);

ssize_t User_Read_Event(Server *serv, User *usr);
ssize_t User_Write_Event(Server *serv, User *usr);

void User_save_to_file(User *usr, const char *filename);
User *User_load_from_file(const char *filename);

bool User_is_member(User *usr, const char *channel_name);
void User_add_channel(User *usr, const char *channel_name);
bool User_remove_channel(User *usr, const char *channel_name);