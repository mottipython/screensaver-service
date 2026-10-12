/*
 * screensaverctl: the settings and the service, from the command line.
 */
#define _GNU_SOURCE
#include "common.h"
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

static pid_t service_pid(void)
{
    char *path = runtime_path("screensaverd.pid");
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

int main(int argc, char **argv)
{
    if (argc < 2) { usage(); return 2; }
    const char *cmd = argv[1];
    static Saver savers[MAX_SAVERS];
    int n = savers_find(savers, MAX_SAVERS);
    Config c = {0};
    config_load(&c);

    if (!strcmp(cmd, "status")) {
        printf("service: %s\nscreensaver: %s\nwait: %d min\nmode: %s\nchange: %d min\nrest: %d min\nlock on return: %s\ninstalled: %d\n",
               service_pid() ? "running" : "not running", c.enabled ? "on" : "off", c.idle / 60, c.mode, c.cycle / 60, c.blank / 60, c.lock ? "yes" : "no", n);
        return 0;
    }
    if (!strcmp(cmd, "list")) {
        for (int i = 0; i < n; i++) printf("%c %-28s %s%s\n", config_chose(&c, savers[i].id) ? '*' : ' ', savers[i].id, savers[i].name, savers[i].standalone ? "" : "  (X11)");
        return 0;
    }
    if (!strcmp(cmd, "on") || !strcmp(cmd, "off")) { c.enabled = cmd[1] == 'n'; return save(&c); }
    if (!strcmp(cmd, "wait") && argc == 3) { c.idle = atoi(argv[2]) * 60; if (c.idle < 60) c.idle = 60; return save(&c); }
    if (!strcmp(cmd, "change") && argc == 3) { c.cycle = atoi(argv[2]) * 60; return save(&c); }
    if (!strcmp(cmd, "rest") && argc == 3) { c.blank = atoi(argv[2]) * 60; return save(&c); }
    if (!strcmp(cmd, "lock") && argc == 3) { c.lock = !strcmp(argv[2], "on"); return save(&c); }
    if (!strcmp(cmd, "mode") && argc == 3 && (!strcmp(argv[2], "random") || !strcmp(argv[2], "cycle") || !strcmp(argv[2], "single"))) {
        snprintf(c.mode, sizeof c.mode, "%s", argv[2]);
        return save(&c);
    }
    if (!strcmp(cmd, "use-all")) { config_free(&c); return save(&c); }
    if (!strcmp(cmd, "use") && argc >= 3) {
        config_free(&c);
        c.chosen = calloc(argc, sizeof *c.chosen);
        for (int i = 2; i < argc; i++) {
            if (!saver_by_id(savers, n, argv[i])) { fprintf(stderr, "screensaverctl: no screensaver called %s; see: screensaverctl list\n", argv[i]); return 1; }
            c.chosen[c.nchosen++] = strdup(argv[i]);
        }
        return save(&c);
    }
    if (!strcmp(cmd, "test") && argc == 3) {
        const Saver *s = saver_by_id(savers, n, argv[2]);
        if (!s) { fprintf(stderr, "screensaverctl: no screensaver called %s; see: screensaverctl list\n", argv[2]); return 1; }
        Running *r = saver_start(s, false);
        if (!r) return 1;
        signal(SIGINT, on_signal); signal(SIGTERM, on_signal); signal(SIGHUP, on_signal); /* so that it is tidied away */
        while (!interrupted && saver_alive(r) && !saver_x_input(r)) usleep(50 * 1000);
        saver_stop(r);
        return 0;
    }
    if (!strcmp(cmd, "start")) return tell(SIGUSR1, true);
    if (!strcmp(cmd, "stop")) return tell(SIGUSR2, true);
    usage();
    return 2;
}
