/* Settings and the list of installed screensavers. */
#define _GNU_SOURCE
#include "common.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *join(const char *a, const char *b)
{
    char *s;
    if (asprintf(&s, "%s/%s", a, b) < 0) abort();
    return s;
}

static char *home_dir(const char *env, const char *fallback)
{
    const char *v = getenv(env), *home = getenv("HOME");
    if (v && v[0] == '/') return strdup(v);
    return join(home ? home : "/", fallback);
}

char *config_path(void)
{
    char *dir = home_dir("XDG_CONFIG_HOME", ".config"), *p = join(dir, NAME "/config");
    free(dir);
    return p;
}

char *runtime_path(const char *leaf)
{
    const char *dir = getenv("XDG_RUNTIME_DIR");
    return join(dir && dir[0] == '/' ? dir : "/tmp", leaf);
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

void config_free(Config *c)
{
    for (int i = 0; i < c->nchosen; i++) free(c->chosen[i]);
    free(c->chosen);
    c->chosen = NULL; c->nchosen = 0;
}

static void set_chosen(Config *c, const char *list)
{
    config_free(c);
    char *copy = strdup(list), *save = NULL;
    for (char *t = strtok_r(copy, ";", &save); t; t = strtok_r(NULL, ";", &save)) {
        t = trim(t);
        if (!*t) continue;
        c->chosen = realloc(c->chosen, (c->nchosen + 1) * sizeof *c->chosen);
        c->chosen[c->nchosen++] = strdup(t);
    }
    free(copy);
}

void config_load(Config *c)
{
    config_free(c);
    c->enabled = true; c->idle = 300; c->cycle = 0; c->blank = 1800; c->lock = false;
    strcpy(c->mode, "random");
    char *path = config_path();
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line), *eq = strchr(s, '=');
        if (*s == '#' || *s == '[' || !eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (!strcmp(k, "enabled")) c->enabled = !strcmp(v, "true") || !strcmp(v, "1");
        else if (!strcmp(k, "idle")) c->idle = atoi(v);
        else if (!strcmp(k, "cycle")) c->cycle = atoi(v);
        else if (!strcmp(k, "blank")) c->blank = atoi(v);
        else if (!strcmp(k, "lock")) c->lock = !strcmp(v, "true") || !strcmp(v, "1");
        else if (!strcmp(k, "mode")) snprintf(c->mode, sizeof c->mode, "%s", v);
        else if (!strcmp(k, "savers")) set_chosen(c, v);
    }
    fclose(f);
    if (c->idle < 10) c->idle = 10;
    if (c->cycle < 0) c->cycle = 0;
    if (c->blank < 0) c->blank = 0;
    if (strcmp(c->mode, "random") && strcmp(c->mode, "cycle") && strcmp(c->mode, "single")) strcpy(c->mode, "random");
}

static int make_parents(char *path)
{
    for (char *p = path + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            if (mkdir(path, 0755) != 0 && errno != EEXIST) { *p = '/'; return -1; }
            *p = '/';
        }
    return 0;
}

int config_save(const Config *c)
{
    char *path = config_path(), *tmp;
    if (make_parents(path) != 0) { free(path); return -1; }
    if (asprintf(&tmp, "%s.new", path) < 0) abort();
    FILE *f = fopen(tmp, "w");
    if (!f) { free(path); free(tmp); return -1; }
    fprintf(f, "[screensaver]\nenabled=%s\nidle=%d\nmode=%s\ncycle=%d\nblank=%d\nlock=%s\nsavers=", c->enabled ? "true" : "false", c->idle, c->mode,
            c->cycle, c->blank, c->lock ? "true" : "false");
    for (int i = 0; i < c->nchosen; i++) fprintf(f, "%s;", c->chosen[i]);
    fputc('\n', f);
    int bad = ferror(f) | fclose(f);
    if (!bad) bad = rename(tmp, path);
    free(path); free(tmp);
    return bad ? -1 : 0;
}

bool config_chose(const Config *c, const char *id)
{
    if (!c->nchosen) return true;
    for (int i = 0; i < c->nchosen; i++) if (!strcmp(c->chosen[i], id)) return true;
    return false;
}

/* ---- the screensavers installed ---- */

