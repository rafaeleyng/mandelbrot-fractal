#import <math.h>
#import <stdlib.h>
#import <stdio.h>
#import <unistd.h>
#import <pthread.h>
#import <err.h>

#import "colors.h"
#import "queue.h"
#import "x11-helpers.h"

//static pthread_mutex_t mutex;

//static const int MAX_THREADS = 4;
static const int PRODUCER_THREADS = 4;
// TODO aumentar
static const int MAX_ITERATIONS = 1024;
static int colors[MAX_ITERATIONS] = {0};

static queue *task_queue;
static queue *result_queue;

// TODO
const double xmin = -2.5;
const double xmax = 1.5;
const double ymin = -2;
const double ymax = 2;
const int IMAGE_SIZE = 800;
const int grain_width = 400;
const int grain_height = 400;

// qual o tamanho ocupado por um pixel, na escala do plano
const float xscal = (xmax - xmin) / IMAGE_SIZE;
const float yscal = (ymax - ymin) / IMAGE_SIZE;

//typedef struct {
//  int size;
//  int num;
//  float xmin, xmax;
//  float ymin, ymax;
//} thread_data;

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
  unsigned *pixels;
} result_data;

static int calculate_mandelbrot_iterations(float c_real, float c_imaginary) {
  float z_real = c_real;
  float z_imaginary = c_imaginary;

  int i;
  // (- 1) porque queremos que i chegue no máximo à última posição do array no valor de retorno
  for (i = 0; i < MAX_ITERATIONS - 1; i++) {
    // FOIL
    float temp_z_real = (z_real * z_real) - (z_imaginary * z_imaginary) + c_real;
    z_imaginary = (2 * z_imaginary * z_real) + c_imaginary;
    z_real = temp_z_real;

    int diverged = (z_real * z_real) + (z_imaginary * z_imaginary) > 4.0;
    if (diverged) {
      break;
    }
  }

  return i;
}

//static void *producer_old(void *data) {
//  thread_data *td = data;
//
//  // qual o tamanho ocupado por um pixel, na escala do plano
//  float xscal = (td->xmax - td->xmin) / td->size;
//  float yscal = (td->ymax - td->ymin) / td->size;
//
//  // itera intercalando as linhas nas threads. Uma vai pegar (0, 4, 8, ...), outra vai pegar (1, 5, 9, ...)
//  for (int y = td->num; y < td->size; y += MAX_THREADS) {
//    // itera colunas
//    for (int x = 0; x < td->size; x++) {
//      float c_real = td->xmin + (x * xscal);
//      float c_imaginary = td->ymin + (y * yscal);
//
//      int iterations = calculate_mandelbrot_iterations(c_real, c_imaginary);
//      int pixel_index = x + y * td->size;
//      ((unsigned *) x_image->data)[pixel_index] = colors[iterations];
//    }
//
//    pthread_mutex_lock(&mutex);
//    x11_put_image(0, y, 0, y, td->size, 1);
//    pthread_mutex_unlock(&mutex);
//  }
//
//  free(td);
//
//  return NULL;
//}
//
//static void compute_mandelbrot_set(int image_size, float xmin, float xmax, float ymin, float ymax) {
//  pthread_t threads[MAX_THREADS];
//
//  // para cada thread, gera uma tarefa
//  for (int i = 0; i < MAX_THREADS; i++) {
//    thread_data *td = malloc(sizeof(thread_data));
//
//    td->size = image_size;
//    td->num = i;
//    td->xmin = xmin;
//    td->xmax = xmax;
//    td->ymin = ymin;
//    td->ymax = ymax;
//
//    pthread_create(&threads[i], NULL, producer_old, td);
//  }
//
//  for (int i = 0; i < MAX_THREADS; i++) {
//    pthread_join(threads[i], NULL);
//  }
//
//  // TODO esse flush provavelmente sairá daqui
//  x11_flush();
//}

