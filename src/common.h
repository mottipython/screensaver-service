/* screensaver-service: a screensaver service for Wayland desktops. */
#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <sys/types.h>

#define NAME "screensaver-service"
#define MAX_SAVERS 1024
#define MAX_OUTPUTS 16

/* A screensaver the service can run, described by a desktop entry.
 *
 * Two kinds are understood. An X11 "hack" in the xscreensaver tradition draws into a
 * window it is handed (XSCREENSAVER_WINDOW); the service makes that window, one per
 * monitor. A standalone screensaver is an ordinary program that goes full screen by
 * itself and may quit when the user touches something. */
typedef struct {
    char *id;       /* the desktop file's name without ".desktop" */
    char *name;     /* Name= */
    char *comment;  /* Comment= */
    char *exec;     /* Exec= */
    bool standalone;
} Saver;

typedef struct {
    bool enabled;
    int idle;       /* seconds without input before the screensaver starts */
    int cycle;      /* seconds before changing to another; 0 = never */
    int blank;      /* seconds after which the screensaver is stopped, to let the screen rest; 0 = never */
    bool lock;      /* ask the desktop to lock the screen when the user comes back */
    char mode[16];  /* "random", "cycle" or "single" */
    char **chosen;  /* ids of the screensavers the user picked; none = all */
    int nchosen;
} Config;

/* config.c */
char *config_path(void);
void config_load(Config *c);
int config_save(const Config *c);
void config_free(Config *c);
bool config_chose(const Config *c, const char *id);
int savers_find(Saver *out, int max);       /* every screensaver installed, sorted by name */
void savers_free(Saver *s, int n);
const Saver *saver_by_id(const Saver *s, int n, const char *id);
char *runtime_path(const char *leaf);       /* a file in XDG_RUNTIME_DIR */
char **split_exec(const char *exec);        /* an Exec= line as an argv, field codes dropped */
void free_argv(char **argv);

/* launch.c */
typedef struct Running Running;
Running *saver_start(const Saver *s, bool dry_run); /* NULL if it could not be started */
void saver_stop(Running *r);
bool saver_alive(Running *r);                       /* false once every process of it has gone */
int saver_x_fd(Running *r);                         /* to poll for input on its windows, or -1 */
bool saver_x_input(Running *r);                     /* true if the user touched one of its windows */

#endif
