/*
 * screensaverctl: the settings and the service, from the command line.
 */
#define _GNU_SOURCE
#include "common.h"
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t interrupted;
static void on_signal(int sig) { (void)sig; interrupted = 1; }

static void usage(void)
{
    fputs("usage: screensaverctl COMMAND\n"
          "  status                 what the service is set to do\n"
          "  list                   the screensavers installed; * marks the ones in use\n"
          "  on | off               turn the screensaver on or off\n"
          "  wait MINUTES           start after this long without input\n"
          "  use NAME...            use only these screensavers\n"
          "  use-all                use every screensaver installed\n"
          "  mode random|cycle|single\n"
          "  change MINUTES         change to another screensaver this often; 0 = never\n"
          "  rest MINUTES           stop after this long, to rest the screen; 0 = never\n"
          "  lock on|off            ask the desktop to lock the screen on return\n"
          "  test NAME              run one screensaver now, until a key or click\n"
          "  start | stop           start or stop the screensaver now\n", stderr);
}

#ifndef FUZZ_OVERRIDE_CTL
static pid_t service_pid(void)
{
    char *path = runtime_path("screensaverd.pid");
    if (!path) return 0;
    FILE *f = fopen(path, "r");
    free(path);
    int pid = 0;
    if (f) { if (fscanf(f, "%d", &pid) != 1) pid = 0; fclose(f); }
    return pid > 0 && kill(pid, 0) == 0 ? pid : 0;
}

static int tell(int sig, bool must)
{
    pid_t pid = service_pid();
    if (!pid) {
        if (must) fputs("screensaverctl: the service is not running; start it with: systemctl --user enable --now screensaverd\n", stderr);
        return must ? 1 : 0;
    }
    return kill(pid, sig) == 0 ? 0 : 1;
}

static int save(Config *c)
{
    if (config_save(c) != 0) { perror("screensaverctl: cannot save the settings"); return 1; }
    return tell(SIGHUP, false);
}
#endif

