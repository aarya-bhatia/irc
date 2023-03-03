#pragma once

#include "types.h"

ssize_t User_Read_Event(Server *serv, User *usr);
ssize_t User_Write_Event(Server *serv, User *usr);
void User_Disconnect(Server *serv, User *usr);
void User_Destroy(User *usr);
void User_add_msg(User *usr, char *msg);
void User_save_to_file(User *usr, const char *filename);
User *User_load_from_file(const char *filename);
