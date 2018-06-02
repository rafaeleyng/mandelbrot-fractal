//
//  queue.h
//  mandelbrot-fractal
//
//  Created by Rafael Eyng on 6/2/18.
//  Copyright © 2018 Rafael Eyng. All rights reserved.
//

#ifndef queue_h
#define queue_h

#import <string.h>

#define QUEUESIZE 100

typedef struct {
  void* data[QUEUESIZE];
  size_t memSize;
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queue_init (size_t memSize) {
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->memSize = memSize;
  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);

  return (q);
}

void queue_destroy (queue *q) {
  pthread_mutex_destroy (q->mut);
  free (q->mut);
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queue_push (queue *q, void *item) {
  q->data[q->tail] = item;
  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queue_pop (queue *q, void *item) {
  memcpy(item, q->data[q->head], q->memSize);
  
  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}
#endif /* queue_h */
