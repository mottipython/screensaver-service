/*
 * screensaverd: starts a screensaver when the session has been idle for a while and
 * stops it when the user comes back.
 *
 * Idleness comes from the compositor, through the ext-idle-notify-v1 protocol, so
 * anything that asks the compositor to hold off (a video player, a presentation)
 * holds the screensaver off too. The daemon draws nothing and locks nothing.
 */
#define _GNU_SOURCE
#include "common.h"
#include "ext-idle-notify-v1-client-protocol.h"
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

static struct wl_display *display;
static struct wl_seat *seat;
static struct ext_idle_notifier_v1 *notifier;
static struct ext_idle_notification_v1 *notification;
static Config cfg;
static Saver savers[MAX_SAVERS];
static int nsavers;
static Running *running;
static int current = -1;         /* which screensaver is up, or was last */
static bool idle, rested;        /* rested: stopped to rest the screen, until the user comes back */
static time_t started, changed;
static bool dry_run, verbose;

static void say(const char *fmt, const char *a)
{
    if (verbose) { fprintf(stderr, NAME ": "); fprintf(stderr, fmt, a); fputc('\n', stderr); }
}

/* The next screensaver to show, from those the user chose. */
static int choose(void)
{
    int pick[MAX_SAVERS], n = 0;
    for (int i = 0; i < nsavers; i++) if (config_chose(&cfg, savers[i].id)) pick[n++] = i;
    if (!n) return -1;
    if (!strcmp(cfg.mode, "single")) return pick[0];
    if (!strcmp(cfg.mode, "cycle")) {
        for (int k = 0; k < n; k++) if (pick[k] > current) return pick[k];
        return pick[0];
    }
    int c = pick[rand() % n];
    for (int tries = 0; n > 1 && c == current && tries < 16; tries++) c = pick[rand() % n]; /* not the same twice running */
    return c;
}

static void stop(void)
{
    if (!running) return;
    say("stopping %s", current >= 0 ? savers[current].id : "?");
    saver_stop(running);
    running = NULL;
}

static void start(void)
{
    stop();
    int c = choose();
    if (c < 0) { say("%s", "no screensaver to start"); return; }
    current = c;
    say("starting %s", savers[c].id);
    running = saver_start(&savers[c], dry_run);
    changed = time(NULL);
}

static void on_idled(void *data, struct ext_idle_notification_v1 *n)
{
    (void)data; (void)n;
    idle = true; rested = false;
    started = time(NULL);
    if (cfg.enabled) start();
}

static void came_back(void)
{
    bool was_up = running != NULL || rested;
    idle = false; rested = false;
    stop();
    if (was_up && cfg.lock && !dry_run) {
        /* locking is the desktop's business: ask the session to lock itself */
        if (system("loginctl lock-session >/dev/null 2>&1") != 0) say("%s", "could not ask the session to lock");
    }
}

static void on_resumed(void *data, struct ext_idle_notification_v1 *n) { (void)data; (void)n; came_back(); }

static const struct ext_idle_notification_v1_listener notification_listener = {.idled = on_idled, .resumed = on_resumed};

static void watch(void)
{
    if (notification) { ext_idle_notification_v1_destroy(notification); notification = NULL; }
    if (!notifier || !seat) return;
    notification = ext_idle_notifier_v1_get_idle_notification(notifier, (uint32_t)cfg.idle * 1000u, seat);
    ext_idle_notification_v1_add_listener(notification, &notification_listener, NULL);
}

static void on_global(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t version)
{
    (void)data; (void)version;
    if (!strcmp(iface, wl_seat_interface.name) && !seat) seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
    else if (!strcmp(iface, ext_idle_notifier_v1_interface.name) && !notifier) notifier = wl_registry_bind(reg, name, &ext_idle_notifier_v1_interface, 1);
}
static void on_global_remove(void *data, struct wl_registry *reg, uint32_t name) { (void)data; (void)reg; (void)name; }
static const struct wl_registry_listener registry_listener = {.global = on_global, .global_remove = on_global_remove};

