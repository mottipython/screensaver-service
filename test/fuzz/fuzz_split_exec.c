#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* Prototype of the target functions */
char **split_exec(const char *cmd);
void free_argv(char **argv);

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (!Data || Size == 0 || Size > 65536)
        return 0;

    char *cmd = malloc(Size + 1);
    if (!cmd)
        return 0;
    memcpy(cmd, Data, Size);
    cmd[Size] = '\0';

    char **argv = split_exec(cmd);
    if (argv) {
        for (char **p = argv; *p; p++) {
            if (strlen(*p) > Size) {
                fprintf(stderr, "Invariant failure: token length > input size\n");
                abort();
            }
        }
        free_argv(argv);
    }

    free(cmd);
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
            "",
            "groucho-time --window",
            "saver -s 1024x768 -t 10",
            "command \"arg with spaces\" and \"quotes\"",
            "\"\" \"\"",
            "foo %o %w %h bar",
            "foo% bar",
            "echo %%100",
            "\"escaped \\\" quote\"",
            "trailing backslash \"inside quote\\",
            "   leading and trailing spaces   ",
            "\"a\\\"b\" \"c\\\\d\" %u %f %%",
            NULL
        };
        for (int i = 0; seeds[i]; i++) {
            LLVMFuzzerTestOneInput((const uint8_t *)seeds[i], strlen(seeds[i]));
        }
        printf("All default regression seeds executed successfully.\n");
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
    printf("split_exec fuzz check completed successfully.\n");
    return 0;
}
#endif
