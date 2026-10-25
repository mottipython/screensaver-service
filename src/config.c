/* Settings and the list of installed screensavers. */
#define _GNU_SOURCE
#include "common.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *join(const char *a, const char *b)
{
    char *s = NULL;
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
    char *dir = home_dir("XDG_CONFIG_HOME", ".config");
    if (!dir) return NULL;
    char *p = join(dir, NAME "/config");
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
    if (!s) return NULL;
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

void config_free(Config *c)
{
    if (!c) return;
    if (c->chosen) {
        for (int i = 0; i < c->nchosen; i++)
            free(c->chosen[i]);
        free(c->chosen);
        c->chosen = NULL;
    }
    c->nchosen = 0;
}

static void set_chosen(Config *c, const char *list)
{
    config_free(c);
    if (!list) return;
    char *copy = strdup(list);
    if (!copy) return;
    char *save = NULL;
    int cap = 0;
    for (char *t = strtok_r(copy, ";", &save); t && c->nchosen < MAX_SAVERS; t = strtok_r(NULL, ";", &save)) {
        t = trim(t);
        if (!*t) continue;
        if (c->nchosen >= cap) {
            int new_cap = cap ? cap * 2 : 16;
            char **new_chosen = realloc(c->chosen, new_cap * sizeof(*c->chosen));
            if (!new_chosen) break;
            c->chosen = new_chosen;
            cap = new_cap;
        }
        char *entry = strdup(t);
        if (!entry) break;
        c->chosen[c->nchosen++] = entry;
    }
    free(copy);
}

static int parse_int_clamped(const char *s, int min_val, int max_val, int def_val)
{
    if (!s) return def_val;
    char *endptr = NULL;
    errno = 0;
    long val = strtol(s, &endptr, 10);
    if (endptr == s || errno == ERANGE) return def_val;
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return (int)val;
}

void config_load_file(Config *c, FILE *f)
{
    if (!c) return;
    config_free(c);
    c->enabled = true;
    c->idle = 300;
    c->cycle = 0;
    c->blank = 1800;
    c->lock = false;
    snprintf(c->mode, sizeof c->mode, "%s", "random");
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof line, f)) {
        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] != '\n' && !feof(f)) {
            int ch;
            while ((ch = fgetc(f)) != EOF && ch != '\n');
        }
        char *s = trim(line), *eq = strchr(s, '=');
        if (*s == '#' || *s == '[' || !eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (!strcmp(k, "enabled"))
            c->enabled = !strcmp(v, "true") || !strcmp(v, "1");
        else if (!strcmp(k, "idle"))
            c->idle = parse_int_clamped(v, 10, 2147483, 300);
        else if (!strcmp(k, "cycle"))
            c->cycle = parse_int_clamped(v, 0, 2147483, 0);
        else if (!strcmp(k, "blank"))
            c->blank = parse_int_clamped(v, 0, 2147483, 1800);
        else if (!strcmp(k, "lock"))
            c->lock = !strcmp(v, "true") || !strcmp(v, "1");
        else if (!strcmp(k, "mode")) {
            if (!strcmp(v, "random") || !strcmp(v, "cycle") || !strcmp(v, "single"))
                snprintf(c->mode, sizeof c->mode, "%s", v);
            else
                snprintf(c->mode, sizeof c->mode, "%s", "random");
        }
        else if (!strcmp(k, "savers"))
            set_chosen(c, v);
    }
    if (c->idle < 10) c->idle = 10;
    if (c->idle > 2147483) c->idle = 2147483;
    if (c->cycle < 0) c->cycle = 0;
    if (c->cycle > 2147483) c->cycle = 2147483;
    if (c->blank < 0) c->blank = 0;
    if (c->blank > 2147483) c->blank = 2147483;
    if (strcmp(c->mode, "random") && strcmp(c->mode, "cycle") && strcmp(c->mode, "single"))
        snprintf(c->mode, sizeof c->mode, "%s", "random");
}

