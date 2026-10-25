#define _GNU_SOURCE
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void config_load_file(Config *c, FILE *f);

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (!Data || Size == 0 || Size > 65536)
        return 0;

    FILE *f = fmemopen((void *)Data, Size, "r");
    if (!f)
        return 0;

    Config c = {0};
    config_load_file(&c, f);
    fclose(f);

    /* Invariant assertions */
    if (c.idle < 10 || c.idle > 2147483) abort();
    if (c.cycle < 0 || c.cycle > 2147483) abort();
    if (c.blank < 0 || c.blank > 2147483) abort();
    if (strcmp(c.mode, "random") && strcmp(c.mode, "cycle") && strcmp(c.mode, "single")) abort();
    if (c.nchosen > MAX_SAVERS) abort();

    config_free(&c);
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
            "[screensaver]\nenabled=true\nidle=300\nmode=single\nsavers=groucho-time;\n",
            "[screensaver]\nidle=1000000000\ncycle=-50\nblank=9999999999\n",
            "# comment line\n[invalid\nsavers=a;b;c;d;e;\n",
            "",
            NULL
        };
        for (int i = 0; seeds[i]; i++) {
            LLVMFuzzerTestOneInput((const uint8_t *)seeds[i], strlen(seeds[i]));
        }
        printf("Default config_load regression seeds executed successfully.\n");
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
    printf("config_load fuzz check completed successfully.\n");
    return 0;
}
#endif
