#pragma once

#include <sys/types.h>

#define INITIAL_CAPACITY 64

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

// forward declare
typedef struct _String String;

size_t _GetCapacity(size_t n);
void StringWrite(String *this, int fd);

/**
 * Creates a new String object with a capacity of at least n characters
 */
String *StringConstructor(size_t n);

/**
 * Creates new String with default capacity
 */
String *StringDefaultConstructor();

size_t StringSize(const String *this);
size_t StringCapacity(const String *this);

/**
 * Adds a single characters to the end of the String
 */
void StringAdd(String *this, char c);

/**
 * Adds a c string to the end of the String
 */
void StringAddCstr(String *this, char *cstr);

/**
 * Concatenates the String other to the String this
 */
void StringAppend(String *this, const String *other);

/**
 * Creates a C string from given String
 */
char *StringToCstr(String *this);

/**
 * Creates a String object from a C string
 */
String *CstrToString(char *cstr);

/**
 * Destructor to free up all memory allocated by the String including itself
 */
void StringDestructor(String *this);

/**
 * Returns a substring (C string) of given String in the range [start ... end)
 * Returns NULL if invalid range i.e. end > start
 */
char *StringSlice(String *this, size_t start, size_t end);

/**
 * To change the size of given String to specified size.
 * - If new size > old size => This function will expand the string in size and capacity and fill new bytes with 0.
 * - If new size < old size => String will destroy the additional bytes and shrink itself to given size and capacity.
 * - This function WILL change the SIZE and CAPACITY of the given String.
 * - The capacity of the string will be at least the minimum capacity, i.e. capacity >= size.
 */
void StringResize(String *this, size_t size);

/**
 * - This function will do nothing if new capacity is smaller than old capacity.
 * - It will ensure that the string has space for at least 'capacity' no. of bytes.
 * - It will NOT change the size of the string, but it MAY change its capacity.
 */
void StringReserve(String *this, size_t capacity);

/** Utilities **/

String *wrap_with_quotes(char *str);