void config_load(Config *c)
{
    if (!c) return;
    char *path = config_path();
    FILE *f = path ? fopen(path, "r") : NULL;
    free(path);
    config_load_file(c, f);
    if (f) fclose(f);
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
    if (!c) return -1;
    char *path = config_path(), *tmp = NULL;
    if (!path) return -1;
    if (make_parents(path) != 0) { free(path); return -1; }
    if (asprintf(&tmp, "%s.new", path) < 0) abort();
    FILE *f = fopen(tmp, "w");
    if (!f) { free(path); free(tmp); return -1; }
    fprintf(f, "[screensaver]\nenabled=%s\nidle=%d\nmode=%s\ncycle=%d\nblank=%d\nlock=%s\nsavers=",
            c->enabled ? "true" : "false", c->idle, c->mode,
            c->cycle, c->blank, c->lock ? "true" : "false");
    for (int i = 0; i < c->nchosen; i++)
        fprintf(f, "%s;", c->chosen[i]);
    fputc('\n', f);
    int bad = ferror(f) | fclose(f);
    if (!bad) bad = rename(tmp, path);
    free(path); free(tmp);
    return bad ? -1 : 0;
}

bool config_chose(const Config *c, const char *id)
{
    if (!c || !id) return false;
    if (!c->nchosen) return true;
    for (int i = 0; i < c->nchosen; i++)
        if (c->chosen[i] && !strcmp(c->chosen[i], id))
            return true;
    return false;
}

/* ---- the screensavers installed ---- */

static bool on_path(const char *prog)
{
    if (!prog || !*prog) return false;
    if (strchr(prog, '/')) return access(prog, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    char *copy = strdup(path), *save = NULL;
    if (!copy) return false;
    bool found = false;
    for (char *d = strtok_r(copy, ":", &save); d && !found; d = strtok_r(NULL, ":", &save)) {
        char *p = join(d, prog);
        if (p) {
            found = access(p, X_OK) == 0;
            free(p);
        }
    }
    free(copy);
    return found;
}

bool read_entry_file(FILE *f, const char *id, bool standalone_dir, Saver *out)
{
    if (!f || !id || !out) return false;
    memset(out, 0, sizeof(*out));
    char line[4096], *name = NULL, *comment = NULL, *exec = NULL, *tryexec = NULL, *snapshot = NULL, *settings = NULL;
    bool in_main = false, hidden = false, standalone = standalone_dir;
    while (fgets(line, sizeof line, f)) {
        size_t llen = strlen(line);
        if (llen > 0 && line[llen - 1] != '\n' && !feof(f)) {
            int ch;
            while ((ch = fgetc(f)) != EOF && ch != '\n');
        }
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
    bool ok = exec && !hidden && *exec != '\0';
    if (ok) {
        char **argv = split_exec(exec);
        ok = argv && argv[0] && on_path(tryexec ? tryexec : argv[0]);
        free_argv(argv);
    }
    free(tryexec);
    if (!ok) {
        free(name); free(comment); free(exec); free(snapshot); free(settings);
        return false;
    }
    out->id = strdup(id);
    out->name = name ? name : strdup(id);
    out->comment = comment ? comment : strdup("");
    out->exec = exec;
    out->standalone = standalone;
    out->snapshot = snapshot;
    out->settings = settings;
    return true;
}

static bool read_entry(const char *path, const char *id, bool standalone_dir, Saver *out)
{
    if (!path) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;
    bool ok = read_entry_file(f, id, standalone_dir, out);
    fclose(f);
    return ok;
}

static int by_name(const void *a, const void *b)
{
    const char *na = ((const Saver *)a)->name;
    const char *nb = ((const Saver *)b)->name;
    if (!na && !nb) return 0;
    if (!na) return -1;
    if (!nb) return 1;
    return strcasecmp(na, nb);
}

/* Screensavers are looked for under every XDG data directory, the user's own first:
 *   screensaver-service/savers/   standalone screensavers registered with this service
 *   applications/screensavers/    X11 hacks, where the xscreensaver packages and the
 *                                 MATE, Xfce and Cinnamon screensavers put theirs */
__attribute__((weak)) int savers_find(Saver *out, int max)
{
    if (!out || max <= 0) return 0;
    char *dirs[64];
    int nd = 0, n = 0;
    char *h = home_dir("XDG_DATA_HOME", ".local/share");
    if (h) dirs[nd++] = h;
    const char *sys = getenv("XDG_DATA_DIRS");
    char *copy = strdup(sys && *sys ? sys : "/usr/local/share:/usr/share");
    if (copy) {
        char *save = NULL;
        for (char *d = strtok_r(copy, ":", &save); d && nd < 64; d = strtok_r(NULL, ":", &save)) {
            if (d[0] == '/') {
                char *dup_d = strdup(d);
                if (dup_d) dirs[nd++] = dup_d;
            }
        }
        free(copy);
    }
    static const char *SUB[2] = {NAME "/savers", "applications/screensavers"};
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < nd; i++) {
            char *dir = join(dirs[i], SUB[k]);
            DIR *dp = opendir(dir);
            struct dirent *e;
            while (dp && (e = readdir(dp)) && n < max) {
                size_t len = strlen(e->d_name);
                if (len < 9 || strcmp(e->d_name + len - 8, ".desktop")) continue;
                char *id = strndup(e->d_name, len - 8);
                char *path = join(dir, e->d_name);
                bool seen = false;
                for (int j = 0; j < n; j++) {
                    if (out[j].id && !strcmp(out[j].id, id)) {
                        seen = true;
                        break;
                    }
                }
                if (!seen && read_entry(path, id, k == 0, &out[n])) n++;
                free(id); free(path);
            }
            if (dp) closedir(dp);
            free(dir);
        }
    }
    for (int i = 0; i < nd; i++) free(dirs[i]);
    qsort(out, n, sizeof *out, by_name);
    return n;
}

