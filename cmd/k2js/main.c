/*
 * k2js - .kry -> JavaScript compiler. Shares the Kir frontend with k2c/k2g/k2b:
 * every .kry parses into a KirProgram (kir_parse.c), then lowers to ESM that
 * calls the web Kryon runtime.
 *
 * usage: k2js [--no-main] [--runtime PATH] --root DIR -o DIR file.kry ...
 */
#include "kir.h"
#include "kir_parse.h"
#include "k2js_lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    fprintf(stderr,
            "usage: k2js [--no-main] [--runtime PATH] "
            "--root DIR -o DIR file.kry ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *out_dir = NULL;
    const char *runtime_import = NULL;
    int no_main = 0;
    KirProgram **progs;
    int file_count;
    int i;
    int first_file = 0;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if(strcmp(argv[i], "--runtime") == 0 && i + 1 < argc) {
            runtime_import = argv[++i];
        } else if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else {
            first_file = i;
            break;
        }
    }
    if(root == NULL || out_dir == NULL || first_file == 0) {
        usage();
        return 1;
    }
    file_count = argc - first_file;
    progs = calloc((size_t)file_count, sizeof(*progs));
    if(progs == NULL) {
        fprintf(stderr, "k2js: out of memory\n");
        return 1;
    }
    for(i = 0; i < file_count; i++) {
        progs[i] = kir_parse_file(argv[first_file + i], root);
        if(progs[i] == NULL) {
            fprintf(stderr, "k2js: failed to parse %s\n", argv[first_file + i]);
            free(progs);
            return 1;
        }
    }
    if(k2js_lower((const KirProgram *const *)progs, file_count, root, out_dir,
                  runtime_import, no_main) != 0) {
        for(i = 0; i < file_count; i++)
            KirProgramFree(progs[i]);
        free(progs);
        return 1;
    }
    for(i = 0; i < file_count; i++)
        KirProgramFree(progs[i]);
    free(progs);
    return 0;
}
