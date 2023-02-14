
#pragma once
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * Struct representing a queue
 */
typedef struct EventQueue EventQueue;

/**
 * Allocate and return a pointer to a new queue (on the heap).
 *
 * 'max_size' determines how many elements the user can add to the queue before
 * it blocks.
 * If non-positive, the queue will never block upon a push (the queue does not
 * have a 'max_size').
 * Else, the queue will block if the user tries to push when the queue is full.
 */
EventQueue *EventQueue_create(ssize_t max_size);

/**
 * Destroys all queue elements by deallocating all the storage capacity
 * allocated by the 'queue'.
 */
void EventQueue_destroy(EventQueue *this);

/**
 * Adds a new 'element' to the end of the queue in constant time.
 * Note: Can be called by multiple threads.
 * Note: Blocks if the queue is full (defined by it's max_size).
 */
void EventQueue_push(EventQueue *this, void *element);

/**
 * Removes the element at the front of the queue in constant time.
 * Note: Can be called by multiple threads.
 * Note: Blocks if the queue is empty.
 */
void *EventQueue_pull(EventQueue *this);
