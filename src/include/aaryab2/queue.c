
#include "queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

/**
 * This queue is implemented with a linked list of queue_nodes.
 */
typedef struct queue_node
{
    void *data;
    struct queue_node *next;
} queue_node;

struct queue
{
    /* queue_node pointers to the head and tail of the queue */
    queue_node *head, *tail;

    /* The number of elements in the queue */
    ssize_t size;

    /**
     * The maximum number of elements the queue can hold.
     * max_size is non-positive if the queue does not have a max size.
     */
    ssize_t max_size;

    /* Mutex and Condition Variable for thread-safety */
    pthread_cond_t cv;
    pthread_mutex_t m;
};

queue *queue_create(ssize_t max_size)
{
    queue *q = (queue *)calloc(1, sizeof(queue));

    if (!q) 
    {
        return NULL;
    }

    q->max_size = max_size;

    pthread_cond_init(&q->cv, NULL);
    pthread_mutex_init(&q->m, NULL);

    return q;
}

void queue_destroy(queue *this)
{
    if (!this)
    {
        return;
    }

    queue_node *itr = this->head;

    while (itr)
    {
        queue_node *tmp = itr->next;
        free(itr);
        itr = tmp;
    }

    pthread_mutex_destroy(&this->m);
    pthread_cond_destroy(&this->cv);

    free(this);
}

void queue_push(queue *this, void *data)
{
    if (!this)
    {
        return;
    }

    queue_node *qnode = (queue_node *) malloc(sizeof(*qnode));

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

void *queue_pull(queue *this)
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

    queue_node *node = this->head;

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