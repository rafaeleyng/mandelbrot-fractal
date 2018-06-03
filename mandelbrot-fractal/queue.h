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

typedef struct {
  int size;
  void** data;
  size_t item_size;
  long head, tail;
  int full, empty;
  pthread_mutex_t *mutex;
  pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queue_init (unsigned size, size_t item_size) {
  queue *q = (queue *)malloc (sizeof (queue));
  q->data = malloc(sizeof(item_size) * size);
  q->size = size;
  q->item_size = item_size;
  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;

  q->mutex = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mutex, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);

  return (q);
}

void queue_destroy (queue *q) {
  pthread_mutex_destroy (q->mutex);
  free(q->mutex);
  pthread_cond_destroy (q->notFull);
  free(q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free(q->notEmpty);
  free(q->data);
  free(q);
}

void queue_push (queue *q, void *item) {
  q->data[q->tail] = item;
  q->tail++;
  if (q->tail == q->size) {
    q->tail = 0;
  }
  if (q->tail == q->head) {
    q->full = 1;
  }
  q->empty = 0;

  return;
}

void queue_pop (queue *q, void *item) {
  memcpy(item, q->data[q->head], q->item_size);

  q->head++;
  if (q->head == q->size) {
    q->head = 0;
  }
  if (q->head == q->tail) {
    q->empty = 1;
  }
  q->full = 0;

  return;
}
#endif /* queue_h */
