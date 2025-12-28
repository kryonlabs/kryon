#include "android_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// ============================================================================
// Storage Entry Structure
// ============================================================================

typedef struct {
    bool used;
    char key[KRYON_ANDROID_MAX_KEY_LEN];
    uint8_t* value;
    size_t value_size;
    uint32_t checksum;
} kryon_android_storage_entry_t;

// ============================================================================
// Storage State
// ============================================================================

typedef struct {
    bool initialized;
    kryon_android_storage_entry_t entries[KRYON_ANDROID_MAX_ENTRIES];
    size_t entry_count;
    char storage_file_path[512];
} kryon_android_storage_state_t;

static kryon_android_storage_state_t g_storage_state = {0};

// ============================================================================
// Internal Helper Functions
// ============================================================================

static uint32_t calculate_checksum(const void* data, size_t size) {
    // Simple checksum (CRC32-like)
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0xFFFFFFFF;

    for (size_t i = 0; i < size; i++) {
        checksum ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            if (checksum & 1) {
                checksum = (checksum >> 1) ^ 0xEDB88320;
            } else {
                checksum >>= 1;
            }
        }
    }

    return ~checksum;
}

static int find_entry_by_key(const char* key) {
    if (!key) return -1;

    for (size_t i = 0; i < KRYON_ANDROID_MAX_ENTRIES; i++) {
        if (g_storage_state.entries[i].used &&
            strcmp(g_storage_state.entries[i].key, key) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static int find_free_entry(void) {
    for (size_t i = 0; i < KRYON_ANDROID_MAX_ENTRIES; i++) {
        if (!g_storage_state.entries[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static bool save_storage_to_file(void) {
    FILE* fp = fopen(g_storage_state.storage_file_path, "wb");
    if (!fp) {
        KRYON_ANDROID_LOG_ERROR("Failed to open storage file for writing: %s\n",
                                g_storage_state.storage_file_path);
        return false;
    }

    // Write header
    uint32_t magic = 0x4B52594E;  // 'KRYN'
    uint32_t version = 1;
    uint32_t entry_count = 0;

    // Count used entries
    for (size_t i = 0; i < KRYON_ANDROID_MAX_ENTRIES; i++) {
        if (g_storage_state.entries[i].used) {
            entry_count++;
        }
    }

    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&version, sizeof(version), 1, fp);
    fwrite(&entry_count, sizeof(entry_count), 1, fp);

    // Write entries
    for (size_t i = 0; i < KRYON_ANDROID_MAX_ENTRIES; i++) {
        if (g_storage_state.entries[i].used) {
            kryon_android_storage_entry_t* entry = &g_storage_state.entries[i];

            // Write key length and key
            uint32_t key_len = (uint32_t)strlen(entry->key);
            fwrite(&key_len, sizeof(key_len), 1, fp);
            fwrite(entry->key, 1, key_len, fp);

            // Write value size and value
            uint32_t value_size = (uint32_t)entry->value_size;
            fwrite(&value_size, sizeof(value_size), 1, fp);
            fwrite(entry->value, 1, entry->value_size, fp);

            // Write checksum
            fwrite(&entry->checksum, sizeof(entry->checksum), 1, fp);
        }
    }

    fclose(fp);
    KRYON_ANDROID_LOG_DEBUG("Saved %u entries to storage file\n", entry_count);
    return true;
}

static bool load_storage_from_file(void) {
    FILE* fp = fopen(g_storage_state.storage_file_path, "rb");
    if (!fp) {
        // File doesn't exist yet, not an error
        KRYON_ANDROID_LOG_INFO("Storage file not found, starting fresh\n");
        return true;
    }

    // Read and verify header
    uint32_t magic, version, entry_count;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&version, sizeof(version), 1, fp) != 1 ||
        fread(&entry_count, sizeof(entry_count), 1, fp) != 1) {
        KRYON_ANDROID_LOG_ERROR("Failed to read storage header\n");
        fclose(fp);
        return false;
    }

    if (magic != 0x4B52594E) {
        KRYON_ANDROID_LOG_ERROR("Invalid storage file magic\n");
        fclose(fp);
        return false;
    }

    if (version != 1) {
        KRYON_ANDROID_LOG_WARN("Unknown storage version: %u\n", version);
    }

    KRYON_ANDROID_LOG_INFO("Loading %u entries from storage\n", entry_count);

    // Read entries
    for (uint32_t i = 0; i < entry_count && i < KRYON_ANDROID_MAX_ENTRIES; i++) {
        // Read key
        uint32_t key_len;
        if (fread(&key_len, sizeof(key_len), 1, fp) != 1) {
            KRYON_ANDROID_LOG_ERROR("Failed to read key length\n");
            break;
        }

        if (key_len >= KRYON_ANDROID_MAX_KEY_LEN) {
            KRYON_ANDROID_LOG_ERROR("Key too long: %u\n", key_len);
            break;
        }

        char key[KRYON_ANDROID_MAX_KEY_LEN];
        if (fread(key, 1, key_len, fp) != key_len) {
            KRYON_ANDROID_LOG_ERROR("Failed to read key\n");
            break;
        }
        key[key_len] = '\0';

        // Read value
        uint32_t value_size;
        if (fread(&value_size, sizeof(value_size), 1, fp) != 1) {
            KRYON_ANDROID_LOG_ERROR("Failed to read value size\n");
            break;
        }

        if (value_size > KRYON_ANDROID_MAX_VALUE_SIZE) {
            KRYON_ANDROID_LOG_ERROR("Value too large: %u\n", value_size);
            break;
        }

        uint8_t* value = (uint8_t*)malloc(value_size);
        if (!value) {
            KRYON_ANDROID_LOG_ERROR("Failed to allocate value buffer\n");
            break;
        }

        if (fread(value, 1, value_size, fp) != value_size) {
            KRYON_ANDROID_LOG_ERROR("Failed to read value\n");
            free(value);
            break;
        }

        // Read and verify checksum
        uint32_t stored_checksum;
        if (fread(&stored_checksum, sizeof(stored_checksum), 1, fp) != 1) {
            KRYON_ANDROID_LOG_ERROR("Failed to read checksum\n");
            free(value);
            break;
        }

        uint32_t calculated_checksum = calculate_checksum(value, value_size);
        if (stored_checksum != calculated_checksum) {
            KRYON_ANDROID_LOG_WARN("Checksum mismatch for key '%s'\n", key);
            free(value);
            continue;
        }

        // Find free slot and store entry
        int slot = find_free_entry();
        if (slot >= 0) {
            g_storage_state.entries[slot].used = true;
            strncpy(g_storage_state.entries[slot].key, key, KRYON_ANDROID_MAX_KEY_LEN - 1);
            g_storage_state.entries[slot].value = value;
            g_storage_state.entries[slot].value_size = value_size;
            g_storage_state.entries[slot].checksum = stored_checksum;
            g_storage_state.entry_count++;
        } else {
            KRYON_ANDROID_LOG_WARN("No free slots for key '%s'\n", key);
            free(value);
        }
    }

    fclose(fp);
    KRYON_ANDROID_LOG_INFO("Loaded %zu entries successfully\n", g_storage_state.entry_count);
    return true;
}

// ============================================================================
// Public API Implementation
// ============================================================================

bool kryon_android_init_storage(void) {
    if (g_storage_state.initialized) {
        return true;
    }

    memset(&g_storage_state, 0, sizeof(g_storage_state));

    // Construct storage file path
    const char* storage_path = kryon_android_get_internal_storage_path();
    snprintf(g_storage_state.storage_file_path,
             sizeof(g_storage_state.storage_file_path),
             "%s/kryon_storage.dat", storage_path);

    KRYON_ANDROID_LOG_INFO("Storage file: %s\n", g_storage_state.storage_file_path);

    // Ensure directory exists
#ifdef __ANDROID__
    mkdir(storage_path, 0755);
#endif

    // Load existing storage
    if (!load_storage_from_file()) {
        KRYON_ANDROID_LOG_WARN("Failed to load storage, starting fresh\n");
    }

    g_storage_state.initialized = true;
    return true;
}

bool kryon_android_storage_set(const char* key, const void* data, size_t size) {
    if (!g_storage_state.initialized) {
        KRYON_ANDROID_LOG_ERROR("Storage not initialized\n");
        return false;
    }

    if (!key || !data || size == 0) {
        KRYON_ANDROID_LOG_ERROR("Invalid parameters\n");
        return false;
    }

    if (strlen(key) >= KRYON_ANDROID_MAX_KEY_LEN) {
        KRYON_ANDROID_LOG_ERROR("Key too long: %s\n", key);
        return false;
    }

    if (size > KRYON_ANDROID_MAX_VALUE_SIZE) {
        KRYON_ANDROID_LOG_ERROR("Value too large: %zu bytes\n", size);
        return false;
    }

    // Check if key already exists
    int existing = find_entry_by_key(key);
    if (existing >= 0) {
        // Update existing entry
        free(g_storage_state.entries[existing].value);

        g_storage_state.entries[existing].value = (uint8_t*)malloc(size);
        if (!g_storage_state.entries[existing].value) {
            KRYON_ANDROID_LOG_ERROR("Failed to allocate value buffer\n");
            g_storage_state.entries[existing].used = false;
            g_storage_state.entry_count--;
            return false;
        }

        memcpy(g_storage_state.entries[existing].value, data, size);
        g_storage_state.entries[existing].value_size = size;
        g_storage_state.entries[existing].checksum = calculate_checksum(data, size);

        KRYON_ANDROID_LOG_DEBUG("Updated storage entry: %s (%zu bytes)\n", key, size);
    } else {
        // Create new entry
        int slot = find_free_entry();
        if (slot < 0) {
            KRYON_ANDROID_LOG_ERROR("Storage full (max %d entries)\n", KRYON_ANDROID_MAX_ENTRIES);
            return false;
        }

        g_storage_state.entries[slot].value = (uint8_t*)malloc(size);
        if (!g_storage_state.entries[slot].value) {
            KRYON_ANDROID_LOG_ERROR("Failed to allocate value buffer\n");
            return false;
        }

        g_storage_state.entries[slot].used = true;
        strncpy(g_storage_state.entries[slot].key, key, KRYON_ANDROID_MAX_KEY_LEN - 1);
        memcpy(g_storage_state.entries[slot].value, data, size);
        g_storage_state.entries[slot].value_size = size;
        g_storage_state.entries[slot].checksum = calculate_checksum(data, size);
        g_storage_state.entry_count++;

        KRYON_ANDROID_LOG_DEBUG("Created storage entry: %s (%zu bytes)\n", key, size);
    }

    // Persist to file
    return save_storage_to_file();
}

bool kryon_android_storage_get(const char* key, void* buffer, size_t* size) {
    if (!g_storage_state.initialized) {
        KRYON_ANDROID_LOG_ERROR("Storage not initialized\n");
        return false;
    }

    if (!key || !buffer || !size) {
        KRYON_ANDROID_LOG_ERROR("Invalid parameters\n");
        return false;
    }

    int index = find_entry_by_key(key);
    if (index < 0) {
        KRYON_ANDROID_LOG_DEBUG("Key not found: %s\n", key);
        return false;
    }

    kryon_android_storage_entry_t* entry = &g_storage_state.entries[index];

    // Verify checksum
    uint32_t calculated = calculate_checksum(entry->value, entry->value_size);
    if (calculated != entry->checksum) {
        KRYON_ANDROID_LOG_ERROR("Checksum mismatch for key: %s\n", key);
        return false;
    }

    if (*size < entry->value_size) {
        KRYON_ANDROID_LOG_ERROR("Buffer too small: need %zu, have %zu\n",
                                entry->value_size, *size);
        *size = entry->value_size;
        return false;
    }

    memcpy(buffer, entry->value, entry->value_size);
    *size = entry->value_size;

    KRYON_ANDROID_LOG_DEBUG("Retrieved storage entry: %s (%zu bytes)\n", key, *size);
    return true;
}

bool kryon_android_storage_has(const char* key) {
    if (!g_storage_state.initialized) {
        return false;
    }

    if (!key) {
        return false;
    }

    return find_entry_by_key(key) >= 0;
}

bool kryon_android_storage_remove(const char* key) {
    if (!g_storage_state.initialized) {
        KRYON_ANDROID_LOG_ERROR("Storage not initialized\n");
        return false;
    }

    if (!key) {
        return false;
    }

    int index = find_entry_by_key(key);
    if (index < 0) {
        KRYON_ANDROID_LOG_DEBUG("Key not found: %s\n", key);
        return false;
    }

    // Free value memory
    free(g_storage_state.entries[index].value);

    // Mark entry as unused
    memset(&g_storage_state.entries[index], 0, sizeof(kryon_android_storage_entry_t));
    g_storage_state.entry_count--;

    KRYON_ANDROID_LOG_DEBUG("Removed storage entry: %s\n", key);

    // Persist to file
    return save_storage_to_file();
}

void kryon_android_storage_clear(void) {
    if (!g_storage_state.initialized) {
        return;
    }

    // Free all value buffers
    for (size_t i = 0; i < KRYON_ANDROID_MAX_ENTRIES; i++) {
        if (g_storage_state.entries[i].used) {
            free(g_storage_state.entries[i].value);
        }
    }

    // Clear all entries
    memset(g_storage_state.entries, 0, sizeof(g_storage_state.entries));
    g_storage_state.entry_count = 0;

    // Save empty storage
    save_storage_to_file();

    KRYON_ANDROID_LOG_INFO("Storage cleared\n");
}

// ============================================================================
// File Storage Implementation
// ============================================================================

bool kryon_android_file_write(const char* filename, const void* data, size_t size) {
    if (!filename || !data || size == 0) {
        KRYON_ANDROID_LOG_ERROR("Invalid parameters\n");
        return false;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s",
             kryon_android_get_internal_storage_path(), filename);

    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        KRYON_ANDROID_LOG_ERROR("Failed to open file for writing: %s (%s)\n",
                                filepath, strerror(errno));
        return false;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        KRYON_ANDROID_LOG_ERROR("Failed to write complete data: %zu/%zu bytes\n",
                                written, size);
        return false;
    }

    KRYON_ANDROID_LOG_DEBUG("Wrote file: %s (%zu bytes)\n", filename, size);
    return true;
}

bool kryon_android_file_read(const char* filename, void** data, size_t* size) {
    if (!filename || !data || !size) {
        KRYON_ANDROID_LOG_ERROR("Invalid parameters\n");
        return false;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s",
             kryon_android_get_internal_storage_path(), filename);

    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        KRYON_ANDROID_LOG_ERROR("Failed to open file for reading: %s (%s)\n",
                                filepath, strerror(errno));
        return false;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        KRYON_ANDROID_LOG_ERROR("Failed to get file size\n");
        fclose(fp);
        return false;
    }

    // Allocate buffer
    *data = malloc((size_t)file_size);
    if (!*data) {
        KRYON_ANDROID_LOG_ERROR("Failed to allocate read buffer\n");
        fclose(fp);
        return false;
    }

    // Read file
    size_t read = fread(*data, 1, (size_t)file_size, fp);
    fclose(fp);

    if (read != (size_t)file_size) {
        KRYON_ANDROID_LOG_ERROR("Failed to read complete file: %zu/%ld bytes\n",
                                read, file_size);
        free(*data);
        *data = NULL;
        return false;
    }

    *size = (size_t)file_size;
    KRYON_ANDROID_LOG_DEBUG("Read file: %s (%zu bytes)\n", filename, *size);
    return true;
}

bool kryon_android_file_exists(const char* filename) {
    if (!filename) {
        return false;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s",
             kryon_android_get_internal_storage_path(), filename);

    return access(filepath, F_OK) == 0;
}

bool kryon_android_file_delete(const char* filename) {
    if (!filename) {
        return false;
    }

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s",
             kryon_android_get_internal_storage_path(), filename);

    if (unlink(filepath) != 0) {
        KRYON_ANDROID_LOG_ERROR("Failed to delete file: %s (%s)\n",
                                filepath, strerror(errno));
        return false;
    }

    KRYON_ANDROID_LOG_DEBUG("Deleted file: %s\n", filename);
    return true;
}