static int create_tasks(int image_width, int image_height) {
  const int horizontal_chunks = image_width / grain_width;
  const int vertical_chunks = image_height / grain_height;

  int tasks_created = 0;

  for(int i = 0; i < horizontal_chunks; i++) {
    for(int j = 0; j < vertical_chunks; j++) {
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
      printf("tasks queue is empty, finishing producer thread\n");
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

    int width = result->xf - result->xi + 1;
    int height = result->yf - result->yi + 1;
    result->pixels = malloc(sizeof(unsigned) * (width * height));

    for (int y = result->yi; y <= result->yf; y++) {
      for (int x = result->xi; x <= result->xf; x++) {
        float c_real = xmin + (x * xscal);
        float c_imaginary = ymin + (y * yscal);

        int iterations = calculate_mandelbrot_iterations(c_real, c_imaginary);
        int pixel_index = (x - result->xi) + ((y - result->yi) * width);
        ((unsigned *) result->pixels)[pixel_index] = colors[iterations];
      }
    }

    printf("producer: processed %d %d %d %d.\n", result->xi, result->xf, result->yi, result->yf);

    pthread_mutex_lock(result_queue->mutex);
    queue_push(result_queue, result);
    pthread_mutex_unlock(result_queue->mutex);
    pthread_cond_signal(result_queue->notEmpty);
  }

  return NULL;
}

static void *consumer(void *data) {
  consumer_data *cd = (consumer_data *) data;
  int consumed_tasks = 0;

  printf("starting consumer thread, were created %d tasks\n", cd->tasks);
  while (1) {
    if (consumed_tasks == cd->tasks) {
      printf("consumer exiting\n");
      return NULL;
    }

    pthread_mutex_lock(result_queue->mutex);
    while (result_queue->empty) {
      printf("consumer: queue EMPTY.\n");
      pthread_cond_wait(result_queue->notEmpty, result_queue->mutex);
    }

    result_data *result = malloc(sizeof(result_data));
    queue_pop(result_queue, result);
    // TODO
    //  x11_put_image(0, y, 0, y, td->size, 1);
    pthread_mutex_unlock(result_queue->mutex);
    pthread_cond_signal(result_queue->notFull); // TODO
    printf("consumer: processed %d %d %d %d.\n", result->xi, result->xf, result->yi, result->yf);
    consumed_tasks++;
  }

  return NULL;
}

int main(void) {
  printf("p1\n");

  task_queue = queue_init(sizeof(task_data));
  result_queue = queue_init(sizeof(result_data));

  int tasks_created = create_tasks(IMAGE_SIZE, IMAGE_SIZE);

  pthread_t producer_threads[PRODUCER_THREADS];
  pthread_t consumer_thread;

  for (int i = 0; i < PRODUCER_THREADS; i++) {
    pthread_create(&producer_threads[i], NULL, producer, NULL);
  }

  printf("p2\n");

  consumer_data *cd = malloc(sizeof(consumer_data));
  cd->tasks = tasks_created;
  pthread_create(&consumer_thread, NULL, consumer, cd);

  for (int i = 0; i < PRODUCER_THREADS; i++) {
    pthread_join(producer_threads[i], NULL);
  }

  pthread_join(consumer_thread, NULL);
  free(cd);

  printf("p3\n");

//  // TODO esse flush provavelmente sairá daqui
//  x11_flush();

//  task_data *task = malloc(sizeof(task_data));
//  queue_pop(task_queue, task);

//
//  colors_init(colors, MAX_ITERATIONS);
//
//  x11_init(IMAGE_SIZE);
//
//  double xmin = -2.5;
//  double xmax = 1.5;
//  double ymin = -2;
//  double ymax = 2;
//  compute_mandelbrot_set(IMAGE_SIZE, xmin, xmax, ymin, ymax);
//
//  x11_handle_events(IMAGE_SIZE);
//
//  x11_destroy();

  return 0;
}
