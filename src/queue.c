#include "include/common.h"
#include "include/queue.h"

void queue_init(queue_t *q)
{
    if(cc_list_new(&(q->l)) != CC_OK) die("cc_list_new");
    pthread_mutex_init(&q->m, NULL);
    pthread_cond_init(&q->cv, NULL);
}

void queue_destroy(queue_t *q, void (*free_callback)(void *))
{
    cc_list_destroy_cb(q->l, free_callback);
    pthread_mutex_destroy(&q->m);
    pthread_cond_destroy(&q->cv);
}

void queue_enqueue(queue_t *q, void *data)
{
    pthread_mutex_lock(&q->m);
    cc_list_add_last(q->l, data);
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->m);
}

void *queue_dequeue(queue_t *q)
{
    void *data;
    pthread_mutex_lock(&q->m);
    while (cc_list_size(q->l) == 0)
    {
        pthread_cond_wait(&q->cv, &q->m);
    }
    cc_list_remove_first(q->l, (void **)&data);
    if (cc_list_size(q->l) > 0)
    {
        pthread_cond_signal(&q->cv);
    }
    pthread_mutex_unlock(&q->m);
    return data;
}
