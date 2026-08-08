/* Diagnostics and error recovery. die() is the universal error path: when a
 * "%s:%d:" location is present and recovery is active, it records a diagnostic
 * and longjmps back to the statement boundary parse_kry installed, so one bad
 * statement does not abort the whole compile. Fatal (no-location) callers fall
 * through to stderr + exit(1).
 *
 * The recovery target (jmp_buf, recovering flag, current file) lives here so
 * parse_kry can install a boundary without other modules reaching into the
 * globals directly. setjmp() must run in parse_kry's own frame, so the buffer
 * is exposed by reference via kc_recover_buf(). */
#include "kc_internal.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static jmp_buf g_kc_recover_buf;
static int g_kc_recovering = 0;
static KryFile *g_kc_current_file = NULL;

jmp_buf *
kc_recover_buf(void)
{
    return &g_kc_recover_buf;
}

void
kc_set_recovery_file(KryFile *file)
{
    g_kc_current_file = file;
}

void
kc_set_recovering(int on)
{
    g_kc_recovering = on;
}

void
die(const char *fmt, ...)
{
    va_list args;
    char message[2048];
    char *p1;
    char *p2;
    char *p3;
    int line = 0;
    int column = 1;

    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    p1 = strchr(message, ':');
    p2 = p1 != NULL ? strchr(p1 + 1, ':') : NULL;
    if(p1 != NULL && p2 != NULL) {
        int all_digits = 1;

        for(char *p = p1 + 1; p < p2; p++) {
            if(*p < '0' || *p > '9') {
                all_digits = 0;
                break;
            }
        }
        if(all_digits) {
            *p1 = '\0';
            *p2 = '\0';
            line = atoi(p1 + 1);
            p3 = strchr(p2 + 1, ':');
            if(p3 != NULL) {
                int column_digits = 1;

                for(char *p = p2 + 1; p < p3; p++) {
                    if(*p < '0' || *p > '9') {
                        column_digits = 0;
                        break;
                    }
                }
                if(column_digits) {
                    *p3 = '\0';
                    column = atoi(p2 + 1);
                    p2 = p3;
                }
            }
            if(column <= 0)
                column = 1;
            /* During parsing, a located error is recoverable: record it as a
             * diagnostic and jump back to the statement boundary so kc can
             * report the next error too. Fatal (non-parse) callers never set
             * the recovery flag and fall through to exit. */
            if(g_kc_recovering && g_kc_current_file != NULL) {
                kc_error(g_kc_current_file, line, "%s", trim(p2 + 1));
                g_kc_current_file->diagnostics[g_kc_current_file->diagnostic_count - 1].column = column;
                longjmp(g_kc_recover_buf, 1);
            }
            /* A fatal error reached mid-parse: print any diagnostics already
             * recorded by earlier recoveries before aborting, so the user sees
             * every error found so far, not just this last one. */
            if(g_kc_current_file != NULL && g_kc_current_file->diagnostic_count > 0)
                kc_flush_diagnostics(g_kc_current_file);
            fprintf(stderr, "%s:%d:%d: error: %s\n",
                    message, line, column, trim(p2 + 1));
            exit(1);
        }
    }
    if(g_kc_current_file != NULL && g_kc_current_file->diagnostic_count > 0)
        kc_flush_diagnostics(g_kc_current_file);
    fprintf(stderr, "kc: error: %s\n", message);
    exit(1);
}

void
kc_error(KryFile *file, int line_no, const char *fmt, ...)
{
    va_list args;
    KryDiagnostic *diag;

    if(file == NULL || file->diagnostic_count >= KC_DIAGNOSTIC_MAX) {
        char msg[2048];
        va_start(args, fmt);
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);
        die("%s", msg);
    }
    diag = &file->diagnostics[file->diagnostic_count++];
    snprintf(diag->path, sizeof(diag->path), "%s", file->path);
    diag->line = line_no > 0 ? line_no : 1;
    diag->column = 1;
    va_start(args, fmt);
    vsnprintf(diag->message, sizeof(diag->message), fmt, args);
    va_end(args);
}

int
kc_flush_diagnostics(const KryFile *file)
{
    int i;

    if(file == NULL)
        return 0;
    for(i = 0; i < file->diagnostic_count; i++) {
        const KryDiagnostic *d = &file->diagnostics[i];
        fprintf(stderr, "%s:%d:%d: error: %s\n", d->path, d->line, d->column,
                d->message);
    }
    return file->diagnostic_count;
}
