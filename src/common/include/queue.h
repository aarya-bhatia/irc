#pragma once

#include <pthread.h>

#include "list.h"

typedef struct queue_t {
    List *l;
    pthread_mutex_t m;
    pthread_cond_t cv;
} queue_t;

queue_t *queue_alloc();
void queue_free(queue_t *q, void (*free_callback)(void *));

void queue_init(queue_t *q);
void queue_destroy(queue_t *q, void (*free_callback)(void *));
void queue_enqueue(queue_t *q, void *data);
void *queue_dequeue(queue_t *q);