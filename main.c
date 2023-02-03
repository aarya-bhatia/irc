#include <collectc/cc_array.h>
#include <log.h>

int main(int argc, char *argv[])
{
	printf("Aarya Bhatia\n");	

	CC_Array *ar;
	cc_array_new(&ar);
	cc_array_add(ar,"hello");
	cc_array_add(ar,"world");
	cc_array_add(ar,NULL);
	
	char *str;

	for(size_t i = 0; i < cc_array_size(ar); i++) {
		cc_array_get_at(ar, i, (void **) &str);
		log_info("%d,%s",i,str);
	}

	return 0;
}

