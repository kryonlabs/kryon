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

/**
 * Parse inline table syntax: { key1 = value1, key2 = value2 }
 * Adds entries to the TOML table with the format: base_key.inner_key
 */
static void parse_inline_table(TOMLTable* table, const char* base_key, const char* inline_table, int line_num) {
    if (!table || !base_key || !inline_table) return;

    // Find the opening and closing braces
    const char* start = strchr(inline_table, '{');
    const char* end = strrchr(inline_table, '}');

    if (!start || !end || end <= start) return;

    // Extract the content between braces
    size_t content_len = end - start - 1;
    char* content = (char*)malloc(content_len + 1);
    if (!content) return;

    strncpy(content, start + 1, content_len);
    content[content_len] = '\0';

    // Parse comma-separated key-value pairs
    char* pair_start = content;
    char* pair_end;

    while (*pair_start) {
        // Find the next comma or end of string
        pair_end = strchr(pair_start, ',');
        if (pair_end) {
            *pair_end = '\0';
        }

        // Parse this key-value pair
        char* equals = strchr(pair_start, '=');
        if (equals) {
            *equals = '\0';
            char* key = trim_whitespace(pair_start);
            char* value = trim_whitespace(equals + 1);

            // Remove quotes from value
            value = remove_quotes(value);

            // Build full key: base_key.inner_key
            char full_key[2048];
            snprintf(full_key, sizeof(full_key), "%s.%s", base_key, key);

            toml_table_add(table, full_key, value, line_num);
        }

        if (!pair_end) break;
        pair_start = pair_end + 1;
    }

    free(content);
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
TOMLTable* kryon_toml_parse_file(const char* path) {
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

                // Build full key with section prefix
                char full_key[1024];
                if (current_section[0]) {
                    snprintf(full_key, sizeof(full_key), "%s.%s", current_section, key);
                } else {
                    strncpy(full_key, key, sizeof(full_key) - 1);
                    full_key[sizeof(full_key) - 1] = '\0';
                }

                // Check if value is an inline table { ... }
                if (value[0] == '{') {
                    parse_inline_table(table, full_key, value, line_num);
                }
                // Check if value is an array [ ... ]
                else if (value[0] == '[' && value[strlen(value)-1] == ']') {
                    // Parse array elements
                    char* array_content = str_copy(value + 1);  // Skip opening [
                    array_content[strlen(array_content) - 1] = '\0';  // Remove closing ]

                    // Parse array elements, handling nested braces
                    char* elem_start = array_content;
                    int idx = 0;
                    while (*elem_start) {
                        // Skip leading whitespace
                        while (*elem_start && (*elem_start == ' ' || *elem_start == '\t')) {
                            elem_start++;
                        }
                        if (!*elem_start) break;

                        char* elem_end = NULL;
                        if (*elem_start == '{') {
                            // Find matching closing brace
                            int brace_depth = 0;
                            char* p = elem_start;
                            while (*p) {
                                if (*p == '{') brace_depth++;
                                else if (*p == '}') {
                                    brace_depth--;
                                    if (brace_depth == 0) {
                                        elem_end = p + 1;
                                        break;
                                    }
                                }
                                p++;
                            }
                            if (brace_depth != 0) break;  // Malformed

                            // Extract inline table
                            size_t elem_len = elem_end - elem_start;
                            char* elem = (char*)malloc(elem_len + 1);
                            strncpy(elem, elem_start, elem_len);
                            elem[elem_len] = '\0';

                            // Parse as inline table with indexed key
                            char indexed_key[2048];
                            snprintf(indexed_key, sizeof(indexed_key), "%s.%d", full_key, idx);
                            parse_inline_table(table, indexed_key, elem, line_num);
                            free(elem);

                            // Move past the inline table and any trailing comma
                            elem_start = elem_end;
                            while (*elem_start && (*elem_start == ' ' || *elem_start == ',' || *elem_start == '\t')) {
                                elem_start++;
                            }
                        } else {
                            // Simple value - find comma or end
                            char* comma = strchr(elem_start, ',');
                            if (comma) {
                                *comma = '\0';
                                elem_end = comma + 1;
                            } else {
                                elem_end = elem_start + strlen(elem_start);
                            }

                            char* elem = trim_whitespace(elem_start);
                            elem = remove_quotes(elem);

                            // Add as full_key.N
                            char indexed_key[2048];
                            snprintf(indexed_key, sizeof(indexed_key), "%s.%d", full_key, idx);
                            toml_table_add(table, indexed_key, elem, line_num);

                            elem_start = elem_end;
                        }
                        idx++;
                    }
                    free(array_content);
                } else {
                    // Remove quotes from simple value
                    value = remove_quotes(value);
                    toml_table_add(table, full_key, value, line_num);
                }
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
const char* kryon_toml_get_string(TOMLTable* table, const char* key, const char* default_value) {
    const char* value = toml_table_get(table, key);
    return value ? value : default_value;
}

/**
 * Get integer value from TOML table
 */
int kryon_toml_get_int(TOMLTable* table, const char* key, int default_value) {
    const char* value = toml_table_get(table, key);
    return value ? atoi(value) : default_value;
}

/**
 * Get boolean value from TOML table
 */
bool kryon_toml_get_bool(TOMLTable* table, const char* key, bool default_value) {
    const char* value = toml_table_get(table, key);
    if (!value) return default_value;

    return strcmp(value, "true") == 0 || strcmp(value, "1") == 0;
}

/**
 * Get all unique plugin names from TOML table
 * Returns array of plugin names and sets count
 * Caller must free the returned array and each string
 */
char** kryon_toml_get_plugin_names(TOMLTable* table, int* count) {
    if (!table || !count) return NULL;

    *count = 0;

    // First pass: count unique plugin names
    char** temp_names = (char**)malloc(table->count * sizeof(char*));
    if (!temp_names) return NULL;

    for (int i = 0; i < table->count; i++) {
        const char* key = table->entries[i].key;

        // Check if key starts with "plugins."
        if (strncmp(key, "plugins.", 8) != 0) continue;

        // Extract plugin name (between "plugins." and next ".")
        const char* name_start = key + 8;
        const char* name_end = strchr(name_start, '.');

        if (!name_end) continue;  // Skip if no second dot

        // Extract the plugin name
        size_t name_len = name_end - name_start;
        char* plugin_name = (char*)malloc(name_len + 1);
        if (!plugin_name) continue;

        strncpy(plugin_name, name_start, name_len);
        plugin_name[name_len] = '\0';

        // Check if we already have this plugin name
        bool found = false;
        for (int j = 0; j < *count; j++) {
            if (strcmp(temp_names[j], plugin_name) == 0) {
                found = true;
                free(plugin_name);
                break;
            }
        }

        if (!found) {
            temp_names[*count] = plugin_name;
            (*count)++;
        }
    }

    // Allocate final array with exact size
    if (*count == 0) {
        free(temp_names);
        return NULL;
    }

    char** names = (char**)malloc(*count * sizeof(char*));
    if (!names) {
        for (int i = 0; i < *count; i++) {
            free(temp_names[i]);
        }
        free(temp_names);
        *count = 0;
        return NULL;
    }

    memcpy(names, temp_names, *count * sizeof(char*));
    free(temp_names);

    return names;
}

/**
 * Free TOML table
 */
void kryon_toml_free(TOMLTable* table) {
    toml_table_free(table);
}
