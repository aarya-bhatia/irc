#pragma once

#include "common_types.h"

#include <sys/types.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Vector
{
	void **elems;
	size_t size;
	size_t capacity;
	elem_copy_type elem_copy;
	elem_free_type elem_free;
	compare_type compare;
} Vector;

Vector *Vector_alloc(size_t capacity, elem_copy_type elem_copy, elem_free_type elem_free);
Vector *Vector_alloc_type(size_t capacity, struct elem_type_info_t elem_type);
void Vector_free(Vector *this);
void Vector_reserve(Vector *this, size_t capacity);
bool Vector_contains(Vector *this, const void *target);
void Vector_remove(Vector *this, size_t index, void **elem_out);
void Vector_push(Vector *this, void *elem);
void Vector_foreach(Vector *this, elem_callback_type cb);
void *Vector_get_at(Vector *this, size_t index);
size_t Vector_size(Vector *this);
