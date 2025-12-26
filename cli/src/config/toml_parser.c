/**
 * TOML Parser
 * Simple TOML parser for kryon.toml configuration files
 * Supports strings, booleans, integers, arrays, and tables
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct {
    char* key;
    char* value;
    int line;
} TOMLEntry;

struct TOMLTable {
    TOMLEntry* entries;
    int count;
    int capacity;
};

static char* trim_whitespace(char* str) {
    // Trim leading
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    // Trim trailing
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

static char* remove_quotes(char* str) {
    size_t len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len-1] == '"') ||
                     (str[0] == '\'' && str[len-1] == '\''))) {
        str[len-1] = '\0';
        return str + 1;
    }
    return str;
}

static TOMLTable* toml_table_create(void) {
    TOMLTable* table = (TOMLTable*)malloc(sizeof(TOMLTable));
    if (!table) return NULL;

    table->capacity = 32;
    table->count = 0;
    table->entries = (TOMLEntry*)malloc(table->capacity * sizeof(TOMLEntry));

    if (!table->entries) {
        free(table);
        return NULL;
    }

    return table;
}

static void toml_table_add(TOMLTable* table, const char* key, const char* value, int line) {
    if (!table || !key || !value) return;

    if (table->count >= table->capacity) {
        table->capacity *= 2;
        TOMLEntry* new_entries = (TOMLEntry*)realloc(table->entries,
                                                     table->capacity * sizeof(TOMLEntry));
        if (!new_entries) return;
        table->entries = new_entries;
    }

    table->entries[table->count].key = str_copy(key);
    table->entries[table->count].value = str_copy(value);
    table->entries[table->count].line = line;
    table->count++;
}

static const char* toml_table_get(TOMLTable* table, const char* key) {
    if (!table || !key) return NULL;

    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].key, key) == 0) {
            return table->entries[i].value;
        }
    }

    return NULL;
}

static void toml_table_free(TOMLTable* table) {
    if (!table) return;

    for (int i = 0; i < table->count; i++) {
        free(table->entries[i].key);
        free(table->entries[i].value);
    }

    free(table->entries);
    free(table);
}

/**
 * Parse TOML file into table
 * Returns NULL on error
 */
TOMLTable* toml_parse_file(const char* path) {
    if (!path) return NULL;

    char* content = file_read(path);
    if (!content) return NULL;

    TOMLTable* table = toml_table_create();
    if (!table) {
        free(content);
        return NULL;
    }

    char* line_start = content;
    char* line_end;
    int line_num = 0;
    char current_section[256] = "";

    while (*line_start) {
        line_num++;

        // Find line end
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0';
        }

        char* line = trim_whitespace(line_start);

        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            if (!line_end) break;
            line_start = line_end + 1;
            continue;
        }

        // Handle section headers [section]
        if (line[0] == '[') {
            char* section_end = strchr(line, ']');
            if (section_end) {
                *section_end = '\0';
                strncpy(current_section, line + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
            }
        }
        // Handle key = value
        else {
            char* equals = strchr(line, '=');
            if (equals) {
                *equals = '\0';
                char* key = trim_whitespace(line);
                char* value = trim_whitespace(equals + 1);

                // Remove quotes from value
                value = remove_quotes(value);

                // Build full key with section prefix
                char full_key[512];
                if (current_section[0]) {
                    snprintf(full_key, sizeof(full_key), "%s.%s", current_section, key);
                } else {
                    strncpy(full_key, key, sizeof(full_key) - 1);
                    full_key[sizeof(full_key) - 1] = '\0';
                }

                toml_table_add(table, full_key, value, line_num);
            }
        }

        if (!line_end) break;
        line_start = line_end + 1;
    }

    free(content);
    return table;
}

/**
 * Get string value from TOML table
 */
const char* toml_get_string(TOMLTable* table, const char* key, const char* default_value) {
    const char* value = toml_table_get(table, key);
    return value ? value : default_value;
}

/**
 * Get integer value from TOML table
 */
int toml_get_int(TOMLTable* table, const char* key, int default_value) {
    const char* value = toml_table_get(table, key);
    return value ? atoi(value) : default_value;
}

/**
 * Get boolean value from TOML table
 */
bool toml_get_bool(TOMLTable* table, const char* key, bool default_value) {
    const char* value = toml_table_get(table, key);
    if (!value) return default_value;

    return strcmp(value, "true") == 0 || strcmp(value, "1") == 0;
}

/**
 * Free TOML table
 */
void toml_free(TOMLTable* table) {
    toml_table_free(table);
}
