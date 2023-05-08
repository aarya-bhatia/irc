#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "common_types.h"

typedef struct Vector {
	void **elems;
	size_t size;
	size_t capacity;
	elem_copy_type elem_copy;
	elem_free_type elem_free;
	compare_type compare;
} Vector;

Vector *Vector_alloc(size_t capacity, elem_copy_type elem_copy,
					 elem_free_type elem_free);
Vector *Vector_alloc_type(size_t capacity, struct elem_type_info_t elem_type);
void Vector_free(Vector *);
void Vector_reserve(Vector *, size_t capacity);
bool Vector_contains(Vector *, const void *target);
void Vector_remove(Vector *, size_t index, void **elem_out);
void Vector_push(Vector *, void *elem);
void Vector_foreach(Vector *, elem_callback_type cb);
void *Vector_get_at(Vector *, size_t index);
size_t Vector_size(Vector *);
