#include "common.h"
#include <collectc/cc_hashtable.h>
#include <collectc/cc_array.h>

int int_cmp(const void *i, const void *j){
	return *(int *) i - *(int *)j;
}

int main()
{
	log_trace("Hello %s", "world");
	log_debug("Hello %s", "world");
	log_info("Hello %s", "world");
	log_warn("Hello %s", "world");
	log_error("Hello %s", "world");
	log_fatal("Hello %s", "world");

	CC_HashTable *ht;
	cc_hashtable_new(&ht);

	int k = 1;
	int l = 2;

	cc_hashtable_add(ht, &k, "k");
	cc_hashtable_add(ht, &l, "l");

	char *kval, *lval;

	cc_hashtable_get(ht, &k, (void **) &kval);
	cc_hashtable_get(ht, &l, (void **) &lval);	

	assert(!strcmp(kval, "k"));
	assert(!strcmp(lval, "l"));

	assert(cc_hashtable_size(ht) == 2);

	CC_HashTableIter itr;
	cc_hashtable_iter_init(&itr, ht);

	TableEntry *entry;

	while (cc_hashtable_iter_next(&itr, &entry) != CC_ITER_END) 
	{
		printf("Key:%d value:%s hash:%zu\n", *(int*)entry->key, entry->value, entry->hash);
	}
	
	cc_hashtable_destroy(ht);
	
	return 0;
}
