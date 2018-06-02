#import <math.h>
#import <stdlib.h>
#import <stdio.h>
#import <unistd.h>
#import <pthread.h>
#import <err.h>

#import "colors.h"
#import "x11-helpers.h"

static pthread_mutex_t x_lock = PTHREAD_MUTEX_INITIALIZER;

static const int MAX_THREADS = 4;
static const int MAX_ITERATIONS = 1024;
static unsigned colors[MAX_ITERATIONS] = {0};

typedef struct thread_data thread_data;
struct thread_data {
  int size;
  int num;
  float xmin, xmax;
  float ymin, ymax;
};

static unsigned mandel_float(float cr, float ci) {
  float zr = cr, zi = ci;
  float tmp;

  unsigned i;

  for (i = 0; i < MAX_ITERATIONS - 1; i++)
  {
    tmp = zr * zr - zi * zi + cr;
    zi *= 2 * zr;
    zi += ci;
    zr = tmp;

    if (zr * zr + zi * zi > 4.0) break;
  }

  return i;
}

static void *thread_func(void *data)
{
  thread_data *td = data;

  int x, y;

  float xscal = (td->xmax - td->xmin) / td->size;
  float yscal = (td->ymax - td->ymin) / td->size;

  float cr, ci;

  unsigned counts;

  for (y = td->num; y < td->size; y += MAX_THREADS)
  {
    for (x = 0; x < td->size; x++)
    {
      cr = td->xmin + x * xscal;
      ci = td->ymin + y * yscal;

      counts = mandel_float(cr, ci);

      ((unsigned *) x_image->data)[x + y * td->size] = colors[counts];
    }

    pthread_mutex_lock(&x_lock);
    x11_put_image(0, y, 0, y, td->size, 1);
    pthread_mutex_unlock(&x_lock);
  }

  free(td);

  return NULL;
}

static void compute_mandelbrot_set(int size, float xmin, float xmax, float ymin, float ymax)
{
  int i;
  thread_data *td;

  pthread_t *threads = malloc(sizeof(pthread_t) * MAX_THREADS);
  if (!threads) errx(1, "Out of memory\n");

  for (i = 0; i < MAX_THREADS; i++)
  {
    td = malloc(sizeof(thread_data));
    if (!td) errx(1, "Out of memory\n");

    td->size = size;
    td->num = i;
    td->xmin = xmin;
    td->xmax = xmax;
    td->ymin = ymin;
    td->ymax = ymax;

    if (pthread_create(&threads[i], NULL, thread_func, td)) errx(1, "pthread_create failed\n");
  }

  for (i = 0; i < MAX_THREADS; i++)
  {
    if (pthread_join(threads[i], NULL)) errx(1, "pthread_join failed\n");
  }

  free(threads);

  // TODO esse flush provavelmente sairá daqui
  x11_flush();
}

int main(void) {
  init_colors(colors, MAX_ITERATIONS);

  const int IMAGE_SIZE = 800;
  x11_init(IMAGE_SIZE);

  double xmin = -2;
  double xmax = 1.0;
  double ymin = -1.5;
  double ymax = 1.5;
  compute_mandelbrot_set(IMAGE_SIZE, xmin, xmax, ymin, ymax);

  x11_handle_events(IMAGE_SIZE);

  x11_destroy();

  return 0;
}
