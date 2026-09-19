#define _POSIX_C_SOURCE 200809L

#include "session.h"
#include "watch.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static void
mode(const char *project, const char *name)
{
    char filename[PREVIEW_PATH_MAX];
    FILE *file;

    snprintf(filename, sizeof(filename), "%s/mode", project);
    file = fopen(filename, "w");
    assert(file != NULL);
    assert(fputs(name, file) >= 0);
    assert(fclose(file) == 0);
}

static int
finish(PreviewSession *session)
{
    struct timespec pause = {0, 1000000};
    int result;

    for(int attempt = 0; attempt < 10000; attempt++) {
        result = PreviewPollBuild(session);
        if(result != 0)
            return result;
        nanosleep(&pause, NULL);
    }
    assert(!"preview build timed out");
    return -1;
}

static int
value(PreviewSession *session)
{
    assert(session->host != NULL);
    return session->host->screen_count(session->host->userdata);
}

int
main(int argc, char **argv)
{
    PreviewSession session = {0};
    PreviewSession other = {0};
    const char *failures[] = {"build-failure", "invalid-library", "missing-symbol",
                              "rejected-abi", "missing-draw"};
    char directory[PREVIEW_PATH_MAX];
    char library[PREVIEW_PATH_MAX];
    AppHost *original;
    uint64_t before;
    uint64_t after;
    char filename[PREVIEW_PATH_MAX];
    FILE *source;

    assert(argc == 2);
    assert(PreviewSourceStamp(argv[1], &before));
    snprintf(filename, sizeof(filename), "%s/source.kry", argv[1]);
    source = fopen(filename, "w");
    assert(source != NULL);
    fputs("Value :: 1\n", source);
    fclose(source);
    assert(PreviewSourceStamp(argv[1], &after) && after != before);
    assert(unlink(filename) == 0);
    assert(PreviewSourceStamp(argv[1], &after) && after == before);
    mode(argv[1], "good-1");
    assert(PreviewStartBuild(&session, argv[1]) > 0);
    assert(finish(&session) > 0);
    assert(value(&session) == 1);
    snprintf(filename, sizeof(filename), "%s/build/generated.kry", argv[1]);
    source = fopen(filename, "w");
    assert(source != NULL);
    fputs("generated build output", source);
    fclose(source);
    assert(PreviewSourceStamp(argv[1], &after) && after == before);
    session.host->draw(session.host->userdata, (Rectangle){0});
    assert(value(&session) == 11);
    original = session.host;
    strcpy(library, session.library_path);
    for(size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        mode(argv[1], failures[i]);
        assert(PreviewStartBuild(&session, argv[1]) > 0);
        assert(session.host == original);
        assert(finish(&session) < 0);
        if(i == 0) {
            assert(session.diagnostic_count == 1);
            assert(strcmp(session.diagnostics[0].path, "sample.kry") == 0);
            assert(strcmp(session.diagnostics[0].code, "check.widget") == 0);
            assert(session.diagnostics[0].line == 4 && session.diagnostics[0].column == 2);
            assert(session.diagnostics[0].end_column == 8);
        } else {
            assert(session.diagnostic_count == 0);
        }
        assert(session.host == original);
        assert(value(&session) == 11);
        assert(access(library, F_OK) == 0);
    }
    mode(argv[1], "slow-good-2");
    assert(PreviewStartBuild(&session, argv[1]) > 0);
    assert(PreviewPollBuild(&session) == 0);
    session.host->draw(session.host->userdata, (Rectangle){0});
    assert(value(&session) == 21);
    assert(finish(&session) > 0);
    assert(value(&session) == 2);
    assert(access(library, F_OK) != 0);
    assert(session.error[0] == '\0');

    mode(argv[1], "good-1");
    assert(PreviewStartBuild(&other, argv[1]) > 0);
    assert(finish(&other) > 0);
    assert(strcmp(session.directory, other.directory) != 0);
    assert(strcmp(session.library_path, other.library_path) != 0);
    PreviewClose(&other);
    assert(value(&session) == 2);

    strcpy(directory, session.directory);
    mode(argv[1], "slow-good-2");
    assert(PreviewStartBuild(&session, argv[1]) > 0);
    PreviewClose(&session);
    assert(session.build_pid == 0 && session.host == NULL);
    assert(access(directory, F_OK) != 0);
    PreviewClose(&session);
    puts("preview session: failure preservation, responsive build, recovery, isolation, cleanup passed");
    return 0;
}
