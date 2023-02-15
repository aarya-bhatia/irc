#include "common.h"
#include "message.h"
#include <collectc/cc_hashtable.h>
#include <collectc/cc_array.h>

CC_HashTable *table;

int *x,*y;

void run()
{
	x = malloc(sizeof *x);
	y = malloc(sizeof *y);
	*x = 3;
	*y = 4;
	cc_hashtable_add(table, x, x);
	cc_hashtable_add(table, y, y);
}

int main()
{
	CC_HashTableConf htc;

	// Initialize all fields to default values
	cc_hashtable_conf_init(&htc);
	htc.hash = GENERAL_HASH;
	htc.key_length = sizeof(int);
	htc.key_compare = int_compare;

	// Create a new HashTable with integer keys
	if (cc_hashtable_new_conf(&htc, &table) != CC_OK)
	{
		log_error("Failed to create hashtable");
		exit(1);
	}

	log_info("Hashtable initialized with capacity %zu", cc_hashtable_capacity(table));

	run();

	int x1 = 4, y1 = 3;
	int *out, *out2;
	cc_hashtable_get(table, &x1, &out);
	cc_hashtable_get(table, &y1, &out2);
	printf("%d %d\n", *out, *out2);

	cc_hashtable_remove(table, &x1, &out);
	free(x);

	cc_hashtable_destroy(table);
	free(y);

	return 0;
}
