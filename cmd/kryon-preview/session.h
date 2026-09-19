#ifndef PREVIEW_SESSION_H
#define PREVIEW_SESSION_H

#include "app_host.h"

#include <stdio.h>
#include <sys/types.h>

#define PREVIEW_PATH_MAX 4096
#define PREVIEW_DIAGNOSTIC_MAX 16

typedef struct PreviewDiagnostic {
    char path[1024];
    char code[96];
    char message[1024];
    int line;
    int column;
    int end_line;
    int end_column;
} PreviewDiagnostic;

typedef struct PreviewSession {
    void *dylib;
    AppHost *host;
    DestroyAppHostCallback destroy_host;
    unsigned long generation;
    pid_t build_pid;
    FILE *build_output;
    char directory[PREVIEW_PATH_MAX];
    char project[PREVIEW_PATH_MAX];
    char library_path[PREVIEW_PATH_MAX];
    char error[512];
    PreviewDiagnostic diagnostics[PREVIEW_DIAGNOSTIC_MAX];
    int diagnostic_count;
    char output_line[16384];
    size_t output_length;
    int output_overflow;
} PreviewSession;

int PreviewStartBuild(PreviewSession *session, const char *project);
/* Zero while building, one after replacement, negative on failure. */
int PreviewPollBuild(PreviewSession *session);
void PreviewClose(PreviewSession *session);

#endif
