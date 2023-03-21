#pragma once

#include <sys/types.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"

typedef struct Vector {
    void **elems;
    size_t size;
    size_t capacity;
    void *(*elem_copy)(void *);
    void (*elem_free)(void *);
} Vector;

Vector *Vector_alloc(size_t capacity, void *(*elem_copy)(void *),
		     void(*elem_free)(void *));
void Vector_free(Vector * this);
void Vector_reserve(Vector * this, size_t capacity);
void *Vector_find(Vector * this, bool (*cb)(void *, const void *),
		  const void *args);
void Vector_remove(Vector * this, size_t index, void **elem_out);
void Vector_push(Vector * this, void *elem);
void Vector_foreach(Vector * this, void (*cb)(void *));
void *Vector_get_at(Vector * this, size_t index);
size_t Vector_size(Vector * this);
