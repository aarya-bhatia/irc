#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "vector.h"

void Vector_init(Vector *this, size_t capacity, void *(*elem_copy)(void *), void (*elem_free)(void *))
{
	this->elems = calloc(capacity, sizeof *this->elems);
	this->size = 0;
	this->capacity = capacity;
	this->elem_copy = elem_copy;
	this->elem_free = elem_free;
}

void Vector_destroy(Vector *this)
{
	for(size_t i = 0; i < this->size; i++) {
		if(this->elem_free) {
			this->elem_free(this->elems[i]);
		}
		this->elems[i] = NULL;
	}

	free(this->elems);
	free(this);
}

void Vector_reserve(Vector *this, size_t capacity)
{
	if(capacity <= this->capacity) { return; }
	this->capacity = capacity;
	this->elems = realloc(this->elems, capacity * sizeof *this->elems);
	memset(this->elems + this->size, 0, capacity - this->size);
}

void *Vector_find(Vector *this, bool (*cb)(void *, void *), void *args)
{
	for(size_t i = 0; i < this->size; i++) {
		if(cb(this->elems[i], args)) {
			return this->elems[i];
		}
	}
	
	return NULL;
}

void Vector_remove(Vector *this, size_t index)
{
	this->elem_free(this->elems[index]);
	this->elems[index] = NULL;
	
	if(index < this->size - 1) {
		memmove(this->elems, this->elems + 1, (this->size-index-1) * sizeof *this->elems); 
	}

	this->size--;
}

void Vector_push(Vector *this, void *elem)
{
	void *new = this->elem_copy ? this->elem_copy(elem) : elem;
	Vector_reserve(this, this->size + 1);
	this->elems[this->size++] = new;
}

void Vector_foreach(Vector *this, void (*cb)(void *))
{
	for(size_t i = 0; i < this->size; i++) {
		cb(this->elems[i]);
	}
}

bool cb(void *elem, void *arg)
{
	return strcmp(elem, arg) == 0;
}

int main()
{
	Vector this;

	Vector_init(&this, 10, strdup, free);

	Vector_push(&this, "hello");
	Vector_push(&this, "world");

	assert(this.size == 2);

	Vector_foreach(&this, puts);

	puts(Vector_find(&this, cb, "hello"));

	Vector_remove(&this, 1);
	Vector_remove(&this, 0);

	assert(this.size == 0);

	Vector_push(&this, "a");
	Vector_push(&this, "b");
	Vector_push(&this, "c");

	Vector_remove(&this, 1);

	assert(this.size == 2);
	
	return 0;
}

