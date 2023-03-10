

#include "include/hashtable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "include/common.h"

size_t djb2hash(const void *key, int len, uint32_t seed);

size_t ht_size(Hashtable *this) {
    return this->size;
}

size_t ht_capacity(Hashtable *this) {
    return this->capacity;
}

Hashtable *ht_alloc()
{
    Hashtable *this = calloc(1, sizeof *this);
    ht_init(this);
    return this;
}

void ht_free(Hashtable *this)
{
    ht_destroy(this);
    free(this);
}

char *ptr_to_string(void *ptr) {
    static char s[24];
    sprintf(s, "0x%p", ptr);
    return s;
}

void ht_print(Hashtable *this, char *(*key_to_string)(void *), char *(*value_to_string)(void *)) {
    if (!key_to_string) {
        key_to_string = ptr_to_string;
    }
    if (!value_to_string) {
        value_to_string = ptr_to_string;
    }

    HTNode *itr = NULL;
    for (size_t i = 0; i < this->capacity; i++) {
        if (!this->table[i]) {
            continue;
        }
        itr = this->table[i];
        while (itr) {
            log_info("bucket: %d, key: %s, value: %s", i, key_to_string(itr->key), value_to_string(itr->value));
            itr = itr->next;
        }
    }
}

/**
 * By default hashtable uses string keys and shallow copy and free for values
*/
void ht_init(Hashtable *this) {
    memset(this, 0, sizeof *this);

    this->capacity = HT_INITIAL_CAPACITY;
    this->table = calloc(HT_INITIAL_CAPACITY, sizeof *this->table);
    this->size = 0;
    this->seed = rand();
    this->key_len = -1;
    this->key_compare = (compare_type)strcmp;
    this->key_free = (elem_free_type)free;
    this->key_copy = (elem_copy_type)strdup;
    this->value_copy = NULL;
    this->value_free = NULL;
}

size_t ht_hash(const void *key, int key_len, int seed) {
    if (key_len < 0) {
        return djb2hash(key, strlen(key), seed);
    } else {
        return djb2hash(key, key_len, seed);
    }
}

HTNode *ht_find(Hashtable *this, const void *key) {
    size_t hash = ht_hash(key, this->key_len, this->seed) % this->capacity;

    HTNode *node = this->table[hash];

    while (node) {
        if (this->key_compare(node->key, key) == 0) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

void ht_set(Hashtable *this, void *key, void *value) {
    assert(this);
    assert(key);

    HTNode *found = ht_find(this, key);

    if (found) {  // Update existing node value
        if (this->value_free) {
            this->value_free(found->value);
        }

        found->value = this->value_copy ? this->value_copy(value) : value;
    } else {  // Create new node and add to bucket
        size_t hash = ht_hash(key, this->key_len, this->seed) % this->capacity;
        HTNode *new_node = calloc(1, sizeof *new_node);
        new_node->key = this->key_copy ? this->key_copy(key) : key;
        new_node->value = this->value_copy ? this->value_copy(value) : value;
        new_node->next = this->table[hash];
        this->table[hash] = new_node;
        this->size++;
    }

    // log_debug("size=%zu, capacity=%zu", this->size, this->capacity);

    /* Rehashing */
    if (this->size / this->capacity > HT_DENSITY) {
        size_t new_capacity = (this->capacity * 2) + 1;
        HTNode **new_table = calloc(new_capacity, sizeof(HTNode *));

        for (size_t i = 0; i < this->capacity; i++) {
            if (this->table[i]) {
                // Move old bucket to new index
                size_t new_hash = ht_hash(this->table[i]->key, this->key_len, this->seed) % new_capacity;
                new_table[new_hash] = this->table[i];
                this->table[i] = NULL;
            }
        }

        free(this->table);
        this->table = new_table;
        this->capacity = new_capacity;
    }
}

void *ht_get(Hashtable *this, const void *key) {
    assert(this);
    assert(key);

    HTNode *found = ht_find(this, key);

    if (found) {
        return found->value;
    }

    return NULL;
}

void ht_destroy(Hashtable *this) {
    if (!this) {
        return;
    }

    for (size_t i = 0; i < this->capacity; i++) {
        if (this->table[i]) {
            HTNode *itr = this->table[i];

            while (itr) {
                if (this->key_free) {
                    this->key_free(itr->key);
                }

                if (this->value_free) {
                    this->value_free(itr->value);
                }

                HTNode *tmp = itr->next;
                free(itr);
                itr = tmp;
            }
        }
    }

    memset(this->table, 0, sizeof *this->table * this->capacity);
    free(this->table);

    memset(this, 0, sizeof *this);
    log_debug("hashtable destroyed");
}

void ht_foreach(Hashtable *this, void (*callback)(void *key, void *value)) {
    for (size_t i = 0; i < this->capacity; i++) {
        if (this->table[i]) {
            HTNode *itr = this->table[i];

            while (itr) {
                callback(itr->key, itr->value);
                itr = itr->next;
            }
        }
    }
}

bool ht_contains(Hashtable *this, const void *key) {
    return ht_find(this, key) != NULL;
}

bool ht_remove(Hashtable *this, const void *key, void **key_out, void **value_out) {
    for (size_t i = 0; i < this->capacity; i++) {
        if (this->table[i]) {
            HTNode *curr = this->table[i];
            HTNode *prev = NULL;

            while (curr) {
                if (this->key_compare(curr->key, key) == 0) {
                    if (!prev) {
                        this->table[i] = curr->next;
                    } else {
                        prev->next = curr->next;
                    }

                    if (this->key_free) {
                        this->key_free(curr->key);
                    } else if (key_out) {
                        *key_out = curr->key;
                    }

                    if (this->value_free) {
                        this->value_free(curr->value);
                    } else if (value_out) {
                        *value_out = curr->value;
                    }

                    memset(curr, 0, sizeof *curr);
                    free(curr);

                    this->size--;

                    return true;
                }

                prev = curr;
                curr = curr->next;
            }
        }
    }

    return false;
}

void ht_iter_init(HashtableIter *itr, Hashtable *ht) {
    itr->hashtable = ht;
    itr->node = NULL;
    itr->index = 0;
}

bool ht_iter_next(HashtableIter *itr, void **key_out, void **value_out) {
    // log_debug("index = %d, size = %d, capacity = %d, node = %p", itr->index, itr->hashtable->size, itr->hashtable->capacity, itr->node);

    // End of table
    if (itr->index >= itr->hashtable->capacity) {
        return false;
    }

    // Get next node in bucket
    if (itr->node) {
        itr->node = itr->node->next;
    }

    // No more nodes in bucket
    if (!itr->node) {
        // Find next available bucket
        for (itr->index = itr->index + 1; itr->index < itr->hashtable->capacity; itr->index++) {
            if (itr->hashtable->table[itr->index]) {
                itr->node = itr->hashtable->table[itr->index];
                break;
            }
        }

        // End of table
        if (!itr->node || itr->index >= itr->hashtable->capacity) {
            return false;
        }
    }

    assert(itr->node);

    // Save key and value pointer in given pointers

    if (key_out) {
        *key_out = itr->node->key;
    }

    if (value_out) {
        *value_out = itr->node->value;
    }

    return true;
}

size_t djb2hash(const void *key, int len, uint32_t seed) {
    const char *str = key;
    register size_t hash = seed + 5381 + len + 1;

    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c;

    return hash;
}
