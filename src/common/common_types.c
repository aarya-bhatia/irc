#include "include/common_types.h"

#include "include/common.h"

void *shallow_copy(void *elem)
{
	return elem;
}

void shallow_free(void *elem)
{
	(void)elem;
}

int shallow_compare(void *elem1, void *elem2)
{
	return elem1 - elem2;
}

const struct elem_type_info_t STRING_TYPE = {(compare_type) strcmp, (elem_copy_type) strdup, free, -1};
const struct elem_type_info_t INT_TYPE = {int_compare, int_copy, free, sizeof(int)};
const struct elem_type_info_t SHALLOW_TYPE = {shallow_compare, shallow_copy, shallow_free, -1};
