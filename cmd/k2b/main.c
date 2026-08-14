/*
 * k2b - .kry -> .krb cartridge compiler. Shares the Kir frontend with k2c:
 * every .kry parses into a KirProgram (kir_parse.c), then lowers to a .krb
 * cartridge + C host (k2b_krb.c). One frontend, two backends.
 */
#include "kir.h"
#include "kir_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void write_krb(const KirModule *m, const char *root, const char *out_dir,
               int no_main);

static void
usage(void)
{
    fprintf(stderr, "usage: k2b [--no-main] --root DIR -o DIR file.kry ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = NULL;
    const char *out_dir = NULL;
    int no_main = 0;
    int i;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
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
            fprintf(stderr, "k2b: failed to parse %s\n", argv[i]);
            return 1;
        }
        for(int m = 0; m < prog->module_count; m++)
            write_krb(&prog->modules[m], root, out_dir, no_main);
        KirProgramFree(prog);
    }
    return 0;
}
