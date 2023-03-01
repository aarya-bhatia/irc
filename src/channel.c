#include "include/common.h"
#include "include/server.h"

Channel *Server_get_channel(Server *serv, char *channel_name);
Channel *Server_add_channel(Server *serv, char *channel_name);
void Server_remove_channel(Server *serv, char *channel_name);

void Channel_create(Server *serv, char *channel_name);
void Channel_save_to_file(Server *serv, char *filename);
void Channel_load_from_file(Server *serv, char *filename);
void Channel_destroy(Server *serv, char *channel_name);
void Channel_add_member(Channel *this, char *username);
void Channel_remove_member(Channel *this, char *username);

Channel *Server_get_channel(Server *serv, char *channel_name);
Channel *Server_add_channel(Server *serv, char *channel_name);
void Server_remove_channel(Server *serv, char *channel_name);

void Channel_create(Server *serv, char *channel_name);
void Channel_save_to_file(Server *serv, char *filename);
void Channel_load_from_file(Server *serv, char *filename);
void Channel_destroy(Server *serv, char *channel_name);
void Channel_add_member(Channel *this, char *username);
void Channel_remove_member(Channel *this, char *username);