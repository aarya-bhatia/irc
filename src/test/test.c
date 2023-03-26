#include <pthread.h>

#include "include/common.h"
#include "include/common_types.h"
#include "include/hashtable.h"
#include "include/list.h"
#include "include/message.h"
#include "include/queue.h"
#include "include/vector.h"

void print_string(void *s)
{
	puts(s);
}

void queue_test()
{
	queue_t *queue = calloc(1, sizeof *queue);
	queue_init(queue);
	int data[1024];
	for (int i = 0; i < 1024; i++)
	{
		data[i] = i;
		if (i % 4 == 0)
		{
			queue_enqueue(queue, data + i);
		}
	}

	queue_enqueue(queue, NULL);

	while (1)
	{
		void *elem = queue_dequeue(queue);

		if (!elem)
		{
			break;
		}

		int i = *(int *)elem;
		data[i] = 0;
	}

	queue_destroy(queue, NULL);

	for (int i = 0; i < 10; i++)
	{
		if (i % 4 == 0)
		{
			assert(data[i] == 0);
		}
		else
		{
			assert(data[i] == i);
		}
	}
	log_info("success");
}

struct s
{
	int x;
};

struct s *struct_copy(struct s *other)
{
	struct s *this = calloc(1, sizeof *this);
	memcpy(this, other, sizeof *this);
	return this;
}

void struct_free(struct s *other)
{
	free(other);
}

void print(char *key, struct s *value)
{
	printf("%s -> %d\n", key, value->x);
}

char *int_to_string(void *integer)
{
	if (!integer)
	{
		return 0;
	}
	static char s[16];
	sprintf(s, "%d", *(int *)integer);
	return s;
}

char *string_to_string(char *string)
{
	return string;
}

void hashtable_iter_test()
{
	Hashtable *this = calloc(1, sizeof *this);
	ht_init(this);
	this->key_compare = (compare_type)int_compare;
	this->key_len = sizeof(int);
	this->key_copy = (elem_copy_type)int_copy;
	this->key_free = free;
	this->value_copy = (elem_copy_type)strdup;
	this->value_free = free;

	int n = 100;
	for (int i = 0; i < n; i++)
	{
		char some_string[16];
		for (int i = 0; i < 15; i++)
		{
			some_string[i] = 'a' + rand() % 26;
		}
		some_string[15] = 0;
		ht_set(this, (void *)&i, (void *)some_string);
	}

	HashtableIter itr;
	ht_iter_init(&itr, this);

	int count = 0;
	while (ht_iter_next(&itr, NULL, NULL) == true)
	{
		count++;
	}

	assert(count == n);

	int *i = NULL;
	int sum = 0;

	ht_iter_init(&itr, this);

	while (ht_iter_next(&itr, (void **)&i, NULL) == true)
	{
		sum += *i;
	}

	assert(sum == n * (n - 1) / 2);

	ht_print(this, (char *(*)(void *))int_to_string,
			 (char *(*)(void *))string_to_string);
	ht_free(this);
}

void hashtable_test()
{
	Hashtable this;
	ht_init(&this);

	this.value_copy = (elem_copy_type)struct_copy;
	this.value_free = (elem_free_type)struct_free;

	{
		struct s s1;

		s1.x = 1;
		ht_set(&this, "hello", &s1);

		s1.x = 2;
		ht_set(&this, "world", &s1);
	}

	ht_foreach(&this, (void (*)(void *, void *))print);

	ht_remove(&this, "hello", NULL, NULL);

	assert(this.size == 1);

	struct s s1;

	s1.x = 1;
	ht_set(&this, "hello", &s1);

	assert(this.size == 2);

	struct s *s2 = ht_get(&this, "hello");
	assert(s2->x == s1.x);

	struct s s3;
	s3.x = -1;
	ht_set(&this, "hello", &s3);

	s2 = ht_get(&this, "hello");
	assert(s2->x == s3.x);

	s2 = ht_get(&this, "world");
	assert(s2->x == 2);

	ht_destroy(&this);
}

bool check_mod_3(int *key, void *val, int *arg)
{
	return *key % 3 == *arg;
}

void hashtable_test1()
{
	// stress test 1

	Hashtable *this = ht_alloc_type(INT_TYPE, SHALLOW_TYPE);

	// insert all
	for (int i = 0; i < 1000; i++)
	{
		assert(!ht_contains(this, &i));
		ht_set(this, &i, NULL);
		assert(ht_size(this) == i + 1);
		assert(ht_contains(this, &i));
	}

	assert(ht_size(this) == 1000);
	assert(ht_capacity(this) > 1000);

	// remove all
	for (int i = 0; i < 1000; i++)
	{
		assert(ht_contains(this, &i));
		assert(ht_remove(this, &i, NULL, NULL));
		assert(!ht_contains(this, &i));
		assert(ht_size(this) == 1000 - i - 1);
	}

	assert(ht_size(this) == 0);

	for (int i = 0; i < 5000; i++)
	{
		ht_set(this, &i, NULL);
	}

	assert(ht_size(this) == 5000);

	for (int i = 0; i < 1000; i++)
	{
		assert(ht_remove(this, &i, NULL, NULL));
	}

	assert(ht_size(this) == 4000);

	for (int i = 0; i < 1000; i++)
	{
		assert(!ht_remove(this, &i, NULL, NULL));
	}

	for (int i = 1000; i < 2000; i++)
	{
		ht_set(this, &i, NULL);
	}

	assert(ht_size(this) == 4000);

	for (int i = 1000; i < 5000; i += 2)
	{
		assert(ht_contains(this, &i));
		ht_set(this, &i, NULL); // update key
		assert(ht_size(this) == 4000);
	}

	ht_free(this);
}