void savers_free(Saver *s, int n)
{
    if (!s || n <= 0) return;
    for (int i = 0; i < n; i++) {
        free(s[i].id);
        free(s[i].name);
        free(s[i].comment);
        free(s[i].exec);
        free(s[i].snapshot);
        free(s[i].settings);
        s[i].id = s[i].name = s[i].comment = s[i].exec = s[i].snapshot = s[i].settings = NULL;
    }
}

const Saver *saver_by_id(const Saver *s, int n, const char *id)
{
    if (!s || !id || n <= 0) return NULL;
    for (int i = 0; i < n; i++) {
        if (s[i].id && !strcmp(s[i].id, id))
            return &s[i];
    }
    return NULL;
}

/* An Exec= line split into arguments the way the desktop entry specification says:
 * double quotes group, a backslash inside them escapes, and %f, %u and the other
 * field codes are dropped, since a screensaver is given no files. */
char **split_exec(const char *cmd)
{
    if (!cmd)
        return NULL;

    size_t capacity = 8;
    char **argv = malloc(capacity * sizeof(char *));
    if (!argv)
        return NULL;
    size_t argc = 0;

    const char *p = cmd;

    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;

        size_t rem = strlen(p);
        char *buf = malloc(rem + 1);
        if (!buf) {
            argv[argc] = NULL;
            free_argv(argv);
            return NULL;
        }

        char *dst = buf;
        int in_quote = 0;
        int was_quoted = 0;

        while (*p) {
            if (in_quote) {
                if (*p == '"') {
                    in_quote ^= 1;
                    was_quoted = 1;
                    p++;
                } else if (*p == '\\') {
                    if (p[1] != '\0') {
                        *dst++ = p[1];
                        p += 2;
                    } else {
                        *dst++ = '\\';
                        p++;
                        break;
                    }
                } else {
                    *dst++ = *p++;
                }
            } else {
                if (*p == ' ' || *p == '\t') {
                    break;
                } else if (*p == '"') {
                    in_quote ^= 1;
                    was_quoted = 1;
                    p++;
                } else if (*p == '%') {
                    if (p[1] == '\0') {
                        *dst++ = '%';
                        p++;
                        break;
                    } else if (p[1] == '%') {
                        *dst++ = '%';
                        p += 2;
                    } else if (isalnum((unsigned char)p[1])) {
                        /* XDG field code (e.g. %o, %w, %h, %f, %u) -> strip */
                        p += 2;
                    } else {
                        /* Non-alphanumeric after %: preserve % as literal without skipping */
                        *dst++ = '%';
                        p++;
                    }
                } else {
                    *dst++ = *p++;
                }
            }
        }

        *dst = '\0';
        if (dst > buf || was_quoted) {
            size_t tok_len = (size_t)(dst - buf);
            char *shrunk = realloc(buf, tok_len + 1);
            if (shrunk)
                buf = shrunk;

            if (argc + 1 >= capacity) {
                size_t new_cap = capacity * 2;
                char **new_argv = realloc(argv, new_cap * sizeof(char *));
                if (!new_argv) {
                    free(buf);
                    argv[argc] = NULL;
                    free_argv(argv);
                    return NULL;
                }
                argv = new_argv;
                capacity = new_cap;
            }
            argv[argc++] = buf;
        } else {
            free(buf);
        }
    }

    argv[argc] = NULL;
    return argv;
}

void free_argv(char **argv)
{
    if (!argv)
        return;
    for (char **p = argv; *p; p++)
        free(*p);
    free(argv);
}
