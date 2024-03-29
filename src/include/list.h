#pragma once

#include <sys/types.h>
#include <stdbool.h>

typedef struct ListNode
{
	void *elem;
	struct ListNode *next;
	struct ListNode *prev;
} ListNode;

typedef struct List
{
	ListNode *head;
	ListNode *tail;
	size_t size;
	void *(*elem_copy)(void *);
	void (*elem_free)(void *);
} List;

typedef struct ListIter
{
	ListNode *current;
} ListIter;

void List_iter_init(ListIter *iter, List *list);
bool List_iter_next(ListIter *iter, void **elem_ptr);

List *List_alloc(void *(*elem_copy)(void *), void (*elem_free)(void *));
void List_free(List *this);

void List_init(List *this, void *(*elem_copy)(void *),
			   void (*elem_free)(void *));
void List_destroy(List *this);
void List_push_front(List *this, void *elem);
void List_push_back(List *this, void *elem);
void List_pop_front(List *this);
void List_pop_back(List *this);
void *List_peek_front(List *this);
void *List_peek_back(List *this);
size_t List_size(List *this);
