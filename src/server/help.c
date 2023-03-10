#include "include/server.h"
#include "include/types.h"

const struct help_t help[] = {
    {"HELP", "** Help system **", "Try /HELP <command> for specific help or type /USERCMDS for a complete list of IRC commands supported on this server."},
    {"PRIVMSG", "** The PRIVMSG Command **", "The /PRIVMSG command is the main way to send messages to other users."},
    {"NOTICE", "** The NOTICE Command **", "The /NOTICE command is used to send messages to a client. This command does not send back automatic replies to the user."},
    {"NICK", "** The NICK Command **", "The /NICK command is used to change your nick."},
    {"USER", "** The USER Command **", "The /USER command is used to register a new client.", "It sets the username and realname of the client."},
    {"QUIT", "** The QUIT Command **", "The /QUIT command is used to close the connection ", " with a client."},
    {"WHO", "** The WHO Command **", ""},
    {"JOIN", "** The JOIN Command **", ""},
    {"PART", "** The PART Command **", ""},
    {"TOPIC", "** The TOPIC Command **", ""},
    {"LIST", "** The LIST Command **", ""},
    {"NAMES", "** The NAMES Command **", ""},
    {"LUSERS", "** The LUSERS Command **", ""},
    {"PING", "** The PING Command **", ""},
};

struct help_t *get_help_text(const char *subject) {
    static size_t n_help = sizeof help / sizeof *help;

    for (size_t i = 0; i < n_help; i++) {
        if (!strcmp(help[i].subject, subject)) {
            return help + i;
        }
    }

    return NULL;
}
