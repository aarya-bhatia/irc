#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/vector.h"

static size_t _align_capacity(size_t capacity)
{
	size_t i = 1;
	while (i < capacity)
	{
		i *= 2;
	}

	return i;
}

Vector *Vector_alloc(size_t capacity, elem_copy_type elem_copy, elem_free_type elem_free)
{
	Vector *this = calloc(1, sizeof *this);
	this->elems = calloc(capacity, sizeof *this->elems);
	this->size = 0;
	this->capacity = capacity;
	this->elem_copy = elem_copy;
	this->elem_free = elem_free;
	return this;
}

Vector *Vector_alloc_type(size_t capacity, struct elem_type_info_t elem_type)
{
	Vector *this = calloc(1, sizeof *this);
	this->elems = calloc(capacity, sizeof *this->elems);
	this->size = 0;
	this->capacity = capacity;
	this->elem_copy = elem_type.copy_type;
	this->elem_free = elem_type.free_type;
	this->compare = elem_type.comapre_type;
	return this;
}

void Vector_free(Vector *this)
{
	assert(this);
	for (size_t i = 0; i < this->size; i++)
	{
		if (this->elem_free)
		{
			this->elem_free(this->elems[i]);
		}
		this->elems[i] = NULL;
	}

	free(this->elems);
	free(this);
}

size_t Vector_size(Vector *this)
{
	assert(this);
	return this->size;
}

void *Vector_get_at(Vector *this, size_t index)
{
	assert(this);
	return index < this->size ? this->elems[index] : NULL;
}

void Vector_reserve(Vector *this, size_t capacity)
{
	assert(this);
	if (capacity <= this->capacity)
	{
		return;
	}
	this->capacity = _align_capacity(capacity);
	this->elems =
		realloc(this->elems, this->capacity * sizeof *this->elems);
	memset(this->elems + this->size, 0,
		   (this->capacity - this->size) * sizeof *this->elems);
}

bool Vector_contains(Vector *this, const void *target)
{
	assert(this);
	if (!this->compare)
	{
		return false;
	}

	for (size_t i = 0; i < this->size; i++)
	{
		if (this->compare(this->elems[i], target) == 0)
		{
			return true;
		}
	}

	return false;
}

void Vector_remove(Vector *this, size_t index, void **elem_out)
{
	assert(this);
	if (index >= this->size)
	{
		return;
	}

	if (this->elem_free)
	{
		this->elem_free(this->elems[index]);
	}
	else if (elem_out)
	{
		*elem_out = this->elems[index];
	}

	this->elems[index] = NULL;

	if (index < this->size - 1)
	{
		memmove(this->elems + index, this->elems + index + 1,
				(this->size - index - 1) * sizeof *this->elems);
	}

	this->size--;
}

void Vector_push(Vector *this, void *elem)
{
	assert(this);
	void *new = this->elem_copy ? this->elem_copy(elem) : elem;
	Vector_reserve(this, this->size + 1);
	this->elems[this->size++] = new;
}

void Vector_foreach(Vector *this, elem_callback_type cb)
{
	assert(this);
	assert(cb);
	for (size_t i = 0; i < this->size; i++)
	{
		cb(this->elems[i]);
	}
}
