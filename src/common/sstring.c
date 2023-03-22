#include "include/sstring.h"

#include "include/common.h"

size_t _align_capacity(size_t n)
{
	size_t c = 1;
	while (c < n)
	{
		c = c << 1;
	}
	return c;
}

sstring *sstring_create1(size_t n)
{
	sstring *this = malloc(sizeof *this);
	this->size = 0;
	this->capacity = MAX(_align_capacity(n), INITIAL_CAPACITY);
	this->buffer = calloc(this->capacity, 1);

	return this;
}

sstring *sstring_create()
{
	return sstring_create1(INITIAL_CAPACITY);
}

size_t sstring_size(const sstring *this)
{
	return this->size;
}

size_t sstring_capacity(const sstring *this)
{
	return this->capacity;
}

/**
 * Adds a single characters to the end of the sstring
 */
void sstring_add_char(sstring *this, char c)
{
	sstring_reserve(this, this->size + 1);
	this->buffer[this->size++] = c;
}

/**
 * Adds a c string to the end of the sstring
 */
void sstring_add_string(sstring *this, char *cstr)
{
	assert(this);
	assert(cstr);
	size_t length = strlen(cstr);
	sstring_reserve(this, this->size + length);
	memcpy(this->buffer + this->size, cstr, length);
	this->size += length;
}

/**
 * Concatenates the sstring other to the sstring this
 */
void sstring_append(sstring *this, const sstring *other)
{
	sstring_reserve(this, this->size + other->size);
	memcpy(this->buffer + this->size, other->buffer, other->size);
	this->size += other->size;
}

/**
 * Creates a C string from given sstring
 */
char *sstring_to_cstring(sstring *this)
{
	char *cstr = malloc(this->size + 1);
	memcpy(cstr, this->buffer, this->size);
	cstr[this->size] = 0;
	return cstr;
}

/**
 * Creates a sstring object from a C string
 */
sstring *cstring_to_sstring(char *cstr)
{
	size_t length = strlen(cstr);
	sstring *this = sstring_create1(length);
	memcpy(this->buffer, cstr, length);
	this->size = length;
	return this;
}

/**
 * Destructor to free up all memory allocated by the sstring including itself
 */
void sstring_destroy(sstring *this)
{
	if (!this)
	{
		return;
	}

	free(this->buffer);
	free(this);
}

/**
 * Returns a substring (C string) of given sstring in the range [start ... end)
 * Returns NULL if invalid range i.e. end < start
 */
char *sstring_slice(sstring *this, size_t start, size_t end)
{
	if (end < start || start >= this->size || end > this->size)
	{
		return NULL;
	}

	size_t length = end - start;
	char *str = calloc(length + 1, 1);
	memcpy(str, this->buffer + start, length);
	return str;
}

/**
 * To change the size of given sstring to specified size.
 * - If new size > old size => This function will expand the string in size and capacity and fill new bytes with 0.
 * - If new size < old size => sstring will destroy the additional bytes and shrink itself to given size and capacity.
 * - This function WILL change the SIZE and CAPACITY of the given sstring.
 * - The capacity of the string will be at least the minimum capacity, i.e. capacity >= size.
 */
void sstring_resize(sstring *this, size_t size)
{
	if (size == this->size)
	{
		return;
	}

	this->capacity = _align_capacity(size);
	this->buffer = realloc(this->buffer, this->capacity);

	if (size > this->size)
	{
		// zero out the new bytes
		memset(this->buffer + this->size, 0, size - this->size);
	}
	else
	{
		// zero out the extra bytes
		memset(this->buffer + size, 0, this->size - size);
	}

	this->size = size;
}

/**
 * - This function will do nothing if new capacity is smaller than old capacity.
 * - It will ensure that the string has space for at least 'capacity' no. of bytes.
 * - It will NOT change the size of the string, but it MAY change its capacity.
 */
void sstring_reserve(sstring *this, size_t capacity)
{
	if (capacity > this->capacity)
	{
		this->capacity = _align_capacity(capacity);
		this->buffer = realloc(this->buffer, this->capacity);
	}
}
