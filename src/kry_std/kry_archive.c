#include "kry_archive.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#if defined(_WIN32)
#include <direct.h>
#define KRY_MKDIR_ONE(p) _mkdir(p)
#else
#include <sys/stat.h>
#define KRY_MKDIR_ONE(p) mkdir((p), 0755)
#endif

typedef struct ArchiveFileEntry {
    char *name;
    uint16_t method;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t local_header_offset;
    int is_directory;
} ArchiveFileEntry;

typedef struct ArchiveImpl {
    FILE *file;
    ArchiveFileEntry *entries;
    int entry_count;
    int entry_capacity;
    int writing;
} ArchiveImpl;

static uint16_t
read_le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t
read_le32(const unsigned char *p)
{
    return (uint32_t)p[0] |
        ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

static int
write_le16(FILE *file, uint16_t value)
{
    unsigned char bytes[2];

    bytes[0] = (unsigned char)(value & 0xff);
    bytes[1] = (unsigned char)((value >> 8) & 0xff);
    return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
}

static int
write_le32(FILE *file, uint32_t value)
{
    unsigned char bytes[4];

    bytes[0] = (unsigned char)(value & 0xff);
    bytes[1] = (unsigned char)((value >> 8) & 0xff);
    bytes[2] = (unsigned char)((value >> 16) & 0xff);
    bytes[3] = (unsigned char)((value >> 24) & 0xff);
    return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
}

static int
write_bytes(FILE *file, const void *data, size_t size)
{
    return size == 0 || fwrite(data, 1, size, file) == size;
}

static char *
copy_name(const char *name, size_t len)
{
    char *out;

    out = (char *)malloc(len + 1);
    if(out == NULL)
        return NULL;
    memcpy(out, name, len);
    out[len] = '\0';
    return out;
}

static void
free_entries(ArchiveImpl *impl)
{
    if(impl == NULL)
        return;
    for(int i = 0; i < impl->entry_count; i++)
        free(impl->entries[i].name);
    free(impl->entries);
    impl->entries = NULL;
    impl->entry_count = 0;
    impl->entry_capacity = 0;
}

static ArchiveImpl *
archive_impl(Archive *archive)
{
    if(archive == NULL)
        return NULL;
    return (ArchiveImpl *)archive->impl;
}

static int
reserve_entry(ArchiveImpl *impl)
{
    ArchiveFileEntry *entries;
    int capacity;

    if(impl->entry_count < impl->entry_capacity)
        return 1;
    capacity = impl->entry_capacity == 0 ? 16 : impl->entry_capacity * 2;
    entries = (ArchiveFileEntry *)realloc(impl->entries,
                                          (size_t)capacity * sizeof(*entries));
    if(entries == NULL)
        return 0;
    impl->entries = entries;
    impl->entry_capacity = capacity;
    return 1;
}

static int
add_entry(ArchiveImpl *impl, ArchiveFileEntry entry)
{
    if(!reserve_entry(impl))
        return 0;
    impl->entries[impl->entry_count++] = entry;
    return 1;
}

static int
seek_u32(FILE *file, uint32_t offset)
{
    return fseek(file, (long)offset, SEEK_SET) == 0;
}

static void
destroy_impl(ArchiveImpl *impl)
{
    if(impl == NULL)
        return;
    if(impl->file != NULL)
        fclose(impl->file);
    free_entries(impl);
    free(impl);
}

int
KryArchiveMkdirP(const char *path)
{
    char partial[1024];
    size_t len;

    if(path == NULL || path[0] == '\0')
        return 0;
    len = strlen(path);
    if(len >= sizeof(partial))
        return 0;
    memcpy(partial, path, len + 1);
    for(size_t i = 1; i < len; i++) {
        if(partial[i] != '/' && partial[i] != '\\')
            continue;
        partial[i] = '\0';
        if(partial[0] != '\0' &&
           KRY_MKDIR_ONE(partial) != 0 && errno != EEXIST)
            return 0;
        partial[i] = path[i];
    }
    if(KRY_MKDIR_ONE(partial) != 0 && errno != EEXIST)
        return 0;
    return 1;
}

int
ArchiveEntryNameSafe(const char *name)
{
    const char *p = name;

    if(name == NULL || name[0] == '\0')
        return 0;
    if(name[0] == '/' || name[0] == '\\')
        return 0;
    if(((name[0] >= 'a' && name[0] <= 'z') ||
        (name[0] >= 'A' && name[0] <= 'Z')) && name[1] == ':')
        return 0;
    while(*p != '\0') {
        if(p[0] == '.' && p[1] == '.' &&
           (p[2] == '/' || p[2] == '\\' || p[2] == '\0')) {
            if(p == name || p[-1] == '/' || p[-1] == '\\')
                return 0;
        }
        p++;
    }
    return 1;
}

int
KryArchiveEntryNameSafe(const char *name)
{
    return ArchiveEntryNameSafe(name);
}

static int
read_central_directory(ArchiveImpl *impl, uint32_t offset, uint16_t count)
{
    if(!seek_u32(impl->file, offset))
        return 0;
    for(uint16_t i = 0; i < count; i++) {
        unsigned char header[46];
        uint16_t name_len;
        uint16_t extra_len;
        uint16_t comment_len;
        ArchiveFileEntry entry;

        if(fread(header, 1, sizeof(header), impl->file) != sizeof(header))
            return 0;
        if(read_le32(header) != 0x02014b50u)
            return 0;
        name_len = read_le16(header + 28);
        extra_len = read_le16(header + 30);
        comment_len = read_le16(header + 32);
        memset(&entry, 0, sizeof(entry));
        entry.method = read_le16(header + 10);
        entry.crc32 = read_le32(header + 16);
        entry.compressed_size = read_le32(header + 20);
        entry.uncompressed_size = read_le32(header + 24);
        entry.local_header_offset = read_le32(header + 42);
        entry.name = copy_name("", name_len);
        if(entry.name == NULL)
            return 0;
        if(fread(entry.name, 1, name_len, impl->file) != name_len) {
            free(entry.name);
            return 0;
        }
        entry.name[name_len] = '\0';
        entry.is_directory = name_len > 0 && entry.name[name_len - 1] == '/';
        if(fseek(impl->file, (long)extra_len + (long)comment_len, SEEK_CUR) != 0) {
            free(entry.name);
            return 0;
        }
        if(!add_entry(impl, entry)) {
            free(entry.name);
            return 0;
        }
    }
    return 1;
}

int
ArchiveOpenZip(Archive *archive, const char *path)
{
    ArchiveImpl *impl;
    unsigned char *tail;
    long file_size;
    long tail_size;
    long eocd_at = -1;
    uint16_t count;
    uint32_t central_offset;

    if(archive == NULL || path == NULL || path[0] == '\0')
        return 0;
    ArchiveClose(archive);
    impl = (ArchiveImpl *)calloc(1, sizeof(*impl));
    if(impl == NULL)
        return 0;
    impl->file = fopen(path, "rb");
    if(impl->file == NULL) {
        free(impl);
        return 0;
    }
    if(fseek(impl->file, 0, SEEK_END) != 0) {
        destroy_impl(impl);
        return 0;
    }
    file_size = ftell(impl->file);
    if(file_size < 22) {
        destroy_impl(impl);
        return 0;
    }
    tail_size = file_size < 66000 ? file_size : 66000;
    tail = (unsigned char *)malloc((size_t)tail_size);
    if(tail == NULL) {
        destroy_impl(impl);
        return 0;
    }
    if(fseek(impl->file, file_size - tail_size, SEEK_SET) != 0 ||
       fread(tail, 1, (size_t)tail_size, impl->file) != (size_t)tail_size) {
        free(tail);
        destroy_impl(impl);
        return 0;
    }
    for(long i = tail_size - 22; i >= 0; i--) {
        if(read_le32(tail + i) == 0x06054b50u) {
            eocd_at = i;
            break;
        }
    }
    if(eocd_at < 0) {
        free(tail);
        destroy_impl(impl);
        return 0;
    }
    count = read_le16(tail + eocd_at + 10);
    central_offset = read_le32(tail + eocd_at + 16);
    free(tail);
    if(!read_central_directory(impl, central_offset, count)) {
        destroy_impl(impl);
        return 0;
    }
    archive->impl = impl;
    return 1;
}

void
ArchiveClose(Archive *archive)
{
    ArchiveImpl *impl = archive_impl(archive);

    if(impl == NULL)
        return;
    destroy_impl(impl);
    archive->impl = NULL;
}

int
ArchiveEntryCount(Archive *archive)
{
    ArchiveImpl *impl = archive_impl(archive);

    if(impl == NULL)
        return 0;
    return impl->entry_count;
}

int
ArchiveReadEntry(Archive *archive, int index, ArchiveEntry *entry)
{
    ArchiveImpl *impl = archive_impl(archive);
    ArchiveFileEntry *file_entry;

    if(impl == NULL || entry == NULL || index < 0 || index >= impl->entry_count)
        return 0;
    file_entry = &impl->entries[index];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->name, sizeof(entry->name), "%s", file_entry->name);
    entry->is_directory = file_entry->is_directory;
    entry->uncompressed_size = file_entry->uncompressed_size;
    return 1;
}

