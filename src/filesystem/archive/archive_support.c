/**
 * @file archive_support.c
 * @brief Kryon Archive Support Implementation
 */

#include "internal/filesystem.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

// =============================================================================
// COMPRESSION UTILITIES
// =============================================================================

static bool is_gzip_file(const char* path) {
    if (!path) return false;
    
    const char* ext = strrchr(path, '.');
    return ext && (strcmp(ext, ".gz") == 0 || strcmp(ext, ".gzip") == 0);
}

static bool is_zip_file(const char* path) {
    if (!path) return false;
    
    const char* ext = strrchr(path, '.');
    return ext && strcmp(ext, ".zip") == 0;
}

// =============================================================================
// GZIP COMPRESSION/DECOMPRESSION
// =============================================================================

bool kryon_archive_compress_gzip(const void* data, size_t data_size, 
                                 void** compressed_data, size_t* compressed_size) {
    if (!data || data_size == 0 || !compressed_data || !compressed_size) {
        return false;
    }
    
    // Estimate compressed size (worst case)
    uLongf dest_len = compressBound(data_size);
    Bytef* dest = kryon_alloc(dest_len);
    if (!dest) return false;
    
    int result = compress(dest, &dest_len, (const Bytef*)data, data_size);
    if (result != Z_OK) {
        kryon_free(dest);
        return false;
    }
    
    // Reallocate to actual size
    Bytef* final_dest = kryon_alloc(dest_len);
    if (!final_dest) {
        kryon_free(dest);
        return false;
    }
    
    memcpy(final_dest, dest, dest_len);
    kryon_free(dest);
    
    *compressed_data = final_dest;
    *compressed_size = dest_len;
    
    return true;
}

bool kryon_archive_decompress_gzip(const void* compressed_data, size_t compressed_size,
                                   void** data, size_t* data_size, size_t max_size) {
    if (!compressed_data || compressed_size == 0 || !data || !data_size) {
        return false;
    }
    
    // Use max_size as initial estimate, or a reasonable default
    uLongf dest_len = max_size > 0 ? max_size : compressed_size * 4;
    Bytef* dest = kryon_alloc(dest_len);
    if (!dest) return false;
    
    int result = uncompress(dest, &dest_len, (const Bytef*)compressed_data, compressed_size);
    if (result != Z_OK) {
        kryon_free(dest);
        return false;
    }
    
    // Reallocate to actual size
    Bytef* final_dest = kryon_alloc(dest_len);
    if (!final_dest) {
        kryon_free(dest);
        return false;
    }
    
    memcpy(final_dest, dest, dest_len);
    kryon_free(dest);
    
    *data = final_dest;
    *data_size = dest_len;
    
    return true;
}

// =============================================================================
// FILE COMPRESSION/DECOMPRESSION
// =============================================================================

bool kryon_archive_compress_file(const char* src_path, const char* dst_path) {
    if (!src_path || !dst_path) return false;
    
    // Read source file
    FILE* src_file = fopen(src_path, "rb");
    if (!src_file) return false;
    
    // Get file size
    fseek(src_file, 0, SEEK_END);
    long file_size = ftell(src_file);
    fseek(src_file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(src_file);
        return false;
    }
    
    // Read file data
    void* file_data = kryon_alloc(file_size);
    if (!file_data) {
        fclose(src_file);
        return false;
    }
    
    size_t bytes_read = fread(file_data, 1, file_size, src_file);
    fclose(src_file);
    
    if (bytes_read != (size_t)file_size) {
        kryon_free(file_data);
        return false;
    }
    
    // Compress data
    void* compressed_data;
    size_t compressed_size;
    bool compress_result = kryon_archive_compress_gzip(file_data, file_size, 
                                                      &compressed_data, &compressed_size);
    kryon_free(file_data);
    
    if (!compress_result) {
        return false;
    }
    
    // Write compressed data
    FILE* dst_file = fopen(dst_path, "wb");
    if (!dst_file) {
        kryon_free(compressed_data);
        return false;
    }
    
    size_t bytes_written = fwrite(compressed_data, 1, compressed_size, dst_file);
    fclose(dst_file);
    kryon_free(compressed_data);
    
    return bytes_written == compressed_size;
}

