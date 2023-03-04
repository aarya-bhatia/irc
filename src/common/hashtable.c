#define _GNU_SOURCE

#include "include/hashtable.h"

#include <assert.h>
#include <log.h>
#include <stdlib.h>
#include <string.h>

#include "include/common.h"

size_t djb2hash(const void *key, int len, uint32_t seed);

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

size_t ht_hash(void *key, int key_len, int seed) {
    if (key_len < 0) {
        return djb2hash(key, strlen(key), seed);
    } else {
        return djb2hash(key, key_len, seed);
    }
}

HTNode *ht_find(Hashtable *this, void *key) {
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

    log_debug("size=%zu, capacity=%zu", this->size, this->capacity);

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

void *ht_get(Hashtable *this, void *key) {
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

bool ht_contains(Hashtable *this, void *key) {
    return ht_find(this, key) != NULL;
}

bool ht_remove(Hashtable *this, void *key, void **key_out, void **value_out) {
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
    itr->index = ht->capacity;

    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->table[i] != NULL) {
            itr->index = i;
            itr->node = ht->table[i];
            break;
        }
    }
}

bool ht_iter_next(HashtableIter *itr, void **key_out, void **value_out) {
    if (!itr->node || itr->index >= itr->hashtable->capacity) {
        return false;
    }

    if (key_out) {
        *key_out = itr->node->key;
    }

    if (value_out) {
        *value_out = itr->node->value;
    }

    if (itr->node->next) {
        itr->node = itr->node->next;
    } else {
        while (itr->index < itr->hashtable->capacity && itr->hashtable->table[itr->index] == NULL) {
            itr->index++;
        }

        if (itr->index < itr->hashtable->capacity) {
            itr->node = itr->hashtable->table[itr->index];
        } else {
            itr->node = NULL;
        }
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
