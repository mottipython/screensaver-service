#define _GNU_SOURCE
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

bool read_entry_file(FILE *f, const char *id, bool standalone_dir, Saver *out);

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (!Data || Size == 0 || Size > 65536)
        return 0;

    FILE *f = fmemopen((void *)Data, Size, "r");
    if (!f)
        return 0;

    Saver s = {0};
    bool ok = read_entry_file(f, "test-saver", true, &s);
    fclose(f);

    if (ok) {
        if (!s.name || !s.exec || !s.id) abort();
        free(s.id);
        free(s.name);
        free(s.comment);
        free(s.exec);
        free(s.snapshot);
        free(s.settings);
    }
    return 0;
}

#ifndef LIBFUZZER
static void run_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0 || sz > 1048576) {
        fclose(fp);
        return;
    }
    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return;
    }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (rd == (size_t)sz) {
        LLVMFuzzerTestOneInput(buf, (size_t)sz);
    }
    free(buf);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        const char *seeds[] = {
            "[Desktop Entry]\nType=Application\nName=Test\nExec=true\n",
            "[Desktop Entry]\nName=Test\nExec=saver --window %o -w %w -h %h\nX-Screensaver-Standalone=true\n",
            "[Desktop Entry]\nHidden=true\nExec=false\n",
            "",
            NULL
        };
        for (int i = 0; seeds[i]; i++) {
            LLVMFuzzerTestOneInput((const uint8_t *)seeds[i], strlen(seeds[i]));
        }
        printf("Default desktop_entry regression seeds executed successfully.\n");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            DIR *d = opendir(argv[i]);
            if (!d)
                continue;
            struct dirent *de;
            char path[1024];
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] == '.')
                    continue;
                snprintf(path, sizeof(path), "%s/%s", argv[i], de->d_name);
                run_file(path);
            }
            closedir(d);
        } else {
            run_file(argv[i]);
        }
    }
    printf("desktop_entry fuzz check completed successfully.\n");
    return 0;
}
#endif
