#include "common.h"

int int_compare(const void *key1, const void *key2)
{
    return *(int *) key1 - *(int *) key2;
}
