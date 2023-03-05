

#include "include/vector.h"

Vector *Vector_alloc(size_t capacity, void *(*elem_copy)(void *), void (*elem_free)(void *)) {
    Vector *this = calloc(1, sizeof *this);
    this->elems = calloc(capacity, sizeof *this->elems);
    this->size = 0;
    this->capacity = capacity;
    this->elem_copy = elem_copy;
    this->elem_free = elem_free;
    return this;
}

void Vector_free(Vector *this) {
    for (size_t i = 0; i < this->size; i++) {
        if (this->elem_free) {
            this->elem_free(this->elems[i]);
        }
        this->elems[i] = NULL;
    }

    free(this->elems);
    free(this);
}

size_t Vector_size(Vector *this) {
    return this->size;
}

void *Vector_get_at(Vector *this, size_t index) {
    return index < this->size ? this->elems[index] : NULL;
}

void Vector_reserve(Vector *this, size_t capacity) {
    if (capacity <= this->capacity) {
        return;
    }
    this->capacity = capacity;
    this->elems = realloc(this->elems, capacity * sizeof *this->elems);
    memset(this->elems + this->size, 0, (capacity - this->size) * sizeof *this->elems);
}

void *Vector_find(Vector *this, bool (*cb)(void *, const void *), const void *args) {
    for (size_t i = 0; i < this->size; i++) {
        if (cb(this->elems[i], args)) {
            return this->elems[i];
        }
    }

    return NULL;
}

void Vector_remove(Vector *this, size_t index, void **elem_out) {
    if (index >= this->size) {
        return;
    }

    if (this->elem_free) {
        this->elem_free(this->elems[index]);
    } else if (elem_out) {
        *elem_out = this->elems[index];
    }

    this->elems[index] = NULL;

    if (index < this->size - 1) {
        memmove(this->elems + index, this->elems + index + 1, (this->size - index - 1) * sizeof *this->elems);
    }

    this->size--;
}

void Vector_push(Vector *this, void *elem) {
    void *new = this->elem_copy ? this->elem_copy(elem) : elem;
    Vector_reserve(this, this->size + 1);
    this->elems[this->size++] = new;
}

void Vector_foreach(Vector *this, void (*cb)(void *)) {
    for (size_t i = 0; i < this->size; i++) {
        cb(this->elems[i]);
    }
}
