#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <err.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

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

    if (bytes_per_pixel != 4)
    {
        printf("Need 32bit colour screen!\n");
        exit_x11();
        exit(0);
    }

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

static void display_float_period(int size, float xmin, float xmax, float ymin, float ymax)
{
    int x, y;

    float cr, ci;

    float xscal = (xmax - xmin) / size;
    float yscal = (ymax - ymin) / size;

    unsigned counts;

    for (y = 0; y < size; y++)
    {
        for (x = 0; x < size; x++)
        {
            cr = xmin + x * xscal;
            ci = ymin + y * yscal;

            counts = mandel_float_period(cr, ci);

            ((unsigned *) bitmap->data)[x + y*size] = cols[counts];
        }

        /* Display it */
        XPutImage(dpy, win, gc, bitmap,
                  0, y, 0, y,
                  size, 1);
    }

    XFlush(dpy);
}

/* Image size */
#define ASIZE 1000

/* Comment out this for benchmarking */
#define WAIT_EXIT

int main(void)
{
    double xmin = -2;
    double xmax = 1.0;
    double ymin = -1.5;
    double ymax = 1.5;

    /* Make a window! */
    init_x11(ASIZE);

    init_colours();

    display_float_period(ASIZE, xmin, xmax, ymin, ymax);

#ifdef WAIT_EXIT
    while(1)
    {
        XEvent event;
        KeySym key;
        char text[255];

        XNextEvent(dpy, &event);

        /* Just redraw everything on expose */
        if ((event.type == Expose) && !event.xexpose.count)
        {
            XPutImage(dpy, win, gc, bitmap,
                      0, 0, 0, 0,
                      ASIZE, ASIZE);
        }

        /* Press 'q' to quit */
        if ((event.type == KeyPress) &&
            XLookupString(&event.xkey, text, 255, &key, 0) == 1)
        {
            if (text[0] == 'q') break;
        }

        /* Or simply close the window */
        if ((event.type == ClientMessage) &&
            ((Atom) event.xclient.data.l[0] == wmDeleteMessage))
        {
            break;
        }
    }
#endif

    /* Done! */
    exit_x11();

    return 0;
}
