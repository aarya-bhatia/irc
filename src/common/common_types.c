#include "include/common_types.h"
#include <stdlib.h>
#include <string.h>

void *shallow_copy(void *elem)
{
	return elem;
}

void shallow_free(void *elem)
{
	(void)elem;
}

int shallow_compare(const void *elem1, const void *elem2)
{
	return (int) (elem1 - elem2);
}

int int_compare(const void *key1, const void *key2)
{
	return *(int *)key1 - *(int *)key2;
}

void *int_copy(void *other_int)
{
	int *this = calloc(1, sizeof *this);
	*this = *(int *)other_int;
	return this;
}

const struct elem_type_info_t STRING_TYPE = {(compare_type) strcmp, (elem_copy_type) strdup, free, -1};
const struct elem_type_info_t INT_TYPE = {int_compare, int_copy, free, sizeof(int)};
const struct elem_type_info_t SHALLOW_TYPE = {shallow_compare, shallow_copy, shallow_free, -1};
