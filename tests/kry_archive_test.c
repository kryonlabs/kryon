#include "kry_archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#define mkdir_one(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define mkdir_one(path) mkdir((path), 0755)
#endif

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while(0)

static int
make_root(char *root, size_t root_size)
{
    for(int i = 0; i < 100; i++) {
        snprintf(root, root_size, "/tmp/kry-archive-test-%ld-%d",
                 (long)getpid(), i);
        if(mkdir_one(root) == 0)
            return 1;
    }
    return 0;
}

static int
read_file(const char *path, char *out, size_t out_size)
{
    FILE *file = fopen(path, "rb");
    size_t n;

    if(file == NULL)
        return 0;
    n = fread(out, 1, out_size - 1, file);
    fclose(file);
    out[n] = '\0';
    return 1;
}

int
main(void)
{
    char root[256];
    char archive_path[512];
    char extract_dir[512];
    char extracted_path[768];
    char extracted[128];
    Archive writer = {0};
    Archive reader = {0};
    ArchiveEntry entry;
    char *data;
    size_t data_size;
    int index;

    CHECK(make_root(root, sizeof(root)));
    snprintf(archive_path, sizeof(archive_path), "%s/test.zip", root);
    snprintf(extract_dir, sizeof(extract_dir), "%s/extract", root);
    snprintf(extracted_path, sizeof(extracted_path), "%s/a/deflated.txt",
             extract_dir);

    CHECK(ArchiveCreateZip(&writer, archive_path));
    CHECK(ArchiveAddMemory(&writer, "plain.txt", "plain", 5, ARCHIVE_STORE));
    CHECK(ArchiveAddMemory(&writer, "a/deflated.txt", "deflated", 8,
                           ARCHIVE_DEFLATE));
    CHECK(!ArchiveAddMemory(&writer, "../bad.txt", "bad", 3,
                            ARCHIVE_STORE));
    CHECK(ArchiveFinishZip(&writer));
    ArchiveClose(&writer);

    CHECK(ArchiveOpenZip(&reader, archive_path));
    CHECK(ArchiveEntryCount(&reader) == 2);
    CHECK(ArchiveReadEntry(&reader, 1, &entry));
    CHECK(strcmp(entry.name, "a/deflated.txt") == 0);
    CHECK(!entry.is_directory);
    CHECK(entry.uncompressed_size == 8);

    index = ArchiveFindEntry(&reader, "plain.txt");
    CHECK(index >= 0);
    data = ArchiveReadEntryHeap(&reader, index, &data_size);
    CHECK(data != NULL);
    CHECK(data_size == 5);
    CHECK(strcmp(data, "plain") == 0);
    free(data);

    data = ArchiveReadNamedEntryHeap(&reader, "a/deflated.txt", &data_size);
    CHECK(data != NULL);
    CHECK(data_size == 8);
    CHECK(strcmp(data, "deflated") == 0);
    free(data);
    ArchiveClose(&reader);

    CHECK(KryArchiveExtractZip(archive_path, extract_dir));
    CHECK(read_file(extracted_path, extracted, sizeof(extracted)));
    CHECK(strcmp(extracted, "deflated") == 0);
    CHECK(!ArchiveEntryNameSafe("../bad.txt"));
    CHECK(!ArchiveEntryNameSafe("/bad.txt"));
    CHECK(!ArchiveEntryNameSafe("C:/bad.txt"));
    CHECK(ArchiveEntryNameSafe("a/good.txt"));

    if(failures != 0) {
        fprintf(stderr, "kry_archive: %d failure(s)\n", failures);
        return 1;
    }
    printf("kry_archive ok\n");
    return 0;
}
