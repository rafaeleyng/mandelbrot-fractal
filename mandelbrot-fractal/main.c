#import <math.h>
#import <stdlib.h>
#import <stdio.h>
#import <unistd.h>
#import <pthread.h>
#import <err.h>

#import "colors.h"
#import "queue.h"
#import "x11-helpers.h"

static const int PRODUCER_THREADS = 8;
static const int ITERATION_LIMIT = 1024;
static int colors[ITERATION_LIMIT + 1] = {0};

static queue *task_queue;
static queue *result_queue;

// TODO
float xmin = -2.5;
float xmax = 1.5;
float ymin = -2;
float ymax = 2;
const int IMAGE_SIZE = 800;

typedef struct {
  int tasks;
} consumer_data;

typedef struct {
  int xi;
  int xf;
  int yi;
  int yf;
} task_data;

typedef struct {
  int xi;
  int xf;
  int yi;
  int yf;
} result_data;

static int calculate_mandelbrot_iterations(float c_real, float c_imaginary) {
  float z_real = c_real;
  float z_imaginary = c_imaginary;

  float test_real, test_imaginary;
  unsigned test_index = 0, test_limit = 8;

  do {
    test_real = z_real;
    test_imaginary = z_imaginary;

    test_limit += test_limit;
    if (test_limit > ITERATION_LIMIT) {
      test_limit = ITERATION_LIMIT;
    }

    for (; test_index < test_limit; test_index++) {
      // FOIL
      float temp_z_real = (z_real * z_real) - (z_imaginary * z_imaginary) + c_real;
      z_imaginary = (2 * z_imaginary * z_real) + c_imaginary;
      z_real = temp_z_real;

      int diverged = (z_real * z_real) + (z_imaginary * z_imaginary) > 4.0;
      if (diverged) {
        return test_index;
      }

      if ((z_real == test_real) && (z_imaginary == test_imaginary)) {
        return ITERATION_LIMIT;
      }
    }
  } while (test_limit != ITERATION_LIMIT);

  return ITERATION_LIMIT;
}

static int create_tasks(int image_width, int image_height) {
  const int grain_width = 50;
  const int grain_height = 50;

  const int horizontal_chunks = image_width / grain_width;
  const int vertical_chunks = image_height / grain_height;

  int tasks_created = 0;

  for(int j = 0; j < vertical_chunks; j++) {
    for(int i = 0; i < horizontal_chunks; i++) {
      int xi = i * grain_width;
      int xf = ((i + 1) * grain_width) - 1;

      int yi = j * grain_height;
      int yf = ((j + 1) * grain_height) - 1;

      task_data *task = malloc(sizeof(task_data));
      task->xi = xi;
      task->xf = xf;
      task->yi = yi;
      task->yf = yf;
      queue_push(task_queue, task);
      tasks_created++;
    }
  }

  return tasks_created;
}

static void *producer(void *data) {
  while (1) {
    pthread_mutex_lock(task_queue->mutex);
    if (task_queue->empty) {
      pthread_mutex_unlock(task_queue->mutex);
      break;
    }
    task_data *task = malloc(sizeof(task_data));
    queue_pop(task_queue, task);
    pthread_mutex_unlock(task_queue->mutex);

    result_data *result = malloc(sizeof(result_data));
    result->xi = task->xi;
    result->xf = task->xf;
    result->yi = task->yi;
    result->yf = task->yf;
    free(task);

    // qual o tamanho ocupado por um pixel, na escala do plano
    const float pixel_width = (xmax - xmin) / IMAGE_SIZE;
    const float pixel_height = (ymax - ymin) / IMAGE_SIZE;

    for (int y = result->yi; y <= result->yf; y++) {
      for (int x = result->xi; x <= result->xf; x++) {
        float c_real = xmin + (x * pixel_width);
        float c_imaginary = ymin + (y * pixel_height);
        int iterations = calculate_mandelbrot_iterations(c_real, c_imaginary);
        int pixel_index = x + (y * IMAGE_SIZE);
        ((unsigned *) x_image->data)[pixel_index] = colors[iterations];
      }
    }

    pthread_mutex_lock(result_queue->mutex);
    while (result_queue->full) {
      pthread_cond_wait(result_queue->notFull, result_queue->mutex);
    }
    queue_push(result_queue, result);
    pthread_mutex_unlock(result_queue->mutex);
    pthread_cond_signal(result_queue->notEmpty);
  }

  return NULL;
}

static void *consumer(void *data) {
  consumer_data *cd = (consumer_data *) data;
  int consumed_tasks = 0;

  while (1) {
    if (consumed_tasks == cd->tasks) {
      x11_flush();
      return NULL;
    }

    pthread_mutex_lock(result_queue->mutex);
    while (result_queue->empty) {
      pthread_cond_wait(result_queue->notEmpty, result_queue->mutex);
    }

    result_data *result = malloc(sizeof(result_data));
    queue_pop(result_queue, result);
    x11_put_image(result->xi, result->yi, result->xi, result->yi, (result->xf - result->xi + 1), (result->yf - result->yi + 1));
    pthread_mutex_unlock(result_queue->mutex);
    pthread_cond_signal(result_queue->notFull);
    consumed_tasks++;
  }
}

void process_mandelbrot_set() {
  int tasks_created = create_tasks(IMAGE_SIZE, IMAGE_SIZE);

  pthread_t producer_threads[PRODUCER_THREADS];
  pthread_t consumer_thread;

  for (int i = 0; i < PRODUCER_THREADS; i++) {
    pthread_create(&producer_threads[i], NULL, producer, NULL);
  }

  consumer_data *cd = malloc(sizeof(consumer_data));
  cd->tasks = tasks_created;
  pthread_create(&consumer_thread, NULL, consumer, cd);

  for (int i = 0; i < PRODUCER_THREADS; i++) {
    pthread_join(producer_threads[i], NULL);
  }

  pthread_join(consumer_thread, NULL);
  free(cd);
}

void transform_coordinates(int xi_signal, int xf_signal, int yi_signal, int yf_signal) {
  float width = xmax - xmin;
  float height = ymax - ymin;
  xmin += width * 0.1 * xi_signal;
  xmax += width * 0.1 * xf_signal;
  ymin += height * 0.1 * yi_signal;
  ymax += height * 0.1 * yf_signal;
  process_mandelbrot_set();
}

int main(void) {
  // init
  x11_init(IMAGE_SIZE);
  colors_init(colors, ITERATION_LIMIT);
  task_queue = queue_init(300, sizeof(task_data));
  result_queue = queue_init(10, sizeof(result_data));

  // compute
  process_mandelbrot_set();

  // loop
  x11_handle_events(IMAGE_SIZE, transform_coordinates);

  // cleanup
  queue_destroy(task_queue);
  queue_destroy(result_queue);
  x11_destroy();

  return 0;
}
