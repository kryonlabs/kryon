/**
 * DIS Link Section Generator for TaijiOS
 *
 * Generates export/import tables for TaijiOS .dis files.
 * Based on /home/wao/Projects/TaijiOS/libinterp/load.c
 */

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "internal.h"

/**
 * Create a new link entry (export)
 */
DISLinkEntry* link_entry_create(const char* name, uint32_t pc, uint32_t type, uint8_t sig) {
    DISLinkEntry* entry = (DISLinkEntry*)calloc(1, sizeof(DISLinkEntry));
    if (!entry) {
        return NULL;
    }

    entry->name = strdup(name);
    if (!entry->name) {
        free(entry);
        return NULL;
    }

    entry->pc = pc;
    entry->type = type;
    entry->sig = sig;

    return entry;
}

/**
 * Destroy a link entry
 */
void link_entry_destroy(DISLinkEntry* entry) {
    if (entry) {
        free(entry->name);
        free(entry);
    }
}

/**
 * Create a new import entry
 */
DISImportEntry* import_entry_create(const char* module, const char* name, uint32_t offset) {
    DISImportEntry* entry = (DISImportEntry*)calloc(1, sizeof(DISImportEntry));
    if (!entry) {
        return NULL;
    }

    entry->module = strdup(module);
    entry->name = strdup(name);
    if (!entry->module || !entry->name) {
        free(entry->module);
        free(entry->name);
        free(entry);
        return NULL;
    }

    entry->offset = offset;

    return entry;
}

/**
 * Destroy an import entry
 */
void import_entry_destroy(DISImportEntry* entry) {
    if (entry) {
        free(entry->module);
        free(entry->name);
        free(entry);
    }
}

/**
 * Add an export to the link section
 * @param builder Module builder
 * @param name Symbol name
 * @param pc Program counter (function entry point)
 * @param type Type index
 * @param sig Function signature
 * @return true on success
 */
bool add_export(DISModuleBuilder* builder, const char* name, uint32_t pc, uint32_t type, uint8_t sig) {
    if (!builder || !name) {
        return false;
    }

    DISLinkEntry* entry = link_entry_create(name, pc, type, sig);
    if (!entry) {
        return false;
    }

    if (!dynamic_array_add(builder->link_section, entry)) {
        link_entry_destroy(entry);
        return false;
    }

    // Also add to function table for quick lookup
    module_builder_add_function(builder, name, pc);

    return true;
}

/**
 * Add an import to the import section
 * @param builder Module builder
 * @param module Module name to import from
 * @param name Symbol name to import
 * @param offset Offset in code section to patch
 * @return true on success
 */
bool add_import(DISModuleBuilder* builder, const char* module, const char* name, uint32_t offset) {
    if (!builder || !module || !name) {
        return false;
    }

    DISImportEntry* entry = import_entry_create(module, name, offset);
    if (!entry) {
        return false;
    }

    if (!dynamic_array_add(builder->import_section, entry)) {
        import_entry_destroy(entry);
        return false;
    }

    return true;
}

/**
 * Write link section (exports) to file
 * Based on TaijiOS format: PC, type, sig, name (null-terminated)
 */
bool write_link_section(DISModuleBuilder* builder) {
    if (!builder || !module_builder_get_output_file(builder) || !builder->link_section) {
        return false;
    }

    for (size_t i = 0; i < builder->link_section->count; i++) {
        DISLinkEntry* entry = (DISLinkEntry*)builder->link_section->items[i];
        if (!entry) {
            continue;
        }

        // Write PC
        write_operand(module_builder_get_output_file(builder), (int32_t)entry->pc);

        // Write type index
        write_operand(module_builder_get_output_file(builder), (int32_t)entry->type);

        // Write signature
        write_operand(module_builder_get_output_file(builder), (int32_t)entry->sig);

        // Write name (null-terminated string)
        fputs(entry->name, module_builder_get_output_file(builder));
        fputc(0, module_builder_get_output_file(builder));
    }

    return true;
}

/**
 * Write import section to file
 * Based on TaijiOS format: module name, symbol name, offset
 */
bool write_import_section(DISModuleBuilder* builder) {
    if (!builder || !module_builder_get_output_file(builder) || !builder->import_section) {
        return false;
    }

    for (size_t i = 0; i < builder->import_section->count; i++) {
        DISImportEntry* entry = (DISImportEntry*)builder->import_section->items[i];
        if (!entry) {
            continue;
        }

        // Write module name (null-terminated)
        fputs(entry->module, module_builder_get_output_file(builder));
        fputc(0, module_builder_get_output_file(builder));

        // Write symbol name (null-terminated)
        fputs(entry->name, module_builder_get_output_file(builder));
        fputc(0, module_builder_get_output_file(builder));

        // Write offset to patch
        write_operand(module_builder_get_output_file(builder), (int32_t)entry->offset);
    }

    return true;
}
