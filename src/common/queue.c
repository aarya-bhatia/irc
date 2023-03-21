#include "include/queue.h"

#include "include/common.h"
#include "include/list.h"

queue_t *queue_alloc()
{
    queue_t *this = calloc(1, sizeof *this);
    queue_init(this);
    return this;
}

void queue_free(queue_t * q, void (*free_callback)(void *))
{
    queue_destroy(q, free_callback);
    free(q);
}

void queue_init(queue_t * q)
{
    q->l = List_alloc(NULL, NULL);
    pthread_mutex_init(&q->m, NULL);
    pthread_cond_init(&q->cv, NULL);
}

void queue_destroy(queue_t * q, void (*free_callback)(void *))
{
    for(ListNode * itr = q->l->head; itr != NULL; itr = itr->next) {
	if (free_callback) {
	    free_callback(itr->elem);
	}

	itr->elem = NULL;
    }

    List_free(q->l);

    pthread_mutex_destroy(&q->m);
    pthread_cond_destroy(&q->cv);
}

void queue_enqueue(queue_t * q, void *data)
{
    pthread_mutex_lock(&q->m);
    List_push_back(q->l, data);
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->m);
}

void *queue_dequeue(queue_t * q)
{
    pthread_mutex_lock(&q->m);
    while (List_size(q->l) == 0) {
	pthread_cond_wait(&q->cv, &q->m);
    }
    void *data = List_peek_front(q->l);
    List_pop_front(q->l);
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->m);
    return data;
}
