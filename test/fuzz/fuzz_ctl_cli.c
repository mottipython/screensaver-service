#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include "common.h"

/* Forward declaration / Mock structures */
static Saver g_mock_savers[] = {
    {"duck-soup", "Duck Soup", "Duck Soup Cartoon", "duck-soup --window", "", "", true},
    {"groucho-time", "Groucho Time", "Groucho Time Clock", "groucho-time --window", "", "", true},
    {"analemma", "Analemma", "Analemma Solar Saver", "analemma --window", "", "", true},
};
static const int g_mock_nsavers = sizeof(g_mock_savers) / sizeof(g_mock_savers[0]);

/* Mock implementations of screensaverctl dependencies */
#define FUZZ_OVERRIDE_CTL

static int save(Config *c) {
    (void)c;
    return 0;
}

static int tell(int sig, bool must) {
    (void)sig;
    (void)must;
    return 0;
}

static pid_t service_pid(void) {
    return 12345;
}

int savers_find(Saver *out, int max) {
    if (!out || max <= 0) return 0;
    int n = g_mock_nsavers < max ? g_mock_nsavers : max;
    for (int i = 0; i < n; i++) {
        out[i].id = strdup(g_mock_savers[i].id);
        out[i].name = strdup(g_mock_savers[i].name);
        out[i].comment = strdup(g_mock_savers[i].comment);
        out[i].exec = strdup(g_mock_savers[i].exec);
        out[i].snapshot = strdup("");
        out[i].settings = strdup("");
        out[i].standalone = g_mock_savers[i].standalone;
    }
    return n;
}

Running *saver_start(const Saver *s, bool dry_run) {
    (void)s;
    (void)dry_run;
    return NULL;
}

#define main screensaverctl_main_orig
#include "../../src/ctl.c"
#undef main

/* Target entry point conforming to libFuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (!Data || Size == 0 || Size > 4096) return 0;

    char *buf = malloc(Size + 1);
    if (!buf) return 0;
    memcpy(buf, Data, Size);
    buf[Size] = '\0';

    char *argv[64];
    argv[0] = "screensaverctl";
    int argc = 1;

    char *p = buf;
    while ((size_t)(p - buf) < Size && argc < 63) {
        argv[argc++] = p;
        p += strlen(p) + 1;
    }
    argv[argc] = NULL;

    screensaverctl_main_orig(argc, argv);

    free(buf);
    return 0;
}

#ifndef LIBFUZZER
static void run_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len > 0 && len <= 4096) {
        uint8_t *data = malloc(len);
        if (data && fread(data, 1, len, f) == (size_t)len) {
            LLVMFuzzerTestOneInput(data, len);
        }
        free(data);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <corpus_file_or_dir>...\n", argv[0]);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            DIR *d = opendir(argv[i]);
            if (!d) continue;
            struct dirent *de;
            char path[1024];
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] == '.') continue;
                snprintf(path, sizeof(path), "%s/%s", argv[i], de->d_name);
                run_file(path);
            }
            closedir(d);
        } else {
            run_file(argv[i]);
        }
    }
    printf("screensaverctl fuzz check completed successfully.\n");
    return 0;
}
#endif