static bool on_path(const char *prog)
{
    if (strchr(prog, '/')) return access(prog, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    char *copy = strdup(path), *save = NULL;
    bool found = false;
    for (char *d = strtok_r(copy, ":", &save); d && !found; d = strtok_r(NULL, ":", &save)) {
        char *p = join(d, prog);
        found = access(p, X_OK) == 0;
        free(p);
    }
    free(copy);
    return found;
}

/* One desktop entry. Returns false if it is not a screensaver that can be run here. */
static bool read_entry(const char *path, const char *id, bool standalone_dir, Saver *out)
{
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char line[4096], *name = NULL, *comment = NULL, *exec = NULL, *tryexec = NULL, *snapshot = NULL, *settings = NULL;
    bool in_main = false, hidden = false, standalone = standalone_dir;
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (*s == '[') { in_main = !strcmp(s, "[Desktop Entry]"); continue; }
        char *eq = strchr(s, '=');
        if (!in_main || *s == '#' || !eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (!strcmp(k, "Name") && !name) name = strdup(v);
        else if (!strcmp(k, "Comment") && !comment) comment = strdup(v);
        else if (!strcmp(k, "Exec") && !exec) exec = strdup(v);
        else if (!strcmp(k, "TryExec") && !tryexec) tryexec = strdup(v);
        else if (!strcmp(k, "Hidden") || !strcmp(k, "NoDisplay")) hidden |= !strcmp(v, "true");
        else if (!strcmp(k, "X-Screensaver-Standalone")) standalone = !strcmp(v, "true");
        else if (!strcmp(k, "X-Screensaver-Snapshot") && !snapshot) snapshot = strdup(v);
        else if (!strcmp(k, "X-Screensaver-Settings") && !settings) settings = strdup(v);
    }
    fclose(f);
    bool ok = exec && !hidden;
    if (ok) {
        char **argv = split_exec(exec);
        ok = argv && argv[0] && on_path(tryexec ? tryexec : argv[0]);
        free_argv(argv);
    }
    free(tryexec);
    if (!ok) { free(name); free(comment); free(exec); free(snapshot); free(settings); return false; }
    out->id = strdup(id); out->name = name ? name : strdup(id); out->comment = comment ? comment : strdup("");
    out->exec = exec; out->standalone = standalone; out->snapshot = snapshot; out->settings = settings;
    return true;
}

static int by_name(const void *a, const void *b) { return strcasecmp(((const Saver *)a)->name, ((const Saver *)b)->name); }

/* Screensavers are looked for under every XDG data directory, the user's own first:
 *   screensaver-service/savers/   standalone screensavers registered with this service
 *   applications/screensavers/    X11 hacks, where the xscreensaver packages and the
 *                                 MATE, Xfce and Cinnamon screensavers put theirs */
int savers_find(Saver *out, int max)
{
    char *dirs[64];
    int nd = 0, n = 0;
    dirs[nd++] = home_dir("XDG_DATA_HOME", ".local/share");
    const char *sys = getenv("XDG_DATA_DIRS");
    char *copy = strdup(sys && *sys ? sys : "/usr/local/share:/usr/share"), *save = NULL;
    for (char *d = strtok_r(copy, ":", &save); d && nd < 64; d = strtok_r(NULL, ":", &save)) if (d[0] == '/') dirs[nd++] = strdup(d);
    free(copy);
    static const char *SUB[2] = {NAME "/savers", "applications/screensavers"};
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < nd; i++) {
            char *dir = join(dirs[i], SUB[k]);
            DIR *dp = opendir(dir);
            struct dirent *e;
            while (dp && (e = readdir(dp)) && n < max) {
                size_t len = strlen(e->d_name);
                if (len < 9 || strcmp(e->d_name + len - 8, ".desktop")) continue;
                char *id = strndup(e->d_name, len - 8), *path = join(dir, e->d_name);
                bool seen = false; /* an entry found earlier hides a later one of the same name */
                for (int j = 0; j < n; j++) seen |= !strcmp(out[j].id, id);
                if (!seen && read_entry(path, id, k == 0, &out[n])) n++;
                free(id); free(path);
            }
            if (dp) closedir(dp);
            free(dir);
        }
    for (int i = 0; i < nd; i++) free(dirs[i]);
    qsort(out, n, sizeof *out, by_name);
    return n;
}

void savers_free(Saver *s, int n)
{
    for (int i = 0; i < n; i++) { free(s[i].id); free(s[i].name); free(s[i].comment); free(s[i].exec); free(s[i].snapshot); free(s[i].settings); }
}

const Saver *saver_by_id(const Saver *s, int n, const char *id)
{
    for (int i = 0; i < n; i++) if (!strcmp(s[i].id, id)) return &s[i];
    return NULL;
}

/* An Exec= line split into arguments the way the desktop entry specification says:
 * double quotes group, a backslash inside them escapes, and %f, %u and the other
 * field codes are dropped, since a screensaver is given no files. */
char **split_exec(const char *exec)
{
    char **argv = calloc(strlen(exec) / 2 + 2, sizeof *argv);
    int argc = 0;
    const char *p = exec;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char *arg = malloc(strlen(p) + 1), *o = arg;
        bool quoted = false, any = false;
        for (; *p && (quoted || (*p != ' ' && *p != '\t')); p++) {
            if (*p == '"') { quoted = !quoted; any = true; }
            else if (*p == '\\' && quoted && p[1]) { *o++ = *++p; }
            else if (*p == '%' && !quoted && p[1]) { if (*++p == '%') *o++ = '%'; }
            else *o++ = *p;
        }
        *o = 0;
        if (o > arg || any) argv[argc++] = arg; else free(arg);
    }
    return argv;
}

void free_argv(char **argv)
{
    if (!argv) return;
    for (int i = 0; argv[i]; i++) free(argv[i]);
    free(argv);
}