static void load(void)
{
    savers_free(savers, nsavers);
    config_load(&cfg);
    nsavers = savers_find(savers, MAX_SAVERS);
    current = -1;
    if (verbose) fprintf(stderr, NAME ": %d screensavers, %s, after %d s\n", nsavers, cfg.enabled ? "on" : "off", cfg.idle);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dry-run")) dry_run = verbose = true;
        else if (!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v")) verbose = true;
        else if (!strcmp(argv[i], "--version")) { puts(NAME " " VERSION); return 0; }
        else { fprintf(stderr, "usage: screensaverd [--verbose] [--dry-run] [--version]\n"); return 2; }
    }
    srand((unsigned)(time(NULL) ^ getpid()));
    display = wl_display_connect(NULL);
    if (!display) { fprintf(stderr, NAME ": no Wayland display; this service is for Wayland sessions\n"); return 1; }
    struct wl_registry *reg = wl_display_get_registry(display);
    wl_registry_add_listener(reg, &registry_listener, NULL);
    wl_display_roundtrip(display);
    if (!notifier || !seat) {
        fprintf(stderr, NAME ": this compositor does not offer ext-idle-notify-v1, so idleness cannot be seen\n");
        return 1;
    }
    load();
    watch();

    sigset_t set;
    sigemptyset(&set);
    int sigs[] = {SIGHUP, SIGUSR1, SIGUSR2, SIGTERM, SIGINT, SIGCHLD};
    for (unsigned i = 0; i < sizeof sigs / sizeof *sigs; i++) sigaddset(&set, sigs[i]);
    sigprocmask(SIG_BLOCK, &set, NULL);
    int sfd = signalfd(-1, &set, SFD_CLOEXEC);

    char *pidfile = runtime_path("screensaverd.pid");
    FILE *pf = fopen(pidfile, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }

    bool quit = false;
    while (!quit) {
        wl_display_flush(display);
        struct pollfd fds[3] = {{wl_display_get_fd(display), POLLIN, 0}, {sfd, POLLIN, 0}, {saver_x_fd(running), POLLIN, 0}};
        if (poll(fds, 3, running ? 1000 : -1) < 0 && errno != EINTR) break;
        if (fds[0].revents & (POLLERR | POLLHUP)) break; /* the compositor has gone */
        if (fds[0].revents & POLLIN) { if (wl_display_dispatch(display) < 0) break; }
        else wl_display_dispatch_pending(display);
        if (fds[1].revents & POLLIN) {
            struct signalfd_siginfo si;
            while (read(sfd, &si, sizeof si) == sizeof si) {
                if (si.ssi_signo == SIGHUP) { stop(); load(); watch(); }          /* settings changed */
                else if (si.ssi_signo == SIGUSR1) { idle = true; rested = false; started = time(NULL); start(); } /* start now */
                else if (si.ssi_signo == SIGUSR2) came_back();                     /* stop now */
                else if (si.ssi_signo == SIGTERM || si.ssi_signo == SIGINT) quit = true;
                struct pollfd again = {sfd, POLLIN, 0};
                if (poll(&again, 1, 0) <= 0) break;
            }
        }
        if (running && saver_x_input(running)) came_back();
        time_t now = time(NULL);
        if (running && !saver_alive(running)) {
            /* it left of its own accord: a standalone one does when the user touches something */
            say("%s has gone", savers[current].id);
            saver_stop(running); running = NULL;
        }
        if (idle && !rested && cfg.enabled) {
            if (cfg.blank > 0 && now - started >= cfg.blank) { say("%s", "resting the screen"); stop(); rested = true; }
            else if (running && cfg.cycle > 0 && now - changed >= cfg.cycle) start();
        }
    }
    stop();
    unlink(pidfile);
    free(pidfile);
    return 0;
}
