//
//  x11.h
//  mandelbrot-fractal
//
//  Created by Rafael Eyng on 6/2/18.
//  Copyright © 2018 Rafael Eyng. All rights reserved.
//

#ifndef x11_h
#define x11_h

#import <X11/Xlib.h>
#import <X11/Xutil.h>

static Display *display;
static Window window;
static XImage *x_image;
static GC gc;

static void x11_init(int image_size) {
  int bytes_per_pixel;

  /* Attempt to open the display */
  display = XOpenDisplay(NULL);

  /* Failure */
  if (!display) exit(0);

  long white = WhitePixel(display, DefaultScreen(display));
  long black = BlackPixel(display, DefaultScreen(display));

  window = XCreateSimpleWindow(display,
                            DefaultRootWindow(display),
                            0, 0,
                            image_size, image_size,
                            0, black,
                            white);

  /* We want to be notified when the window appears */
  XSelectInput(display, window, StructureNotifyMask);

  /* Make it appear */
  XMapWindow(display, window);

  while (1) {
    XEvent e;
    XNextEvent(display, &e);
    if (e.type == MapNotify) break;
  }

  XTextProperty tp;
  char name[128] = "Mandelbrot set with pthreads";
  char *n = name;
  Status st = XStringListToTextProperty(&n, 1, &tp);
  if (st) XSetWMName(display, window, &tp);

  /* Wait for the MapNotify event */
  XFlush(display);

  int ii, jj;
  int depth = DefaultDepth(display, DefaultScreen(display));
  Visual *visual = DefaultVisual(display, DefaultScreen(display));
  int total;

  /* Determine total bytes needed for image */
  ii = 1;
  jj = (depth - 1) >> 2;
  while (jj >>= 1) ii <<= 1;

  /* Pad the scanline to a multiple of 4 bytes */
  total = image_size * ii;
  total = (total + 3) & ~3;
  total *= image_size;
  bytes_per_pixel = ii;

  x_image = XCreateImage(display, visual, depth,
                        ZPixmap, 0, malloc(total),
                        image_size, image_size, 32, 0);

  gc = XCreateGC(display, window, 0, NULL);
  XSetForeground(display, gc, black);

  XSelectInput(display, window, ExposureMask | KeyPressMask);
}

static void x11_destroy(void) {
  XDestroyImage(x_image);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
}

static void x11_put_image(int src_x, int src_y, int dest_x, int dest_y, int width, int height) {
  XPutImage(display, window, gc, x_image, src_x, src_y, dest_x, dest_y, width, height);
}

static void x11_flush() {
  XFlush(display);
}

static void x11_handle_events(int image_size) {
  while(1) {
    XEvent event;
    KeySym key;

    XNextEvent(display, &event);

    // redraw on expose (resize etc)
    if ((event.type == Expose) && !event.xexpose.count) {
      x11_put_image(0, 0, 0, 0, image_size, image_size);
    }

    // esc to close
    char key_buffer[128];
    if (event.type == KeyPress) {
      XLookupString(&event.xkey, key_buffer, sizeof key_buffer, &key, NULL);
      if (key == XK_Escape) {
        break;
      }
    }
  }

}

#endif /* x11_h */
