// #include "common.h"

// typedef struct StringPair
// {
// char *first;
// char *second;
// } StringPair;

// char *get_subst(CC_List *subst, char *word)
// {
// CC_ListIter iter;
// cc_list_iter_init(&iter, subst);

// StringPair *out = NULL;

// while(cc_list_iter_next(&iter, (void **) &out) != CC_ITER_END)
// {
// if(out && out->first && !strcmp(out->first, word)) {
// return out->second;
// }
// }

// return NULL;
// }

// void render_template(char *template, char *params[MAX_MSG_PARAM],
// CC_List *subst)
// {
// char *ptr = strstr(template, "{}");
// char tmp[MAX_MSG_LEN + 1];

// for(size_t i = 0; i < MAX_MSG_PARAM && params[i] != NULL; i++)
// {
// char *value = get_subst(subst, params[i]);

// if(!value) {
// log_error("No substitution for %s", params[i]);
// return;
// }

// strcpy(tmp, ptr + 2);
// strcpy(ptr, value);
// strcat(ptr, tmp);
// ptr = strstr(ptr, "{}");
// log_debug(template);
// }
// }

// int main()
// {
// char *params[15] = {"client", "nick", "realname", "message"};
// char msg[513] = "Hello {} {} {} :{}";

// CC_List *subst;
// cc_list_new(&subst);

// StringPair pairs[] = {
// { "client", "aarya" },
// { "nick", "aarya" },
// { "realname", "Aarya Bhatia" },
// { "message", "Hello world" }
// };

// for(size_t i = 0; i < sizeof pairs/sizeof pairs[0]; i++)
// cc_list_add(subst, pairs + i);

// render_template(msg, params, subst);

// cc_list_destroy(subst);

// return 0;
// }

// // cc -Wall -g -std=c99 -Werror src/replies_test.c -Llib -llog
// -lcollectc
