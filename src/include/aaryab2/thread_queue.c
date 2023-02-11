
#include "thread_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

/**
 * This thread_queue is implemented with a linked list of thread_queue_nodes.
 */
typedef struct thread_queue_node
{
    void *data;
    struct thread_queue_node *next;
} thread_queue_node;

struct thread_queue
{
    /* thread_queue_node pointers to the head and tail of the thread_queue */
    thread_queue_node *head, *tail;

    /* The number of elements in the thread_queue */
    ssize_t size;

    /**
     * The maximum number of elements the thread_queue can hold.
     * max_size is non-positive if the thread_queue does not have a max size.
     */
    ssize_t max_size;

    /* Mutex and Condition Variable for thread-safety */
    pthread_cond_t cv;
    pthread_mutex_t m;
};

thread_queue *thread_queue_create(ssize_t max_size)
{
    thread_queue *q = (thread_queue *)calloc(1, sizeof(thread_queue));

    if (!q) 
    {
        return NULL;
    }

    q->max_size = max_size;

    pthread_cond_init(&q->cv, NULL);
    pthread_mutex_init(&q->m, NULL);

    return q;
}

void thread_queue_destroy(thread_queue *this)
{
    if (!this)
    {
        return;
    }

    thread_queue_node *itr = this->head;

    while (itr)
    {
        thread_queue_node *tmp = itr->next;
        free(itr);
        itr = tmp;
    }

    pthread_mutex_destroy(&this->m);
    pthread_cond_destroy(&this->cv);

    free(this);
}

void thread_queue_push(thread_queue *this, void *data)
{
    if (!this)
    {
        return;
    }

    thread_queue_node *qnode = (thread_queue_node *) malloc(sizeof(*qnode));

    if (!qnode)
    {
        return;
    }

    qnode->data = data;
    qnode->next = NULL;

    pthread_mutex_lock(&this->m);

    if (this->max_size > 0)
    {
        while (this->size >= this->max_size)
        {
            pthread_cond_wait(&this->cv, &this->m);
        }
    }

    if (!this->head && !this->tail)
    {
        this->head = qnode;
        this->tail = qnode;
    }
    else
    {
        this->tail->next = qnode;
        this->tail = qnode;
    }

    this->size++;

    pthread_cond_broadcast(&this->cv);
    pthread_mutex_unlock(&this->m);
}

void *thread_queue_pull(thread_queue *this)
{
    if (!this)
    {
        return NULL;
    }

    pthread_mutex_lock(&this->m);

    while (this->size <= 0)
    {
        pthread_cond_wait(&this->cv, &this->m);
    }

    thread_queue_node *node = this->head;

    if(this->head == this->tail) {
        this->head = NULL;
        this->tail = NULL;
    } else {
        this->head = this->head->next;
    }

    this->size--;

    pthread_cond_broadcast(&this->cv);
    pthread_mutex_unlock(&this->m);

    void *data = node->data;
    free(node);

    return data;
}