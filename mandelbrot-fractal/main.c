#import <math.h>
#import <stdlib.h>
#import <stdio.h>
#import <unistd.h>
#import <pthread.h>
#import <err.h>

#import <X11/Xlib.h>
#import <X11/Xutil.h>

/* X11 data */
static Display *dpy;
static Window win;
static XImage *bitmap;
static Atom wmDeleteMessage;
static GC gc;

static void exit_x11(void)
{
  XDestroyImage(bitmap);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);
}

static void init_x11(int size)
{
  int bytes_per_pixel;

  /* Attempt to open the display */
  dpy = XOpenDisplay(NULL);

  /* Failure */
  if (!dpy) exit(0);

  unsigned long white = WhitePixel(dpy,DefaultScreen(dpy));
  unsigned long black = BlackPixel(dpy,DefaultScreen(dpy));


  win = XCreateSimpleWindow(dpy,
                            DefaultRootWindow(dpy),
                            0, 0,
                            size, size,
                            0, black,
                            white);

  /* We want to be notified when the window appears */
  XSelectInput(dpy, win, StructureNotifyMask);

  /* Make it appear */
  XMapWindow(dpy, win);

  while (1)
  {
    XEvent e;
    XNextEvent(dpy, &e);
    if (e.type == MapNotify) break;
  }

  XTextProperty tp;
  char name[128] = "Mandelbrot";
  char *n = name;
  Status st = XStringListToTextProperty(&n, 1, &tp);
  if (st) XSetWMName(dpy, win, &tp);

  /* Wait for the MapNotify event */
  XFlush(dpy);

  int ii, jj;
  int depth = DefaultDepth(dpy, DefaultScreen(dpy));
  Visual *visual = DefaultVisual(dpy, DefaultScreen(dpy));
  int total;

  /* Determine total bytes needed for image */
  ii = 1;
  jj = (depth - 1) >> 2;
  while (jj >>= 1) ii <<= 1;

  /* Pad the scanline to a multiple of 4 bytes */
  total = size * ii;
  total = (total + 3) & ~3;
  total *= size;
  bytes_per_pixel = ii;

  if (bytes_per_pixel != 4)
  {
    printf("Need 32bit colour screen!");

  }

  /* Make bitmap */
  bitmap = XCreateImage(dpy, visual, depth,
                        ZPixmap, 0, malloc(total),
                        size, size, 32, 0);

  /* Init GC */
  gc = XCreateGC(dpy, win, 0, NULL);
  XSetForeground(dpy, gc, black);

  XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);

  wmDeleteMessage = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, win, &wmDeleteMessage, 1);
}

#define MAX_ITER    (1 << 14)
static unsigned cols[MAX_ITER + 1];

static void init_colours(void)
{
  int i;

  for (i = 0; i < MAX_ITER; i++)
  {
    char r = (rand() & 0xff) * MAX_ITER / (i + MAX_ITER + 1);
    char g = (rand() & 0xff) * MAX_ITER / (i + MAX_ITER + 1);
    char b = (rand() & 0xff) * MAX_ITER / (i + MAX_ITER + 1);

    cols[i] = b + 256 * g + 256 * 256 * r;
  }

  cols[MAX_ITER] = 0;
}

static unsigned mandel_float_period(float cr, float ci)
{
  float zr = cr, zi = ci;
  float tmp;

  float ckr, cki;

  unsigned p = 0, ptot = 8;

  do
  {
    ckr = zr;
    cki = zi;

    ptot += ptot;
    if (ptot > MAX_ITER) ptot = MAX_ITER;

    for (; p < ptot; p++)
    {
      tmp = zr * zr - zi * zi + cr;
      zi *= 2 * zr;
      zi += ci;
      zr = tmp;

      if (zr * zr + zi * zi > 4.0) return p;

      if ((zr == ckr) && (zi == cki)) return MAX_ITER;
    }
  }
  while (ptot != MAX_ITER);

  return MAX_ITER;
}

typedef struct thread_data thread_data;
struct thread_data
{
  int size;
  int num;
  float xmin, xmax;
  float ymin, ymax;
};

/* Change this to 1 to update the image line-by-line */
static const int show = 1;
static pthread_mutex_t x_lock = PTHREAD_MUTEX_INITIALIZER;

static const int MAX_THREADS = 4;

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

      counts = mandel_float_period(cr, ci);

      ((unsigned *) bitmap->data)[x + y * td->size] = cols[counts];
    }

    if (show)
    {
      pthread_mutex_lock(&x_lock);
      /* Display it */
      XPutImage(dpy, win, gc, bitmap,
                0, y, 0, y,
                td->size, 1);
      pthread_mutex_unlock(&x_lock);
    }
  }

  free(td);

  return NULL;
}

static void display_mandelbrot_set(int size, float xmin, float xmax, float ymin, float ymax)
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

  if (!show)
  {
    XPutImage(dpy, win, gc, bitmap,
              0, 0, 0, 0,
              size, size);
  }

  XFlush(dpy);
}

/* Image size */
#define ASIZE 800

int main(void) {
  double xmin = -2;
  double xmax = 1.0;
  double ymin = -1.5;
  double ymax = 1.5;

  init_x11(ASIZE);

  init_colours();

  display_mandelbrot_set(ASIZE, xmin, xmax, ymin, ymax);

  while(1) {
    XEvent event;
    KeySym key;

    XNextEvent(dpy, &event);

    // redraw on expose (resize etc)
    if ((event.type == Expose) && !event.xexpose.count) {
      XPutImage(
                dpy, win, gc, bitmap,
                0, 0, 0, 0,
                ASIZE, ASIZE
                );
    }

    // esc to close
    char key_buffer[128];
    if (event.type == KeyPress) {
      XLookupString(&event.xkey, key_buffer, sizeof key_buffer, &key, NULL);
      if (key == XK_Escape) {
        break;
      }
    }

    // click on close window button
    if ((event.type == ClientMessage) && ((Atom) event.xclient.data.l[0] == wmDeleteMessage)) {
      break;
    }
  }

  exit_x11();

  return 0;
}
