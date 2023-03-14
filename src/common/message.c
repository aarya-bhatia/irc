#include "include/message.h"

#include "include/common.h"

void message_init(Message *msg) {
    assert(msg);
    memset(msg, 0, sizeof(Message));
}

void message_destroy(Message *msg) {
    if (!msg) {
        return;
    }

    free(msg->origin);
    free(msg->command);
    free(msg->body);

    for (size_t i = 0; i < MAX_MSG_PARAM; i++) {
        free(msg->params[i]);
    }
}

void message_free_callback(void *ptr) {
    message_destroy(ptr);
    free(ptr);
}

Vector *parse_message_list(List *list) {
    Vector *array = Vector_alloc(4, NULL, message_free_callback);  // initialise a message vector with shallow copy and destructor

    int ret;

    while(List_size(list) > 0) {
        char *message = List_peek_front(list);

        Message *msg = calloc(1, sizeof *msg);
        message_init(msg);

        if ((ret = parse_message(message, msg)) == -1) {
            log_warn("Invalid message");
            message_destroy(msg);
        } else {
            Vector_push(array, msg);
        }

        List_pop_front(list);
    }

    return array;
}

Vector *parse_all_messages(char *str) {
    assert(str);

    char *saveptr;
    char *tok = strtok_r(str, "\r\n", &saveptr);

    int ret;

    Vector *array = Vector_alloc(4, NULL, message_free_callback);  // initialise a message vector with shallow copy and destructor

    while (tok) {
        Message *msg = calloc(1, sizeof *msg);
        message_init(msg);

        log_debug("Message: %s", tok);

        if ((ret = parse_message(tok, msg)) == -1) {
            log_warn("Invalid message");
            message_destroy(msg);
        } else {
            Vector_push(array, msg);
        }

        tok = strtok_r(NULL, "\r\n", &saveptr);
    }

    return array;
}

int parse_message(char *str, Message *msg) {
    assert(str);

    char *ptr = strstr(str, ":");

    if (ptr == str) {
        ptr = strstr(ptr + 1, ":");
    }

    if (ptr) {
        msg->body = strdup(ptr + 1);
        *ptr = 0;
    }

    char *saveptr;
    char *tok = strtok_r(str, " ", &saveptr);

    if (!tok) {
        return -1;
    }
    // prefix
    if (tok[0] == ':') {
        if (tok[1] != ' ' && tok[1] != '\0') {
            msg->origin = strdup(tok + 1);
        }

        tok = strtok_r(NULL, " ", &saveptr);
    }

    if (!tok) {
        return -1;
    }
    // command
    msg->command = strdup(tok);

    // params
    size_t i = 0;

    while (tok && i < 15) {
        if ((tok = strtok_r(NULL, " ", &saveptr)) != NULL) {
            msg->params[i++] = strdup(tok);
        }
    }

    // log_debug("Origin: %s", msg->origin);
    // log_debug("Command: %s", msg->command);
    // log_debug("Body: %s", msg->body);

    msg->n_params = i;

    // for (size_t j = 0; j < i; j++)
    //{
    //      log_debug("Param %d: %s", j + 1, msg->params[j]);
    // }

    return 0;
}
