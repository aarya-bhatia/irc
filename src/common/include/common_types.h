#pragma once
typedef int (*compare_type)(const void *, const void *);
typedef void *(*elem_copy_type)(void *elem);
typedef void (*elem_free_type)(void *elem);