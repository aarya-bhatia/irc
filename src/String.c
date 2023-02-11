#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "String.h"

/**
 * Represents a String object as a flexible array of characters
 */
struct _String
{
	char *buffer;
	size_t size;
	size_t capacity;
};

size_t _GetCapacity(size_t n)
{
	size_t c = 1;
	while (c < n)
	{
		c = c << 1;
	}
	return c;
}

void StringWrite(String *this, int fd)
{
	write(fd, this->buffer, this->size);
}

/**
 * Creates a new String object with a capacity of at least n characters
 */
String *StringConstructor(size_t n)
{
	String *this = malloc(sizeof *this);
	this->size = 0;
	this->capacity = MAX(_GetCapacity(n), INITIAL_CAPACITY);
	this->buffer = calloc(this->capacity, 1);

	return this;
}

String *StringDefaultConstructor()
{
	return StringConstructor(INITIAL_CAPACITY);
}

size_t StringSize(const String *this)
{
	return this->size;
}

size_t StringCapacity(const String *this)
{
	return this->capacity;
}

/**
 * Adds a single characters to the end of the String
 */
void StringAdd(String *this, char c)
{
	StringReserve(this, this->size + 1);
	this->buffer[this->size++] = c;
}

/**
 * Adds a c string to the end of the String
 */
void StringAddCstr(String *this, char *cstr)
{
	assert(this);
	assert(cstr);
	size_t length = strlen(cstr);
	StringReserve(this, this->size + length);
	memcpy(this->buffer + this->size, cstr, length);
	this->size += length;
}

/**
 * Concatenates the String other to the String this
 */
void StringAppend(String *this, const String *other)
{
	StringReserve(this, this->size + other->size);
	memcpy(this->buffer + this->size, other->buffer, other->size);
	this->size += other->size;
}

/**
 * Creates a C string from given String
 */
char *StringToCstr(String *this)
{
	char *cstr = malloc(this->size + 1);
	memcpy(cstr, this->buffer, this->size);
	cstr[this->size] = 0;
	return cstr;
}

/**
 * Creates a String object from a C string
 */
String *CstrToString(char *cstr)
{
	size_t length = strlen(cstr);
	String *this = StringConstructor(length);
	memcpy(this->buffer, cstr, length);
	this->size = length;
	return this;
}

/**
 * Destructor to free up all memory allocated by the String including itself
 */
void StringDestructor(String *this)
{
	if (!this)
	{
		return;
	}

	free(this->buffer);
	free(this);
}

/**
 * Returns a substring (C string) of given String in the range [start ... end)
 * Returns NULL if invalid range i.e. end < start
 */
char *StringSlice(String *this, size_t start, size_t end)
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
 * To change the size of given String to specified size.
 * - If new size > old size => This function will expand the string in size and capacity and fill new bytes with 0.
 * - If new size < old size => String will destroy the additional bytes and shrink itself to given size and capacity.
 * - This function WILL change the SIZE and CAPACITY of the given String.
 * - The capacity of the string will be at least the minimum capacity, i.e. capacity >= size.
 */
void StringResize(String *this, size_t size)
{
	if (size == this->size)
	{
		return;
	}

	this->capacity = _GetCapacity(size);
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
void StringReserve(String *this, size_t capacity)
{
	if (capacity > this->capacity)
	{
		this->capacity = _GetCapacity(capacity);
		this->buffer = realloc(this->buffer, this->capacity);
	}
}

String *wrap_with_quotes(char *str)
{
	String *s = StringDefaultConstructor();
	StringAdd(s, '"');
	StringAddCstr(s, str);
	StringAdd(s, '"');
	return s;
}