#pragma once
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

typedef int (*compare_type)(const void *, const void *);
typedef void *(*elem_copy_type)(void *elem);
typedef void (*elem_free_type)(void *elem);
typedef void (*elem_callback_type)(void *elem);

struct elem_type_info_t
{
	compare_type comapre_type;
	elem_copy_type copy_type;
	elem_free_type free_type;
	int elem_len;
};

extern const struct elem_type_info_t STRING_TYPE;
extern const struct elem_type_info_t INT_TYPE;
extern const struct elem_type_info_t SHALLOW_TYPE;

void *shallow_copy(void *elem);
void shallow_free(void *elem);
int shallow_compare(const void *elem1, const void *elem2);
int int_compare(const void *key1, const void *key2);
void *int_copy(void *other_int);
