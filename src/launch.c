/* Starting and stopping a screensaver. */
#define _GNU_SOURCE
#include "common.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct Running {
    pid_t pid[MAX_OUTPUTS];
    int npid;
    Display *dpy;                /* for an X11 hack: the windows it draws in */
    Window win[MAX_OUTPUTS];
    int nwin;
};

static int quiet_x_error(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }

/* A black, borderless, full-screen window over one monitor, with no pointer. */
static Window cover(Display *dpy, int x, int y, int w, int h)
{
    int scr = DefaultScreen(dpy);
    XSetWindowAttributes at = {0};
    at.background_pixel = BlackPixel(dpy, scr);
    at.event_mask = KeyPressMask | ButtonPressMask | PointerMotionMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, scr), x, y, w, h, 0, CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &at);
    XStoreName(dpy, win, "Screensaver");
    XClassHint cls = {(char *)"screensaverd", (char *)"Screensaver"};
    XSetClassHint(dpy, win, &cls);
    Atom state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom states[2] = {XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False), XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False)};
    XChangeProperty(dpy, win, state, XA_ATOM, 32, PropModeReplace, (unsigned char *)states, 2);
    long bypass = 1; /* let the compositor step out of the way while it is up */
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_BYPASS_COMPOSITOR", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&bypass, 1);
    XSizeHints hints = {.flags = PPosition | PSize, .x = x, .y = y, .width = w, .height = h};
    XSetWMNormalHints(dpy, win, &hints);
    char none[1] = {0};
    Pixmap blank = XCreateBitmapFromData(dpy, win, none, 1, 1);
    XColor black = {0};
    Cursor cur = XCreatePixmapCursor(dpy, blank, blank, &black, &black, 0, 0);
    XDefineCursor(dpy, win, cur);
    XFreeCursor(dpy, cur); XFreePixmap(dpy, blank);
    XMapRaised(dpy, win);
    return win;
}

static pid_t spawn(char **argv, const char *window)
{
    pid_t pid = fork();
    if (pid != 0) return pid;
    setpgid(0, 0); /* its own group, so that whatever it starts can be stopped with it */
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, NULL);
    if (window) setenv("XSCREENSAVER_WINDOW", window, 1);
    execvp(argv[0], argv);
    fprintf(stderr, NAME ": cannot run %s: %s\n", argv[0], strerror(errno));
    _exit(127);
}

Running *saver_start(const Saver *s, bool dry_run)
{
    char **argv = split_exec(s->exec);
    if (!argv || !argv[0]) { free_argv(argv); return NULL; }
    Running *r = calloc(1, sizeof *r);
    if (dry_run) { fprintf(stderr, NAME ": would start %s (%s)\n", s->id, s->exec); free_argv(argv); return r; }
    if (s->standalone) {
        pid_t pid = spawn(argv, NULL);
        if (pid > 0) r->pid[r->npid++] = pid;
    } else {
        /* An X11 hack draws in a window it is given. Under Wayland that window lives
         * in Xwayland; one is made for each monitor, with its own copy of the hack. */
        r->dpy = XOpenDisplay(NULL);
        if (!r->dpy) { fprintf(stderr, NAME ": %s needs X11 (Xwayland), which is not running\n", s->id); free(r); free_argv(argv); return NULL; }
        XSetErrorHandler(quiet_x_error);
        Window root = DefaultRootWindow(r->dpy);
        int nmon = 0;
        XRRMonitorInfo *mon = XRRGetMonitors(r->dpy, root, True, &nmon);
        if (!mon || nmon < 1) {
            r->win[r->nwin++] = cover(r->dpy, 0, 0, DisplayWidth(r->dpy, DefaultScreen(r->dpy)), DisplayHeight(r->dpy, DefaultScreen(r->dpy)));
        } else for (int i = 0; i < nmon && r->nwin < MAX_OUTPUTS; i++)
            r->win[r->nwin++] = cover(r->dpy, mon[i].x, mon[i].y, mon[i].width, mon[i].height);
        if (mon) XRRFreeMonitors(mon);
        XSync(r->dpy, False);
        for (int i = 0; i < r->nwin; i++) {
            char id[32];
            snprintf(id, sizeof id, "0x%lx", (unsigned long)r->win[i]);
            pid_t pid = spawn(argv, id);
            if (pid > 0) r->pid[r->npid++] = pid;
        }
    }
    free_argv(argv);
    if (!r->npid) { saver_stop(r); return NULL; }
    return r;
}

bool saver_alive(Running *r)
{
    bool any = false;
    for (int i = 0; i < r->npid; i++) {
        if (r->pid[i] <= 0) continue;
        if (waitpid(r->pid[i], NULL, WNOHANG) == r->pid[i]) r->pid[i] = 0; else any = true;
    }
    return any || r->npid == 0; /* a dry run counts as alive */
}

void saver_stop(Running *r)
{
    if (!r) return;
    for (int i = 0; i < r->npid; i++) if (r->pid[i] > 0) kill(-r->pid[i], SIGTERM);
    /* give them two seconds to go quietly */
    for (int tries = 0; tries < 40; tries++) {
        bool any = false;
        for (int i = 0; i < r->npid; i++) {
            if (r->pid[i] <= 0) continue;
            if (waitpid(r->pid[i], NULL, WNOHANG) == r->pid[i]) r->pid[i] = 0; else any = true;
        }
        if (!any) break;
        struct timespec ts = {0, 50 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    for (int i = 0; i < r->npid; i++) if (r->pid[i] > 0) { kill(-r->pid[i], SIGKILL); waitpid(r->pid[i], NULL, 0); }
    if (r->dpy) {
        for (int i = 0; i < r->nwin; i++) XDestroyWindow(r->dpy, r->win[i]);
        XCloseDisplay(r->dpy);
    }
    free(r);
}

int saver_x_fd(Running *r) { return r && r->dpy ? ConnectionNumber(r->dpy) : -1; }

bool saver_x_input(Running *r)
{
    bool touched = false;
    int moved = 0;
    while (r && r->dpy && XPending(r->dpy)) {
        XEvent e;
        XNextEvent(r->dpy, &e);
        if (e.type == KeyPress || e.type == ButtonPress) touched = true;
        else if (e.type == MotionNotify && ++moved > 8) touched = true; /* not just a nudge of the desk */
    }
    return touched;
}
