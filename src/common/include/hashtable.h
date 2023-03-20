#pragma once

#include "include/common_types.h"

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

#define HT_DENSITY 0.6
#define HT_INITIAL_CAPACITY 11

typedef struct HTNode
{
	void *key;
	void *value;
	struct HTNode *next;
} HTNode;

typedef struct Hashtable
{
	HTNode **table;
	size_t size;
	size_t capacity;
	int seed;
	int key_len;
	int (*key_compare)(const void *, const void *);
	void *(*key_copy)(void *);
	void (*key_free)(void *);
	void *(*value_copy)(void *);
	void (*value_free)(void *);
} Hashtable;

typedef struct HashtableIter
{
	Hashtable *hashtable;
	size_t index;
	HTNode *node;
} HashtableIter;

void Hashtable_set_value_type(Hashtable *this, struct elem_type_info_t info);
void Hashtable_set_key_type(Hashtable *this, struct elem_type_info_t info);

Hashtable *ht_alloc_type(struct elem_type_info_t key_type, struct elem_type_info_t value_type);

void ht_print(Hashtable *this, char *(*key_to_string)(void *), char *(*value_to_string)(void *));

Hashtable *ht_alloc();
void ht_free(Hashtable *this);

size_t ht_size(Hashtable *this);
size_t ht_capacity(Hashtable *this);

void ht_init(Hashtable *this);
void ht_destroy(Hashtable *this);
void *ht_get(Hashtable *this, const void *key);
void ht_set(Hashtable *this, void *key, void *value);
bool ht_remove(Hashtable *this, const void *key, void **key_out, void **value_out);
void ht_foreach(Hashtable *this, void (*callback)(void *key, void *value));
bool ht_contains(Hashtable *this, const void *key);
HTNode *ht_find(Hashtable *this, const void *key);

void ht_iter_init(HashtableIter *itr, Hashtable *ht);
bool ht_iter_next(HashtableIter *itr, void **key_out, void **value_out);