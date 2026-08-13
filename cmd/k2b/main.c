/*
 * k2b — standalone .kry -> .krb cartridge compiler. Independent of kc.
 *
 *   k2b [--no-main] --root DIR -o DIR file.kry [file.kry ...]
 *
 * For each input .kry, parses it (k2b_parse.c) and emits a .krb cartridge plus
 * a C host pair (*.krb.c / *.krb.h) next to the output directory.
 */
#include "k2b.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        if(strcmp(argv[i], "--no-main") == 0) {
            no_main = 1;
        } else if(strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
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
        static K2bFile file;

        memset(&file, 0, sizeof(file));
        file.no_main = no_main;
        if(k2b_parse_file(&file, argv[i], root) != 0)
            return 1;
        write_krb(&file, root, out_dir);
    }
    return 0;
}