int main(int argc, char **argv)
{
    if (argc < 2) { usage(); return 2; }
    const char *cmd = argv[1];
    static Saver savers[MAX_SAVERS];
    int n = savers_find(savers, MAX_SAVERS);
    Config c = {0};
    config_load(&c);

    int ret = 0;

    if (!strcmp(cmd, "status")) {
        printf("service: %s\nscreensaver: %s\nwait: %d min\nmode: %s\nchange: %d min\nrest: %d min\nlock on return: %s\ninstalled: %d\n",
               service_pid() ? "running" : "not running", c.enabled ? "on" : "off", c.idle / 60, c.mode, c.cycle / 60, c.blank / 60, c.lock ? "yes" : "no", n);
        ret = 0;
    }
    else if (!strcmp(cmd, "list")) {
        for (int i = 0; i < n; i++)
            printf("%c %-28s %s%s\n", config_chose(&c, savers[i].id) ? '*' : ' ', savers[i].id, savers[i].name, savers[i].standalone ? "" : "  (X11)");
        ret = 0;
    }
    else if (!strcmp(cmd, "on") || !strcmp(cmd, "off")) {
        c.enabled = cmd[1] == 'n';
        ret = save(&c);
    }
    else if (!strcmp(cmd, "wait")) {
        if (argc != 3) {
            fprintf(stderr, "screensaverctl: wait requires MINUTES argument\n");
            usage();
            ret = 2;
        } else {
            char *endptr = NULL;
            errno = 0;
            long min = strtol(argv[2], &endptr, 10);
            if (*endptr != '\0' || endptr == argv[2] || errno == ERANGE || min < 1 || min > (INT_MAX / 60)) {
                fprintf(stderr, "screensaverctl: invalid minutes (must be 1-%d): %s\n", INT_MAX / 60, argv[2]);
                ret = 2;
            } else {
                c.idle = (int)(min * 60);
                ret = save(&c);
            }
        }
    }
    else if (!strcmp(cmd, "change")) {
        if (argc != 3) {
            fprintf(stderr, "screensaverctl: change requires MINUTES argument\n");
            usage();
            ret = 2;
        } else {
            char *endptr = NULL;
            errno = 0;
            long min = strtol(argv[2], &endptr, 10);
            if (*endptr != '\0' || endptr == argv[2] || errno == ERANGE || min < 0 || min > (INT_MAX / 60)) {
                fprintf(stderr, "screensaverctl: invalid minutes (must be >= 0): %s\n", argv[2]);
                ret = 2;
            } else {
                c.cycle = (int)(min * 60);
                ret = save(&c);
            }
        }
    }
    else if (!strcmp(cmd, "rest")) {
        if (argc != 3) {
            fprintf(stderr, "screensaverctl: rest requires MINUTES argument\n");
            usage();
            ret = 2;
        } else {
            char *endptr = NULL;
            errno = 0;
            long min = strtol(argv[2], &endptr, 10);
            if (*endptr != '\0' || endptr == argv[2] || errno == ERANGE || min < 0 || min > (INT_MAX / 60)) {
                fprintf(stderr, "screensaverctl: invalid minutes (must be >= 0): %s\n", argv[2]);
                ret = 2;
            } else {
                c.blank = (int)(min * 60);
                ret = save(&c);
            }
        }
    }
    else if (!strcmp(cmd, "lock")) {
        if (argc != 3) {
            fprintf(stderr, "screensaverctl: lock requires 'on' or 'off'\n");
            usage();
            ret = 2;
        } else if (!strcmp(argv[2], "on")) {
            c.lock = true;
            ret = save(&c);
        } else if (!strcmp(argv[2], "off")) {
            c.lock = false;
            ret = save(&c);
        } else {
            fprintf(stderr, "screensaverctl: lock requires 'on' or 'off', got: %s\n", argv[2]);
            usage();
            ret = 2;
        }
    }
    else if (!strcmp(cmd, "mode")) {
        if (argc != 3) {
            fprintf(stderr, "screensaverctl: mode requires 'random', 'cycle' or 'single'\n");
            usage();
            ret = 2;
        } else if (strcmp(argv[2], "random") && strcmp(argv[2], "cycle") && strcmp(argv[2], "single")) {
            fprintf(stderr, "screensaverctl: unknown mode '%s'; must be random, cycle or single\n", argv[2]);
            usage();
            ret = 2;
        } else {
            snprintf(c.mode, sizeof c.mode, "%s", argv[2]);
            ret = save(&c);
        }
    }
    else if (!strcmp(cmd, "use-all")) {
        config_free(&c);
        ret = save(&c);
    }
    else if (!strcmp(cmd, "use")) {
        if (argc < 3) {
            fprintf(stderr, "screensaverctl: use requires at least one NAME\n");
            usage();
            ret = 2;
        } else {
            config_free(&c);
            c.chosen = calloc(argc, sizeof *c.chosen);
            if (!c.chosen) {
                ret = 1;
            } else {
                bool ok = true;
                for (int i = 2; i < argc; i++) {
                    if (!saver_by_id(savers, n, argv[i])) {
                        fprintf(stderr, "screensaverctl: no screensaver called %s; see: screensaverctl list\n", argv[i]);
                        ok = false;
                        break;
                    }
                    c.chosen[c.nchosen++] = strdup(argv[i]);
                }
                if (!ok) {
                    config_free(&c);
                    ret = 1;
                } else {
                    ret = save(&c);
                }
            }
        }
    }
    else if (!strcmp(cmd, "test")) {
        if (argc != 3) {
            fprintf(stderr, "screensaverctl: test requires NAME argument\n");
            usage();
            ret = 2;
        } else {
            const Saver *s = saver_by_id(savers, n, argv[2]);
            if (!s) {
                fprintf(stderr, "screensaverctl: no screensaver called %s; see: screensaverctl list\n", argv[2]);
                ret = 1;
            } else {
                Running *r = saver_start(s, false);
                if (!r) {
                    ret = 1;
                } else {
                    signal(SIGINT, on_signal);
                    signal(SIGTERM, on_signal);
                    signal(SIGHUP, on_signal);
                    while (!interrupted && saver_alive(r) && !saver_x_input(r))
                        usleep(50 * 1000);
                    saver_stop(r);
                    ret = 0;
                }
            }
        }
    }
    else if (!strcmp(cmd, "start")) {
        ret = tell(SIGUSR1, true);
    }
    else if (!strcmp(cmd, "stop")) {
        ret = tell(SIGUSR2, true);
    }
    else {
        usage();
        ret = 2;
    }

    config_free(&c);
    savers_free(savers, n);
    return ret;
}
