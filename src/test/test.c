#include "include/common.h"
#include "include/hashtable.h"
#include "include/list.h"
#include "include/message.h"
#include "include/vector.h"

struct s {
    int x;
};

struct s *struct_copy(struct s *other) {
    struct s *this = calloc(1, sizeof *this);
    memcpy(this, other, sizeof *this);
    return this;
}

void struct_free(struct s *other) {
    free(other);
}

void print(char *key, struct s *value) {
    printf("%s -> %d\n", key, value->x);
}

void hashtable_iter_test() {
}

void hashtable_test() {
    Hashtable this;
    ht_init(&this);

    this.value_copy = struct_copy;
    this.value_free = struct_free;

    {
        struct s s1;

        s1.x = 1;
        ht_set(&this, "hello", &s1);

        s1.x = 2;
        ht_set(&this, "world", &s1);
    }

    ht_foreach(&this, print);

    ht_remove(&this, "hello", NULL, NULL);

    assert(this.size == 1);

    struct s s1;

    s1.x = 1;
    ht_set(&this, "hello", &s1);

    assert(this.size == 2);

    struct s *s2 = ht_get(&this, "hello");
    assert(s2->x == s1.x);

    struct s s3;
    s3.x = -1;
    ht_set(&this, "hello", &s3);

    s2 = ht_get(&this, "hello");
    assert(s2->x == s3.x);

    s2 = ht_get(&this, "world");
    assert(s2->x == 2);

    ht_destroy(&this);
}

bool cb(void *elem, void *arg) {
    return strcmp(elem, arg) == 0;
}

void vector_test() {
    Vector *this = Vector_alloc(10, (elem_copy_type)strdup, free);

    Vector_push(this, "hello");
    Vector_push(this, "world");

    assert(this->size == 2);

    Vector_foreach(this, puts);

    puts(Vector_find(this, cb, "hello"));

    Vector_remove(this, 1, NULL);
    Vector_remove(this, 0, NULL);

    assert(this->size == 0);

    Vector_push(this, "a");
    Vector_push(this, "b");
    Vector_push(this, "c");

    Vector_remove(this, 1, NULL);

    assert(this->size == 2);

    Vector_free(this);
}

void message_test() {
    char s1[] = "NICK aarya\r\n";
    char s2[] = "USER aarya * * :Aarya Bhatia\r\n";
    char s3[] = "NICK aarya\r\nUSER aarya * * :Aarya Bhatia\r\n";

    Vector *arr = parse_all_messages(s1);
    Vector *arr2 = parse_all_messages(s2);
    Vector *arr3 = parse_all_messages(s3);

    Vector_free(arr);
    Vector_free(arr2);
    Vector_free(arr3);
}

void log_test() {
    log_trace("Hello %s", "world");
    log_debug("Hello %s", "world");
    log_info("Hello %s", "world");
    log_warn("Hello %s", "world");
    log_error("Hello %s", "world");
    log_fatal("Hello %s", "world");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return;
    }

    int test_num = atoi(argv[1]);

    switch (test_num) {
        case 1:
            message_test();
            break;
        case 2:
            vector_test();
            break;
        case 3:
            hashtable_test();
            break;
        case 4:
            hashtable_iter_test();
            break;
    }
    return 0;
}