void hashtable_test2()
{
	Hashtable *this = ht_alloc_type(INT_TYPE, SHALLOW_TYPE);

	int i;
	int n = 609;

	for (i = 0; i < n; i++)
	{
		ht_set(this, &i, NULL);
	}

	log_info("ht_size() = %zu", ht_size(this));
	log_info("ht_capacity() = %zu", ht_capacity(this));
	assert(ht_size(this) == n);

	int arg;

	for (int i = 0; i < 3; i++)
	{
		arg = i;
		log_info("Removing elements which are congruent to %d mod 3", arg);
		ht_remove_all_filter(this, (filter_type)check_mod_3, &arg);
		log_info("ht_size() = %zu", ht_size(this));
	}

	log_info("ht_size() = %zu", ht_size(this));
	assert(ht_size(this) == 0);

	ht_free(this);
}

void vector_test()
{
	Vector *this = Vector_alloc_type(10, STRING_TYPE);

	Vector_push(this, "hello");
	Vector_push(this, "world");

	assert(this->size == 2);

	Vector_foreach(this, print_string);

	assert(Vector_contains(this, "hello"));
	assert(!Vector_contains(this, "hello1"));

	Vector_remove(this, 1, NULL);
	Vector_remove(this, 0, NULL);

	assert(this->size == 0);

	Vector_push(this, "a");
	Vector_push(this, "b");
	Vector_push(this, "c");

	Vector_remove(this, 1, NULL);

	assert(this->size == 2);

	Vector_free(this);
}

void message_test()
{
	char s1[] = "NICK aarya\r\n";
	char s2[] = "USER aarya * * :Aarya Bhatia\r\n";
	char s3[] = "NICK aarya\r\nUSER aarya * * :Aarya Bhatia\r\n";

	Vector *arr = parse_all_messages(s1);
	Vector *arr2 = parse_all_messages(s2);
	Vector *arr3 = parse_all_messages(s3);

	Vector_free(arr);
	Vector_free(arr2);
	Vector_free(arr3);
}

void log_test()
{
	log_trace("Hello %s", "world");
	log_debug("Hello %s", "world");
	log_info("Hello %s", "world");
	log_warn("Hello %s", "world");
	log_error("Hello %s", "world");
	log_fatal("Hello %s", "world");
}

void read_test(const char *filename)
{
	FILE *file = fopen(filename, "r");

	if (!file)
	{
		perror("fopen");
		log_error("Failed to open file %s", filename);
		return;
	}

	Vector *lines = Vector_alloc(10, (elem_copy_type)strdup, free);

	char *line = NULL;
	size_t len = 0;
	ssize_t nread;

	// Second line contains channel topic
	while ((nread = getline(&line, &len, file)) > 0)
	{
		assert(line);
		assert(len > 0);

		size_t n = strlen(line);
		line[n - 1] = 0;

		if (strlen(line) == 0)
		{
			continue;
		}

		Vector_push(lines, line);
	}

	free(line);
	fclose(file);

	for (size_t i = 0; i < Vector_size(lines); i++)
	{
		puts(Vector_get_at(lines, i));
	}

	Vector_free(lines);
}

void line_wrap_test(size_t width)
{
	const char help_who[] =
		"The /WHO Command is used to query a list of users that match given mask.\n"
		"Example: WHO emersion ; request information on user 'emersion'.\n"
		"Example: WHO #ircv3 ; list users in the '#ircv3' channel";

	const char help_privmsg[] =
		"The /PRIVMSG command is the main way to send messages to other users.\n"
		"PRIVMSG Angel :yes I'm receiving it ! ; Command to send a message to Angel.\n"
		"PRIVMSG #bunny :Hi! I have a problem! ; Command to send a message to channel #bunny.";

	Vector *lines = text_wrap(help_who, width);
	Vector_foreach(lines, print_string);
	Vector_free(lines);

	puts("\n");

	lines = text_wrap(help_privmsg, width);
	Vector_foreach(lines, print_string);
	Vector_free(lines);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s test_case\n", *argv);
		return 1;
	}

	int test_num = atoi(argv[1]);
	char *filename;
	size_t width;

	switch (test_num)
	{
	case 1:
		message_test();
		break;
	case 2:
		vector_test();
		break;
	case 3:
		hashtable_test();
		break;
	case 4:
		hashtable_iter_test();
		break;
	case 5:
		queue_test();
		break;
	case 6:
		filename = argv[2];
		if (!filename)
		{
			return 1;
		}
		read_test(filename);
		break;
	case 7:
		width = argc < 3 ? 10 : atol(argv[2]);
		line_wrap_test(width);
		break;
	case 8:
		hashtable_test2();
		break;
	case 9:
		hashtable_test1();
		break;
	default:
		log_error("No such test case");
		break;
	}
	return 0;
}
