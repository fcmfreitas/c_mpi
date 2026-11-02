#define _POSIX_C_SOURCE 200809L

#include "julia_viewer.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

void viewer_next_c(double *c_re, double *c_im) {
    double angle = atan2(*c_im, *c_re) + 0.005, radius = hypot(*c_re, *c_im);
    *c_re = radius * cos(angle);
    *c_im = radius * sin(angle);
}

#ifndef NO_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>

struct Viewer { Display *display; Window window; XImage *image; };

Viewer *viewer_open(uint32_t *pixels, const Options *o, const char *title) {
    Viewer *v = malloc(sizeof(*v));
    if (v == NULL) return NULL;
    v->display = XOpenDisplay(NULL);
    if (v->display == NULL) {
        free(v);
        return NULL;
    }
    int screen = DefaultScreen(v->display);
    v->window = XCreateSimpleWindow(v->display, RootWindow(v->display, screen), 0, 0, o->width, o->height, 1,
                                    BlackPixel(v->display, screen), WhitePixel(v->display, screen));
    XStoreName(v->display, v->window, title);
    XSelectInput(v->display, v->window, ExposureMask | KeyPressMask);
    XMapWindow(v->display, v->window);
    v->image = XCreateImage(v->display, DefaultVisual(v->display, screen), DefaultDepth(v->display, screen), ZPixmap,
                            0, (char *)pixels, o->width, o->height, 32, 0);
    return v;
}

bool viewer_show(Viewer *v) {
    XPutImage(v->display, v->window, DefaultGC(v->display, DefaultScreen(v->display)), v->image, 0, 0, 0, 0,
              v->image->width, v->image->height);
    XFlush(v->display);
    nanosleep(&(struct timespec){.tv_nsec = 33000000}, NULL);
    bool running = true;
    while (XPending(v->display) > 0) {
        XEvent event;
        XNextEvent(v->display, &event);
        if (event.type == KeyPress) running = false;
    }
    return running;
}

void viewer_close(Viewer *v) {
    if (v == NULL) return;
    v->image->data = NULL; /* os pixels sao do programa, nao do XImage */
    XDestroyImage(v->image);
    XDestroyWindow(v->display, v->window);
    XCloseDisplay(v->display);
    free(v);
}
#else
Viewer *viewer_open(uint32_t *pixels, const Options *o, const char *title) {
    (void)pixels; (void)o; (void)title;
    return NULL;
}
bool viewer_show(Viewer *v) { (void)v; return false; }
void viewer_close(Viewer *v) { (void)v; }
#endif
