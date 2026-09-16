#include "ui_style_sheet.h"
#include "ui/kss_parser.h"

#include "runtime/kss_formatter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* kssfmt: format KSS style sheets through the shared .kry formatter.
 * Mirrors kry-fmt: default rewrites files in place, --check exits nonzero
 * when a file would change. */

static void
usage(FILE *out)
{
    fputs("usage: kssfmt [--check] file.kss [...]\n", out);
}

static char *
read_file(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    long size;
    char *data;

    if(f == NULL)
        return NULL;
    if(fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
       fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    data = (char *)malloc((size_t)size + 1);
    if(data == NULL) {
        fclose(f);
        return NULL;
    }
    if(fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    data[size] = '\0';
    fclose(f);
    if(size_out != NULL)
        *size_out = (size_t)size;
    return data;
}

static int
write_file(const char *path, const char *data, size_t size)
{
    FILE *f = fopen(path, "wb");

    if(f == NULL)
        return -1;
    if(fwrite(data, 1, size, f) != size) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int
format_one(const char *path, int check, int *status)
{
    size_t size = 0;
    char *source = read_file(path, &size);
    char *out;
    size_t capacity = size * 4 + 65536;
    char diagnostic[256];
    int length;

    if(source == NULL) {
        fprintf(stderr, "kssfmt: cannot read %s\n", path);
        *status = 1;
        return 0;
    }
    out = (char *)malloc(capacity);
    if(out == NULL) {
        free(source);
        fprintf(stderr, "kssfmt: out of memory\n");
        *status = 1;
        return 0;
    }
    length = kss_format_string(source, out, capacity, diagnostic,
                               sizeof(diagnostic));
    if(length < 0) {
        if(diagnostic[0] == '\0' && capacity < size * 16 + 262144) {
            free(out);
            capacity = size * 16 + 262144;
            out = (char *)malloc(capacity);
            if(out != NULL)
                length = kss_format_string(source, out, capacity, diagnostic,
                                           sizeof(diagnostic));
        }
    }
    if(length < 0) {
        fprintf(stderr, "kssfmt: %s: %s\n", path,
                diagnostic[0] != '\0' ? diagnostic : "format failed");
        free(out);
        free(source);
        *status = 1;
        return 0;
    }
    if((size_t)length != size || memcmp(out, source, size) != 0) {
        if(check) {
            fprintf(stderr, "kssfmt: would reformat %s\n", path);
            *status = 1;
        } else if(write_file(path, out, (size_t)length) != 0) {
            fprintf(stderr, "kssfmt: cannot write %s\n", path);
            *status = 1;
        }
    }
    free(out);
    free(source);
    return 1;
}

int
main(int argc, char **argv)
{
    int check = 0;
    int status = 0;
    int first = 1;

    if(argc > 1 && strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return 0;
    }
    if(argc > 1 && strcmp(argv[1], "--check") == 0) {
        check = 1;
        first = 2;
    }
    if(argc <= first) {
        usage(stderr);
        return 2;
    }
    for(int i = first; i < argc; i++)
        format_one(argv[i], check, &status);
    return status;
}
