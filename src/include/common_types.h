#pragma once

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
