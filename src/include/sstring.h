#pragma once

#include <sys/types.h>

#define INITIAL_CAPACITY 64

/**
 * Represents a String object as an array of characters
 */
typedef struct _sstring
{
	char *buffer;
	size_t size;
	size_t capacity;
} sstring;

/**
 * Creates a new sstring object with a capacity of at least n characters
 */
sstring *sstring_create1(size_t n);

/**
 * Creates new sstring with default capacity
 */
sstring *sstring_create();

size_t sstring_size(const sstring *this);

size_t sstring_capacity(const sstring *this);

/**
 * Adds a single characters to the end of the sstring
 */
void sstring_add_char(sstring *this, char c);

/**
 * Adds a c string to the end of the sstring
 */
void sstring_add_string(sstring *this, char *cstr);

/**
 * Concatenates the sstring other to the sstring this
 */
void sstring_append(sstring *this, const sstring *other);

/**
 * Creates a C string from given sstring
 */
char *sstring_to_cstring(sstring *this);

/**
 * Creates a sstring object from a C string
 */
sstring *cstring_to_sstring(char *cstr);

/**
 * Destructor to free up all memory allocated by the sstring including itself
 */
void sstring_destro(sstring *this);

/**
 * Returns a substring (C string) of given sstring in the range [start ... end)
 * Returns NULL if invalid range i.e. end > start
 */
char *sstring_slice(sstring *this, size_t start, size_t end);

/**
 * To change the size of given sstring to specified size.
 * - If new size > old size => This function will expand the string in size and capacity and fill new bytes with 0.
 * - If new size < old size => sstring will destroy the additional bytes and shrink itself to given size and capacity.
 * - This function WILL change the SIZE and CAPACITY of the given sstring.
 * - The capacity of the string will be at least the minimum capacity, i.e. capacity >= size.
 */
void sstring_resize(sstring *this, size_t size);

/**
 * - This function will do nothing if new capacity is smaller than old capacity.
 * - It will ensure that the string has space for at least 'capacity' no. of bytes.
 * - It will NOT change the size of the string, but it MAY change its capacity.
 */
void sstring_reserve(sstring *this, size_t capacity);
