#pragma once

#include <sys/types.h>

typedef struct Vector
{
	void **elems;
	size_t size;
	size_t capacity;
	void *(*elem_copy)(void *);
	void (*elem_free)(void *);
} Vector;

void Vector_init(Vector *this, size_t capacity, void *(*elem_copy)(void *), void (*elem_free)(void *));
void Vector_destroy(Vector *this);
void Vector_reserve(Vector *this, size_t capacity);
void *Vector_find(Vector *this, bool (*cb)(void *, void *), void *args);
void Vector_remove(Vector *this, size_t index);
void Vector_push(Vector *this, void *elem);
void Vector_foreach(Vector *this, void (*cb)(void *));
void *Vector_get_at(Vector *this, size_t index);
size_t Vector_size(Vector *this);