int
ArchiveFindEntry(Archive *archive, const char *name)
{
    ArchiveImpl *impl = archive_impl(archive);

    if(impl == NULL || name == NULL || name[0] == '\0')
        return -1;
    for(int i = 0; i < impl->entry_count; i++) {
        if(strcmp(impl->entries[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int
entry_data_offset(ArchiveImpl *impl, const ArchiveFileEntry *entry,
                  uint32_t *offset)
{
    unsigned char header[30];
    uint16_t name_len;
    uint16_t extra_len;

    if(!seek_u32(impl->file, entry->local_header_offset))
        return 0;
    if(fread(header, 1, sizeof(header), impl->file) != sizeof(header))
        return 0;
    if(read_le32(header) != 0x04034b50u)
        return 0;
    name_len = read_le16(header + 26);
    extra_len = read_le16(header + 28);
    *offset = entry->local_header_offset + 30u + name_len + extra_len;
    return 1;
}

void *
ArchiveReadEntryHeap(Archive *archive, int index, size_t *out_size)
{
    ArchiveImpl *impl = archive_impl(archive);
    ArchiveFileEntry *entry;
    unsigned char *compressed;
    unsigned char *out;
    uint32_t data_offset;

    if(out_size != NULL)
        *out_size = 0;
    if(impl == NULL || index < 0 || index >= impl->entry_count)
        return NULL;
    entry = &impl->entries[index];
    if(entry->is_directory)
        return NULL;
    if(!entry_data_offset(impl, entry, &data_offset))
        return NULL;
    compressed = (unsigned char *)malloc(entry->compressed_size == 0 ? 1 :
                                         entry->compressed_size);
    out = (unsigned char *)malloc((size_t)entry->uncompressed_size + 1);
    if(compressed == NULL || out == NULL) {
        free(compressed);
        free(out);
        return NULL;
    }
    if(!seek_u32(impl->file, data_offset) ||
       fread(compressed, 1, entry->compressed_size, impl->file) !=
       entry->compressed_size) {
        free(compressed);
        free(out);
        return NULL;
    }
    if(entry->method == 0) {
        if(entry->compressed_size != entry->uncompressed_size) {
            free(compressed);
            free(out);
            return NULL;
        }
        memcpy(out, compressed, entry->uncompressed_size);
    } else if(entry->method == 8) {
        z_stream stream;
        int rc;

        memset(&stream, 0, sizeof(stream));
        stream.next_in = compressed;
        stream.avail_in = entry->compressed_size;
        stream.next_out = out;
        stream.avail_out = entry->uncompressed_size;
        if(inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            free(compressed);
            free(out);
            return NULL;
        }
        rc = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        if(rc != Z_STREAM_END || stream.total_out != entry->uncompressed_size) {
            free(compressed);
            free(out);
            return NULL;
        }
    } else {
        free(compressed);
        free(out);
        return NULL;
    }
    free(compressed);
    if(crc32(0L, out, entry->uncompressed_size) != entry->crc32) {
        free(out);
        return NULL;
    }
    out[entry->uncompressed_size] = '\0';
    if(out_size != NULL)
        *out_size = entry->uncompressed_size;
    return out;
}

void *
ArchiveReadNamedEntryHeap(Archive *archive, const char *name, size_t *out_size)
{
    int index = ArchiveFindEntry(archive, name);

    if(index < 0)
        return NULL;
    return ArchiveReadEntryHeap(archive, index, out_size);
}

int
ArchiveExtractEntry(Archive *archive, int index, const char *path)
{
    FILE *out_file;
    void *data;
    size_t size;
    int ok;

    if(path == NULL || path[0] == '\0')
        return 0;
    data = ArchiveReadEntryHeap(archive, index, &size);
    if(data == NULL)
        return 0;
    out_file = fopen(path, "wb");
    if(out_file == NULL) {
        free(data);
        return 0;
    }
    ok = fwrite(data, 1, size, out_file) == size;
    if(fclose(out_file) != 0)
        ok = 0;
    free(data);
    return ok;
}

int
ArchiveCreateZip(Archive *archive, const char *path)
{
    ArchiveImpl *impl;

    if(archive == NULL || path == NULL || path[0] == '\0')
        return 0;
    ArchiveClose(archive);
    impl = (ArchiveImpl *)calloc(1, sizeof(*impl));
    if(impl == NULL)
        return 0;
    impl->file = fopen(path, "wb");
    if(impl->file == NULL) {
        free(impl);
        return 0;
    }
    impl->writing = 1;
    archive->impl = impl;
    return 1;
}

static int
deflate_raw(const unsigned char *data, size_t data_size,
            unsigned char **out, uint32_t *out_size)
{
    z_stream stream;
    unsigned long bound;
    int rc;

    bound = compressBound((uLong)data_size);
    *out = (unsigned char *)malloc(bound == 0 ? 1 : bound);
    if(*out == NULL)
        return 0;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)data;
    stream.avail_in = (uInt)data_size;
    stream.next_out = *out;
    stream.avail_out = (uInt)bound;
    rc = deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
                      8, Z_DEFAULT_STRATEGY);
    if(rc != Z_OK) {
        free(*out);
        *out = NULL;
        return 0;
    }
    rc = deflate(&stream, Z_FINISH);
    if(rc != Z_STREAM_END) {
        deflateEnd(&stream);
        free(*out);
        *out = NULL;
        return 0;
    }
    *out_size = (uint32_t)stream.total_out;
    deflateEnd(&stream);
    return 1;
}

static int
write_local_header(FILE *file, const ArchiveFileEntry *entry)
{
    return write_le32(file, 0x04034b50u) &&
        write_le16(file, 20) &&
        write_le16(file, 0) &&
        write_le16(file, entry->method) &&
        write_le16(file, 0) &&
        write_le16(file, 0) &&
        write_le32(file, entry->crc32) &&
        write_le32(file, entry->compressed_size) &&
        write_le32(file, entry->uncompressed_size) &&
        write_le16(file, (uint16_t)strlen(entry->name)) &&
        write_le16(file, 0) &&
        write_bytes(file, entry->name, strlen(entry->name));
}

int
ArchiveAddMemory(Archive *archive, const char *name,
                 const void *data, size_t data_size,
                 ArchiveCompression compression)
{
    ArchiveImpl *impl = archive_impl(archive);
    ArchiveFileEntry entry;
    unsigned char *compressed = NULL;
    const unsigned char *bytes = (const unsigned char *)data;
    const unsigned char *payload = bytes;
    uint32_t payload_size = (uint32_t)data_size;
    long offset;
    int ok;

    if(impl == NULL || !impl->writing || name == NULL || name[0] == '\0')
        return 0;
    if(!ArchiveEntryNameSafe(name))
        return 0;
    if(data == NULL && data_size != 0)
        return 0;
    if(data_size > UINT32_MAX || strlen(name) > UINT16_MAX)
        return 0;
    memset(&entry, 0, sizeof(entry));
    entry.name = copy_name(name, strlen(name));
    if(entry.name == NULL)
        return 0;
    entry.method = compression == ARCHIVE_STORE ? 0 : 8;
    entry.crc32 = crc32(0L, bytes, (uInt)data_size);
    entry.uncompressed_size = (uint32_t)data_size;
    if(entry.method == 8) {
        if(!deflate_raw(bytes, data_size, &compressed, &payload_size)) {
            free(entry.name);
            return 0;
        }
        payload = compressed;
    }
    entry.compressed_size = payload_size;
    offset = ftell(impl->file);
    if(offset < 0 || offset > UINT32_MAX) {
        free(compressed);
        free(entry.name);
        return 0;
    }
    entry.local_header_offset = (uint32_t)offset;
    ok = write_local_header(impl->file, &entry) &&
        write_bytes(impl->file, payload, payload_size);
    free(compressed);
    if(!ok || !add_entry(impl, entry)) {
        free(entry.name);
        return 0;
    }
    return 1;
}

static int
write_central_entry(FILE *file, const ArchiveFileEntry *entry)
{
    size_t name_len = strlen(entry->name);

    return write_le32(file, 0x02014b50u) &&
        write_le16(file, 20) &&
        write_le16(file, 20) &&
        write_le16(file, 0) &&
        write_le16(file, entry->method) &&
        write_le16(file, 0) &&
        write_le16(file, 0) &&
        write_le32(file, entry->crc32) &&
        write_le32(file, entry->compressed_size) &&
        write_le32(file, entry->uncompressed_size) &&
        write_le16(file, (uint16_t)name_len) &&
        write_le16(file, 0) &&
        write_le16(file, 0) &&
        write_le16(file, 0) &&
        write_le16(file, 0) &&
        write_le32(file, 0) &&
        write_le32(file, entry->local_header_offset) &&
        write_bytes(file, entry->name, name_len);
}

int
ArchiveFinishZip(Archive *archive)
{
    ArchiveImpl *impl = archive_impl(archive);
    long central_offset;
    long central_end;
    uint32_t central_size;

    if(impl == NULL || !impl->writing || impl->entry_count > UINT16_MAX)
        return 0;
    central_offset = ftell(impl->file);
    if(central_offset < 0 || central_offset > UINT32_MAX)
        return 0;
    for(int i = 0; i < impl->entry_count; i++) {
        if(!write_central_entry(impl->file, &impl->entries[i]))
            return 0;
    }
    central_end = ftell(impl->file);
    if(central_end < central_offset || central_end - central_offset > UINT32_MAX)
        return 0;
    central_size = (uint32_t)(central_end - central_offset);
    if(!write_le32(impl->file, 0x06054b50u) ||
       !write_le16(impl->file, 0) ||
       !write_le16(impl->file, 0) ||
       !write_le16(impl->file, (uint16_t)impl->entry_count) ||
       !write_le16(impl->file, (uint16_t)impl->entry_count) ||
       !write_le32(impl->file, central_size) ||
       !write_le32(impl->file, (uint32_t)central_offset) ||
       !write_le16(impl->file, 0))
        return 0;
    impl->writing = 0;
    return fflush(impl->file) == 0;
}

int
KryArchiveExtractZip(const char *zip_path, const char *dest_dir)
{
    Archive archive = {0};
    int ok = 1;

    if(zip_path == NULL || dest_dir == NULL || dest_dir[0] == '\0')
        return 0;
    if(!KryArchiveMkdirP(dest_dir))
        return 0;
    if(!ArchiveOpenZip(&archive, zip_path))
        return 0;
    for(int i = 0; i < ArchiveEntryCount(&archive); i++) {
        ArchiveEntry entry;
        char dest[1024];
        char *last_slash;
        char *last_backslash;
        char *last;

        if(!ArchiveReadEntry(&archive, i, &entry) || entry.is_directory)
            continue;
        if(!ArchiveEntryNameSafe(entry.name)) {
            ok = 0;
            break;
        }
        if(snprintf(dest, sizeof(dest), "%s/%s", dest_dir, entry.name) >=
           (int)sizeof(dest)) {
            ok = 0;
            break;
        }
        last_slash = strrchr(dest, '/');
        last_backslash = strrchr(dest, '\\');
        last = last_slash > last_backslash ? last_slash : last_backslash;
        if(last != NULL) {
            char saved = *last;

            *last = '\0';
            if(!KryArchiveMkdirP(dest)) {
                ok = 0;
                break;
            }
            *last = saved;
        }
        if(!ArchiveExtractEntry(&archive, i, dest)) {
            ok = 0;
            break;
        }
    }
    ArchiveClose(&archive);
    return ok;
}
