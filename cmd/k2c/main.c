/*
 * k2c - .kry -> C compiler. Kir is the only pipeline: every .kry parses
 * into a KirProgram (kir_parse.c) and lowers to C (k2c_lower.c). All inputs
 * are parsed first so cross-module calls resolve through one symbol table.
 */
#include "kir.h"
#include "kir_parse.h"
#include "k2c_lower.h"
#include "k2c_plan9.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    fprintf(stderr,
            "usage: k2c [--no-main] [--plan9] [--include-dir DIR] "
            "--root DIR -o DIR file.kry ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *out_dir = NULL;
    int no_main = 0;
    int plan9 = 0;
    int unresolved = 0;
    KirProgram **progs;
    K2cModuleSyms *syms;
    int file_count;
    int i;
    int first_file = 0;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(strcmp(argv[i], "--plan9") == 0) {
            plan9 = 1;
        } else if(strcmp(argv[i], "--include-dir") == 0 && i + 1 < argc) {
            k2c_plan9_add_include_dir(argv[++i]);
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
    syms = calloc((size_t)file_count, sizeof(*syms));
    if(progs == NULL || syms == NULL) {
        fprintf(stderr, "k2c: out of memory\n");
        return 1;
    }
    k2c_plan9_set_enabled(plan9);
    /* Pass 1: parse every file, build the cross-module symbol table. */
    for(i = 0; i < file_count; i++) {
        progs[i] = kir_parse_file(argv[first_file + i], root);
        if(progs[i] == NULL) {
            fprintf(stderr, "k2c: failed to parse %s\n",
                    argv[first_file + i]);
            return 1;
        }
        k2c_build_syms(progs[i], &syms[i]);
    }
    /* Pass 2: lower with full cross-module resolution. */
    for(i = 0; i < file_count; i++)
        k2c_lower(progs[i], root, out_dir, syms, file_count);
    k2c_write_project(progs, file_count, root, out_dir, no_main);
    for(i = 0; i < file_count; i++)
        KirProgramFree(progs[i]);
    free(progs);
    free(syms);
    unresolved = k2c_plan9_unresolved();
    if(plan9 && unresolved > 0) {
        fprintf(stderr,
                "k2c: --plan9 left %d __auto_type declarations unresolved; "
                "add --include-dir with the headers that declare them\n",
                unresolved);
        return 1;
    }
    return 0;
}