bool kryon_archive_decompress_file(const char* src_path, const char* dst_path) {
    if (!src_path || !dst_path) return false;
    
    // Read compressed file
    FILE* src_file = fopen(src_path, "rb");
    if (!src_file) return false;
    
    // Get file size
    fseek(src_file, 0, SEEK_END);
    long file_size = ftell(src_file);
    fseek(src_file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(src_file);
        return false;
    }
    
    // Read compressed data
    void* compressed_data = kryon_alloc(file_size);
    if (!compressed_data) {
        fclose(src_file);
        return false;
    }
    
    size_t bytes_read = fread(compressed_data, 1, file_size, src_file);
    fclose(src_file);
    
    if (bytes_read != (size_t)file_size) {
        kryon_free(compressed_data);
        return false;
    }
    
    // Decompress data
    void* decompressed_data;
    size_t decompressed_size;
    bool decompress_result = kryon_archive_decompress_gzip(compressed_data, file_size,
                                                          &decompressed_data, &decompressed_size, 0);
    kryon_free(compressed_data);
    
    if (!decompress_result) {
        return false;
    }
    
    // Write decompressed data
    FILE* dst_file = fopen(dst_path, "wb");
    if (!dst_file) {
        kryon_free(decompressed_data);
        return false;
    }
    
    size_t bytes_written = fwrite(decompressed_data, 1, decompressed_size, dst_file);
    fclose(dst_file);
    kryon_free(decompressed_data);
    
    return bytes_written == decompressed_size;
}

// =============================================================================
// ARCHIVE CREATION AND EXTRACTION
// =============================================================================

typedef struct KryonArchiveEntry {
    char* name;              // Relative path within archive
    void* data;              // File content
    size_t size;             // Uncompressed size
    size_t compressed_size;  // Compressed size
    bool is_directory;
    time_t modified_time;
    struct KryonArchiveEntry* next;
} KryonArchiveEntry;

typedef struct {
    KryonArchiveEntry* entries;
    size_t entry_count;
    size_t total_size;
    size_t compressed_size;
} KryonArchive;

KryonArchiveHandle kryon_archive_create(void) {
    KryonArchive* archive = kryon_alloc(sizeof(KryonArchive));
    if (!archive) return NULL;
    
    memset(archive, 0, sizeof(KryonArchive));
    return (KryonArchiveHandle)archive;
}

void kryon_archive_destroy(KryonArchiveHandle handle) {
    if (!handle) return;
    
    KryonArchive* archive = (KryonArchive*)handle;
    
    KryonArchiveEntry* entry = archive->entries;
    while (entry) {
        KryonArchiveEntry* next = entry->next;
        kryon_free(entry->name);
        kryon_free(entry->data);
        kryon_free(entry);
        entry = next;
    }
    
    kryon_free(archive);
}

