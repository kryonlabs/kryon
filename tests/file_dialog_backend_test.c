#define _POSIX_C_SOURCE 200809L

#include "file_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;

static void
check_backend(const char *name, const char *expected)
{
    const char *actual = GetFileDialogBackendName();

    if(strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL: %s got %s want %s\n", name, actual, expected);
        failures++;
    }
}

static void
write_fake_command(const char *dir, const char *name)
{
    char path[512];
    FILE *file;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    file = fopen(path, "w");
    if(file == NULL) {
        fprintf(stderr, "FAIL: could not create %s\n", path);
        failures++;
        return;
    }
    fputs("#!/bin/sh\nprintf '%s\\n' /tmp/selected.txt\n", file);
    fclose(file);
    if(chmod(path, 0700) != 0) {
        fprintf(stderr, "FAIL: could not chmod %s\n", path);
        failures++;
    }
}

static void
set_test_path(const char *dir)
{
    setenv("PATH", dir, 1);
}

static void
test_backend_priority(void)
{
    char template[] = "/tmp/flint-file-dialog-test.XXXXXX";
    char *dir = mkdtemp(template);

    if(dir == NULL) {
        fprintf(stderr, "FAIL: mkdtemp failed\n");
        failures++;
        return;
    }
    unsetenv("FLINT_FILE_DIALOG_BACKEND");
    set_test_path(dir);
    check_backend("no helper returns none", "none");

    write_fake_command(dir, "yad");
    check_backend("yad fallback", "yad");

    write_fake_command(dir, "kdialog");
    check_backend("kdialog preferred over yad", "kdialog");

    write_fake_command(dir, "zenity");
    check_backend("zenity preferred over desktop-specific helpers", "zenity");
}

static void
test_forced_backend(void)
{
    char template[] = "/tmp/flint-file-dialog-test.XXXXXX";
    char *dir = mkdtemp(template);

    if(dir == NULL) {
        fprintf(stderr, "FAIL: mkdtemp failed\n");
        failures++;
        return;
    }
    set_test_path(dir);

    write_fake_command(dir, "zenity");
    write_fake_command(dir, "yad");
    setenv("FLINT_FILE_DIALOG_BACKEND", "yad", 1);
    check_backend("env forces available backend", "yad");

    setenv("FLINT_FILE_DIALOG_BACKEND", "kdialog", 1);
    check_backend("env missing backend fails closed", "none");

    setenv("FLINT_FILE_DIALOG_BACKEND", "none", 1);
    check_backend("env disables backend", "none");
}

int
main(void)
{
    test_backend_priority();
    test_forced_backend();
    if(failures != 0)
        return 1;
    printf("file dialog backend tests passed\n");
    return 0;
}
