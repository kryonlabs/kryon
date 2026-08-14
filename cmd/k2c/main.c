/*
 * k2c - .kry -> C compiler. Kir is the only pipeline: every .kry parses
 * into a KirProgram (kir_parse.c) and lowers to C (k2c_lower.c).
 */
#include "kir.h"
#include "kir_parse.h"
#include "k2c_lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
    fprintf(stderr,
            "usage: k2c [--no-main] --root DIR -o DIR file.kry ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *out_dir = NULL;
    int i;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if(argv[i][0] == '-') {
            /* accept and ignore --no-main (main() generation is app-driven
             * via Kir app metadata; a standalone main flavor lands with it) */
            if(strcmp(argv[i], "--no-main") != 0) {
                usage();
                return 1;
            }
        } else {
            break;
        }
    }
    if(root == NULL || out_dir == NULL || i >= argc) {
        usage();
        return 1;
    }
    for(; i < argc; i++) {
        KirProgram *prog = kir_parse_file(argv[i], root);

        if(prog == NULL) {
            fprintf(stderr, "k2c: failed to parse %s\n", argv[i]);
            return 1;
        }
        k2c_lower(prog, root, out_dir);
        KirProgramFree(prog);
    }
    return 0;
}