bool kryon_archive_add_file(KryonArchiveHandle handle, const char* archive_path, const char* file_path) {
    if (!handle || !archive_path || !file_path) return false;
    
    KryonArchive* archive = (KryonArchive*)handle;
    
    // Read file
    FILE* file = fopen(file_path, "rb");
    if (!file) return false;
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        return false;
    }
    
    void* file_data = NULL;
    if (file_size > 0) {
        file_data = kryon_alloc(file_size);
        if (!file_data) {
            fclose(file);
            return false;
        }
        
        size_t bytes_read = fread(file_data, 1, file_size, file);
        if (bytes_read != (size_t)file_size) {
            kryon_free(file_data);
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    
    // Create entry
    KryonArchiveEntry* entry = kryon_alloc(sizeof(KryonArchiveEntry));
    if (!entry) {
        kryon_free(file_data);
        return false;
    }
    
    entry->name = kryon_alloc(strlen(archive_path) + 1);
    if (!entry->name) {
        kryon_free(file_data);
        kryon_free(entry);
        return false;
    }
    strcpy(entry->name, archive_path);
    
    entry->data = file_data;
    entry->size = file_size;
    entry->compressed_size = file_size; // Will be updated if we compress
    entry->is_directory = false;
    entry->modified_time = time(NULL);
    entry->next = archive->entries;
    
    archive->entries = entry;
    archive->entry_count++;
    archive->total_size += file_size;
    
    return true;
}

bool kryon_archive_add_directory(KryonArchiveHandle handle, const char* archive_path) {
    if (!handle || !archive_path) return false;
    
    KryonArchive* archive = (KryonArchive*)handle;
    
    // Create directory entry
    KryonArchiveEntry* entry = kryon_alloc(sizeof(KryonArchiveEntry));
    if (!entry) return false;
    
    entry->name = kryon_alloc(strlen(archive_path) + 1);
    if (!entry->name) {
        kryon_free(entry);
        return false;
    }
    strcpy(entry->name, archive_path);
    
    entry->data = NULL;
    entry->size = 0;
    entry->compressed_size = 0;
    entry->is_directory = true;
    entry->modified_time = time(NULL);
    entry->next = archive->entries;
    
    archive->entries = entry;
    archive->entry_count++;
    
    return true;
}

bool kryon_archive_save(KryonArchiveHandle handle, const char* archive_path) {
    if (!handle || !archive_path) return false;
    
    KryonArchive* archive = (KryonArchive*)handle;
    
    FILE* file = fopen(archive_path, "wb");
    if (!file) return false;
    
    // Write simple archive format:
    // [4 bytes: entry count]
    // For each entry:
    //   [4 bytes: name length]
    //   [name_length bytes: name]
    //   [1 byte: is_directory]
    //   [8 bytes: modified_time]
    //   [4 bytes: size]
    //   [4 bytes: compressed_size]
    //   [compressed_size bytes: data (if not directory)]
    
    uint32_t entry_count = archive->entry_count;
    if (fwrite(&entry_count, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    KryonArchiveEntry* entry = archive->entries;
    while (entry) {
        // Write name length and name
        uint32_t name_len = strlen(entry->name);
        if (fwrite(&name_len, sizeof(uint32_t), 1, file) != 1 ||
            fwrite(entry->name, 1, name_len, file) != name_len) {
            fclose(file);
            return false;
        }
        
        // Write metadata
        uint8_t is_dir = entry->is_directory ? 1 : 0;
        uint64_t mod_time = entry->modified_time;
        uint32_t size = entry->size;
        uint32_t comp_size = entry->compressed_size;
        
        if (fwrite(&is_dir, sizeof(uint8_t), 1, file) != 1 ||
            fwrite(&mod_time, sizeof(uint64_t), 1, file) != 1 ||
            fwrite(&size, sizeof(uint32_t), 1, file) != 1 ||
            fwrite(&comp_size, sizeof(uint32_t), 1, file) != 1) {
            fclose(file);
            return false;
        }
        
        // Write data (if not directory)
        if (!entry->is_directory && entry->data && entry->size > 0) {
            // For simplicity, store uncompressed for now
            if (fwrite(entry->data, 1, entry->size, file) != entry->size) {
                fclose(file);
                return false;
            }
        }
        
        entry = entry->next;
    }
    
    fclose(file);
    return true;
}

KryonArchiveHandle kryon_archive_load(const char* archive_path) {
    if (!archive_path) return NULL;
    
    FILE* file = fopen(archive_path, "rb");
    if (!file) return NULL;
    
    KryonArchive* archive = kryon_alloc(sizeof(KryonArchive));
    if (!archive) {
        fclose(file);
        return NULL;
    }
    memset(archive, 0, sizeof(KryonArchive));
    
    // Read entry count
    uint32_t entry_count;
    if (fread(&entry_count, sizeof(uint32_t), 1, file) != 1) {
        kryon_free(archive);
        fclose(file);
        return NULL;
    }
    
    // Read entries
    for (uint32_t i = 0; i < entry_count; i++) {
        // Read name length and name
        uint32_t name_len;
        if (fread(&name_len, sizeof(uint32_t), 1, file) != 1) {
            kryon_archive_destroy((KryonArchiveHandle)archive);
            fclose(file);
            return NULL;
        }
        
        char* name = kryon_alloc(name_len + 1);
        if (!name || fread(name, 1, name_len, file) != name_len) {
            kryon_free(name);
            kryon_archive_destroy((KryonArchiveHandle)archive);
            fclose(file);
            return NULL;
        }
        name[name_len] = '\0';
        
        // Read metadata
        uint8_t is_dir;
        uint64_t mod_time;
        uint32_t size;
        uint32_t comp_size;
        
        if (fread(&is_dir, sizeof(uint8_t), 1, file) != 1 ||
            fread(&mod_time, sizeof(uint64_t), 1, file) != 1 ||
            fread(&size, sizeof(uint32_t), 1, file) != 1 ||
            fread(&comp_size, sizeof(uint32_t), 1, file) != 1) {
            kryon_free(name);
            kryon_archive_destroy((KryonArchiveHandle)archive);
            fclose(file);
            return NULL;
        }
        
        // Create entry
        KryonArchiveEntry* entry = kryon_alloc(sizeof(KryonArchiveEntry));
        if (!entry) {
            kryon_free(name);
            kryon_archive_destroy((KryonArchiveHandle)archive);
            fclose(file);
            return NULL;
        }
        
        entry->name = name;
        entry->size = size;
        entry->compressed_size = comp_size;
        entry->is_directory = is_dir != 0;
        entry->modified_time = mod_time;
        entry->data = NULL;
        
        // Read data if not directory
        if (!entry->is_directory && size > 0) {
            entry->data = kryon_alloc(size);
            if (!entry->data || fread(entry->data, 1, size, file) != size) {
                kryon_free(entry->name);
                kryon_free(entry->data);
                kryon_free(entry);
                kryon_archive_destroy((KryonArchiveHandle)archive);
                fclose(file);
                return NULL;
            }
        }
        
        entry->next = archive->entries;
        archive->entries = entry;
        archive->entry_count++;
        archive->total_size += size;
    }
    
    fclose(file);
    return (KryonArchiveHandle)archive;
}

bool kryon_archive_extract_all(KryonArchiveHandle handle, const char* output_dir) {
    if (!handle || !output_dir) return false;
    
    KryonArchive* archive = (KryonArchive*)handle;
    
    // Create output directory if it doesn't exist
    kryon_directory_create_recursive(output_dir);
    
    KryonArchiveEntry* entry = archive->entries;
    while (entry) {
        char* output_path = kryon_path_join(output_dir, entry->name);
        if (!output_path) {
            return false;
        }
        
        if (entry->is_directory) {
            kryon_directory_create_recursive(output_path);
        } else if (entry->data && entry->size > 0) {
            // Create parent directory
            char* parent_dir = kryon_path_dirname(output_path);
            if (parent_dir) {
                kryon_directory_create_recursive(parent_dir);
                kryon_free(parent_dir);
            }
            
            // Write file
            FILE* output_file = fopen(output_path, "wb");
            if (output_file) {
                fwrite(entry->data, 1, entry->size, output_file);
                fclose(output_file);
            }
        }
        
        kryon_free(output_path);
        entry = entry->next;
    }
    
    return true;
}

size_t kryon_archive_get_entry_count(KryonArchiveHandle handle) {
    if (!handle) return 0;
    
    KryonArchive* archive = (KryonArchive*)handle;
    return archive->entry_count;
}

KryonArchiveInfo kryon_archive_get_info(KryonArchiveHandle handle) {
    KryonArchiveInfo info = {0};
    
    if (!handle) return info;
    
    KryonArchive* archive = (KryonArchive*)handle;
    info.entry_count = archive->entry_count;
    info.total_size = archive->total_size;
    info.compressed_size = archive->compressed_size;
    
    return info;
}