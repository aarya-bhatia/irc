#include "include/common.h"
#include "include/message.h"
#include "include/hashtable.h"

int main()
{
	HashTable *ht = calloc(1, sizeof *ht);

	ht_setup(ht, sizeof(int), sizeof(int), 16);

	{
		ht_insert(ht, &(int){1}, &(int){0});
		ht_insert(ht, &(int){2}, &(int){0});
	}

	int key = 1;
	assert(*(int *)ht_lookup(ht, &key) == 0);

	key = 2;
	assert(*(int *)ht_lookup(ht, &key) == 0);

	ht_erase(ht, &(int){1});
	ht_erase(ht, &(int){2});

	ht_destroy(ht);

	free(ht);

	return 0;
}