
#include "EventQueue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

/**
 * This EventQueue is implemented with a linked list of EventQueue_nodes.
 */
typedef struct EventQueue_node
{
    void *data;
    struct EventQueue_node *next;
} EventQueue_node;

struct EventQueue
{
    /* EventQueue_node pointers to the head and tail of the EventQueue */
    EventQueue_node *head, *tail;

    /* The number of elements in the EventQueue */
    ssize_t size;

    /**
     * The maximum number of elements the EventQueue can hold.
     * max_size is non-positive if the EventQueue does not have a max size.
     */
    ssize_t max_size;

    /* Mutex and Condition Variable for thread-safety */
    pthread_cond_t cv;
    pthread_mutex_t m;
};

EventQueue *EventQueue_create(ssize_t max_size)
{
    EventQueue *q = (EventQueue *)calloc(1, sizeof(EventQueue));

    if (!q) 
    {
        return NULL;
    }

    q->max_size = max_size;

    pthread_cond_init(&q->cv, NULL);
    pthread_mutex_init(&q->m, NULL);

    return q;
}

void EventQueue_destroy(EventQueue *this)
{
    if (!this)
    {
        return;
    }

    EventQueue_node *itr = this->head;

    while (itr)
    {
        EventQueue_node *tmp = itr->next;
        free(itr);
        itr = tmp;
    }

    pthread_mutex_destroy(&this->m);
    pthread_cond_destroy(&this->cv);

    free(this);
}

void EventQueue_push(EventQueue *this, void *data)
{
    if (!this)
    {
        return;
    }

    EventQueue_node *qnode = (EventQueue_node *) malloc(sizeof(*qnode));

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

void *EventQueue_pull(EventQueue *this)
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

    EventQueue_node *node = this->head;

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