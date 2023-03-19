#include "include/list.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void List_iter_init(ListIter *iter, List *list)
{
    assert(iter);
    assert(list);

    iter->current = list->head;
}

bool List_iter_next(ListIter *iter, void **elem_ptr)
{
    assert(iter);

    if(!iter->current) {
        return false;
    }

    if(elem_ptr) {
        *elem_ptr = iter->current->elem;
    }

    iter->current = iter->current->next;

    return true;
}

List *List_alloc(void *(*elem_copy)(void *), void (*elem_free)(void *)) {
    List *this = calloc(1, sizeof *this);
    List_init(this, elem_copy, elem_free);
    return this;
}

void List_free(List *this) {
    List_destroy(this);
    free(this);
}

void List_init(List *this, void *(*elem_copy)(void *), void (*elem_free)(void *)) {
    this->head = NULL;
    this->tail = NULL;
    this->size = 0;
    this->elem_copy = elem_copy;
    this->elem_free = elem_free;
}

void List_destroy(List *this) {
    ListNode *tmp = this->head;

    while (tmp) {
        ListNode *next = tmp->next;

        if (this->elem_free) {
            this->elem_free(tmp->elem);
            tmp->elem = NULL;
        }

        free(tmp);
        tmp = next;
    }

    memset(this, 0, sizeof *this);
}

size_t List_size(List *this) {
    return this->size;
}

void List_push_front(List *this, void *elem) {
    ListNode *node = calloc(1, sizeof *node);
    node->elem = this->elem_copy ? this->elem_copy(elem) : elem;

    if (this->head) {
        node->next = this->head;
        this->head->prev = node;
        this->head = node;
    } else {
        this->head = this->tail = node;
    }

    this->size++;
}

void List_push_back(List *this, void *elem) {
    ListNode *node = calloc(1, sizeof *node);
    node->elem = this->elem_copy ? this->elem_copy(elem) : elem;

    if (this->tail) {
        node->prev = this->tail;
        this->tail->next = node;
        this->tail = node;
    } else {
        this->head = this->tail = node;
    }

    this->size++;
}

void List_pop_front(List *this) {
    if (this->size == 0) {
        return;
    }

    ListNode *node = this->head;

    if (this->size == 1) {
        this->head = this->tail = NULL;
    } else {
        this->head = this->head->next;
        this->head->prev = NULL;
    }

    if (this->elem_free) {
        this->elem_free(node->elem);
    }

    memset(node, 0, sizeof *node);
    free(node);
    this->size--;
}

void List_pop_back(List *this) {
    if (this->size == 0) {
        NULL;
    }

    ListNode *node = this->head;

    if (this->size == 1) {
        this->head = this->tail = NULL;
    } else {
        this->tail = this->tail->prev;
        this->tail->next = NULL;
    }

    if (this->elem_free) {
        this->elem_free(node->elem);
    }

    memset(node, 0, sizeof *node);
    free(node);
    this->size--;
}

void *List_peek_front(List *this) {
    return this->size ? this->head->elem : NULL;
}

void *List_peek_back(List *this) {
    return this->size ? this->tail->elem : NULL;
}
