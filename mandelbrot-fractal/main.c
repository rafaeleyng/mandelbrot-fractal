#import <math.h>
#import <stdlib.h>
#import <stdio.h>
#import <unistd.h>
#import <pthread.h>
#import <err.h>

#import "colors.h"
//#import "queue.h"
#import "x11-helpers.h"

static pthread_mutex_t mutex;

static const int MAX_THREADS = 4;
static const int MAX_ITERATIONS = 1024;
static int colors[MAX_ITERATIONS] = {0};

//queue *task_queue;
//queue *result_queue;

typedef struct {
  int size;
  int num;
  float xmin, xmax;
  float ymin, ymax;
} task_data;

//typedef struct {
//  int size2;
//  int type2;
//} result_data;

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

static void *producer(void *data) {
  task_data *td = data;

  // qual o tamanho ocupado por um pixel, na escala do plano
  float xscal = (td->xmax - td->xmin) / td->size;
  float yscal = (td->ymax - td->ymin) / td->size;

  // itera intercalando as linhas nas threads. Uma vai pegar (0, 4, 8, ...), outra vai pegar (1, 5, 9, ...)
  for (int y = td->num; y < td->size; y += MAX_THREADS) {
    // itera colunas
    for (int x = 0; x < td->size; x++) {
      float c_real = td->xmin + (x * xscal);
      float c_imaginary = td->ymin + (y * yscal);

      int iterations = calculate_mandelbrot_iterations(c_real, c_imaginary);
      int pixel_index = x + y * td->size;
      ((unsigned *) x_image->data)[pixel_index] = colors[iterations];
    }

    pthread_mutex_lock(&mutex);
    x11_put_image(0, y, 0, y, td->size, 1);
    pthread_mutex_unlock(&mutex);
  }

  free(td);

  return NULL;
}

static void compute_mandelbrot_set(int image_size, float xmin, float xmax, float ymin, float ymax) {
  pthread_t threads[MAX_THREADS];

  // para cada thread, gera uma tarefa
  for (int i = 0; i < MAX_THREADS; i++) {
    task_data *td = malloc(sizeof(task_data));

    td->size = image_size;
    td->num = i;
    td->xmin = xmin;
    td->xmax = xmax;
    td->ymin = ymin;
    td->ymax = ymax;

    pthread_create(&threads[i], NULL, producer, td);
  }

  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // TODO esse flush provavelmente sairá daqui
  x11_flush();
}

int main(void) {
//  task_queue = queue_init(sizeof(task_data));
//
//  task_data *taskData = malloc(sizeof(task_data));
//  taskData->size = 1;
//  taskData->type = 2;
//  queue_push(task_queue, taskData);
//  task_data *got1 = malloc(sizeof(task_data));
//  queue_pop(task_queue, got1);

  pthread_mutex_init(&mutex, NULL);

  colors_init(colors, MAX_ITERATIONS);

  const int IMAGE_SIZE = 800;
  x11_init(IMAGE_SIZE);

  double xmin = -2.5;
  double xmax = 1.5;
  double ymin = -2;
  double ymax = 2;
  compute_mandelbrot_set(IMAGE_SIZE, xmin, xmax, ymin, ymax);

  x11_handle_events(IMAGE_SIZE);

  x11_destroy();

  return 0;
}
