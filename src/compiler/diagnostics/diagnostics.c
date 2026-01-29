/**

 * @file diagnostics.c
 * @brief Kryon Compiler Diagnostics System
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */
#include "lib9.h"


#include "diagnostics.h"
#include "memory.h"
#include <stdarg.h>
#include <time.h>

// =============================================================================
// DIAGNOSTIC SEVERITY COLORS (ANSI)
// =============================================================================

static const char *severity_colors[] = {
    "\033[0m",     // NOTE - normal
    "\033[1;33m",  // WARNING - yellow
    "\033[1;31m",  // ERROR - red
    "\033[1;35m",  // FATAL - magenta
    "\033[1;34m"   // INFO - blue
};

static const char *severity_names[] = {
    "note",
    "warning", 
    "error",
    "fatal",
    "info"
};

// =============================================================================
// DIAGNOSTIC MANAGER IMPLEMENTATION
// =============================================================================

KryonDiagnosticManager *kryon_diagnostic_create(void) {
    KryonDiagnosticManager *manager = kryon_alloc(sizeof(KryonDiagnosticManager));
    if (!manager) {
        return NULL;
    }
    
    memset(manager, 0, sizeof(KryonDiagnosticManager));
    
    // Initialize configuration
    manager->config.enable_colors = true;
    manager->config.show_source_context = true;
    manager->config.context_lines = 2;
    manager->config.max_diagnostics = 1000;
    manager->config.error_limit = 100;
    manager->config.warning_limit = 500;
    
    // Initialize counters
    manager->error_count = 0;
    manager->warning_count = 0;
    manager->note_count = 0;
    
    // Initialize diagnostic array
    manager->diagnostic_capacity = 64;
    manager->diagnostics = kryon_alloc(manager->diagnostic_capacity * sizeof(KryonDiagnostic));
    if (!manager->diagnostics) {
        kryon_free(manager);
        return NULL;
    }
    
    return manager;
}

void kryon_diagnostic_destroy(KryonDiagnosticManager *manager) {
    if (!manager) {
        return;
    }
    
    // Free all diagnostic messages
    for (size_t i = 0; i < manager->diagnostic_count; i++) {
        kryon_free(manager->diagnostics[i].message);
        kryon_free(manager->diagnostics[i].suggestion);
        kryon_free(manager->diagnostics[i].source_filename);
    }
    
    kryon_free(manager->diagnostics);
    kryon_free(manager);
}

static bool expand_diagnostic_array(KryonDiagnosticManager *manager) {
    if (manager->diagnostic_count >= manager->diagnostic_capacity) {
        size_t new_capacity = manager->diagnostic_capacity * 2;
        KryonDiagnostic *new_diagnostics = kryon_realloc(manager->diagnostics,
                                                        new_capacity * sizeof(KryonDiagnostic));
        if (!new_diagnostics) {
            return false;
        }
        
        manager->diagnostics = new_diagnostics;
        manager->diagnostic_capacity = new_capacity;
    }
    
    return true;
}

static char *duplicate_string(const char *str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char *copy = kryon_alloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

void kryon_diagnostic_report(KryonDiagnosticManager *manager,
                            KryonDiagnosticSeverity severity,
                            const KryonSourceLocation *location,
                            const char *message,
                            const char *suggestion) {
    if (!manager || !message) {
        return;
    }
    
    // Check limits
    if (manager->diagnostic_count >= manager->config.max_diagnostics) {
        return;
    }
    
    if (severity == KRYON_DIAG_ERROR && 
        manager->error_count >= manager->config.error_limit) {
        return;
    }
    
    if (severity == KRYON_DIAG_WARNING && 
        manager->warning_count >= manager->config.warning_limit) {
        return;
    }
    
    // Expand array if needed
    if (!expand_diagnostic_array(manager)) {
        return;
    }
    
    // Create diagnostic
    KryonDiagnostic *diag = &manager->diagnostics[manager->diagnostic_count];
    memset(diag, 0, sizeof(KryonDiagnostic));
    
    diag->severity = severity;
    diag->message = duplicate_string(message);
    diag->suggestion = suggestion ? duplicate_string(suggestion) : NULL;
    diag->timestamp = time(NULL);
    
    // Copy location info
    if (location) {
        diag->location = *location;
        diag->source_filename = duplicate_string(location->filename);
    }
    
    // Update counters
    switch (severity) {
        case KRYON_DIAG_ERROR:
        case KRYON_DIAG_FATAL:
            manager->error_count++;
            break;
        case KRYON_DIAG_WARNING:
            manager->warning_count++;
            break;
        case KRYON_DIAG_NOTE:
        case KRYON_DIAG_INFO:
            manager->note_count++;
            break;
    }
    
    manager->diagnostic_count++;
}

void kryon_diagnostic_reportf(KryonDiagnosticManager *manager,
                             KryonDiagnosticSeverity severity,
                             const KryonSourceLocation *location,
                             const char *format, ...) {
    if (!manager || !format) {
        return;
    }
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    kryon_diagnostic_report(manager, severity, location, buffer, NULL);
}

// =============================================================================
// DIAGNOSTIC OUTPUT FORMATTING
// =============================================================================

static void print_source_context(FILE *file, const KryonDiagnostic *diag,
                                 const KryonDiagnosticConfig *config) {
    if (!config->show_source_context || !diag->source_filename) {
        return;
    }
    
    FILE *source_file = fopen(diag->source_filename, "r");
    if (!source_file) {
        return;
    }
    
    // Read lines around the error location
    char line_buffer[512];
    size_t current_line = 1;
    size_t target_line = diag->location.line;
    size_t start_line = (target_line > config->context_lines) ? 
                       target_line - config->context_lines : 1;
    size_t end_line = target_line + config->context_lines;
    
    // Skip to start line
    while (current_line < start_line && fgets(line_buffer, sizeof(line_buffer), source_file)) {
        current_line++;
    }
    
    // Print context lines
    while (current_line <= end_line && fgets(line_buffer, sizeof(line_buffer), source_file)) {
        // Remove trailing newline
        size_t len = strlen(line_buffer);
        if (len > 0 && line_buffer[len-1] == '\n') {
            line_buffer[len-1] = '\0';
        }
        
        if (current_line == target_line) {
            // Highlight the error line
            if (config->enable_colors) {
                fprintf(file, " %4zu | \033[1;37m%s\033[0m\n", current_line, line_buffer);
            } else {
                fprintf(file, " %4zu | %s\n", current_line, line_buffer);
            }
            
            // Print caret pointing to error column
            fprintf(file, " %4s | ", "");
            for (size_t i = 1; i < diag->location.column; i++) {
                fprintf(file, " ");
            }
            if (config->enable_colors) {
                fprintf(file, "\033[1;32m^\033[0m\n");
            } else {
                fprintf(file, "^\n");
            }
        } else {
            fprintf(file, " %4zu | %s\n", current_line, line_buffer);
        }
        
        current_line++;
    }
    
    fclose(source_file);
}

void kryon_diagnostic_print(const KryonDiagnosticManager *manager,
                           const KryonDiagnostic *diag,
                           FILE *file) {
    if (!manager || !diag || !file) {
        return;
    }
    
    const KryonDiagnosticConfig *config = &manager->config;
    
    // Print severity and location
    if (config->enable_colors) {
        fprintf(file, "%s%s\033[0m: ", 
               severity_colors[diag->severity],
               severity_names[diag->severity]);
    } else {
        fprintf(file, "%s: ", severity_names[diag->severity]);
    }
    
    // Print message
    fprintf(file, "%s", diag->message);
    
    // Print location if available
    if (diag->source_filename) {
        fprintf(file, "\n  --> %s:%u:%u",
               diag->source_filename,
               diag->location.line,
               diag->location.column);
    }
    
    fprintf(file, "\n");
    
    // Print source context
    print_source_context(file, diag, config);
    
    // Print suggestion if available
    if (diag->suggestion) {
        if (config->enable_colors) {
            fprintf(file, "\033[1;36mHelp\033[0m: %s\n", diag->suggestion);
        } else {
            fprintf(file, "Help: %s\n", diag->suggestion);
        }
    }
    
    fprintf(file, "\n");
}

void kryon_diagnostic_print_all(const KryonDiagnosticManager *manager, FILE *file) {
    if (!manager || !file) {
        return;
    }
    
    for (size_t i = 0; i < manager->diagnostic_count; i++) {
        kryon_diagnostic_print(manager, &manager->diagnostics[i], file);
    }
}

void kryon_diagnostic_print_summary(const KryonDiagnosticManager *manager, FILE *file) {
    if (!manager || !file) {
        return;
    }
    
    if (manager->diagnostic_count == 0) {
        if (manager->config.enable_colors) {
            fprintf(file, "\033[1;32mNo diagnostics\033[0m\n");
        } else {
            fprintf(file, "No diagnostics\n");
        }
        return;
    }
    
    // Print summary
    fprintf(file, "Compilation summary: ");
    
    if (manager->error_count > 0) {
        if (manager->config.enable_colors) {
            fprintf(file, "\033[1;31m%zu error%s\033[0m",
                   manager->error_count,
                   manager->error_count == 1 ? "" : "s");
        } else {
            fprintf(file, "%zu error%s",
                   manager->error_count,
                   manager->error_count == 1 ? "" : "s");
        }
    }
    
    if (manager->warning_count > 0) {
        if (manager->error_count > 0) fprintf(file, ", ");
        if (manager->config.enable_colors) {
            fprintf(file, "\033[1;33m%zu warning%s\033[0m",
                   manager->warning_count,
                   manager->warning_count == 1 ? "" : "s");
        } else {
            fprintf(file, "%zu warning%s",
                   manager->warning_count,
                   manager->warning_count == 1 ? "" : "s");
        }
    }
    
    if (manager->note_count > 0) {
        if (manager->error_count > 0 || manager->warning_count > 0) fprintf(file, ", ");
        fprintf(file, "%zu note%s",
               manager->note_count,
               manager->note_count == 1 ? "" : "s");
    }
    
    fprintf(file, "\n");
}

// =============================================================================
// DIAGNOSTIC QUERIES
// =============================================================================

size_t kryon_diagnostic_get_count(const KryonDiagnosticManager *manager,
                                 KryonDiagnosticSeverity severity) {
    if (!manager) {
        return 0;
    }
    
    switch (severity) {
        case KRYON_DIAG_ERROR:
        case KRYON_DIAG_FATAL:
            return manager->error_count;
        case KRYON_DIAG_WARNING:
            return manager->warning_count;
        case KRYON_DIAG_NOTE:
        case KRYON_DIAG_INFO:
            return manager->note_count;
        default:
            return 0;
    }
}

bool kryon_diagnostic_has_errors(const KryonDiagnosticManager *manager) {
    return manager && manager->error_count > 0;
}

bool kryon_diagnostic_has_warnings(const KryonDiagnosticManager *manager) {
    return manager && manager->warning_count > 0;
}

const KryonDiagnostic *kryon_diagnostic_get_all(const KryonDiagnosticManager *manager,
                                               size_t *out_count) {
    if (!manager || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    *out_count = manager->diagnostic_count;
    return manager->diagnostics;
}

// =============================================================================
// CONFIGURATION
// =============================================================================

void kryon_diagnostic_set_config(KryonDiagnosticManager *manager,
                                 const KryonDiagnosticConfig *config) {
    if (!manager || !config) {
        return;
    }
    
    manager->config = *config;
}

const KryonDiagnosticConfig *kryon_diagnostic_get_config(const KryonDiagnosticManager *manager) {
    return manager ? &manager->config : NULL;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void kryon_diagnostic_clear(KryonDiagnosticManager *manager) {
    if (!manager) {
        return;
    }
    
    // Free existing diagnostics
    for (size_t i = 0; i < manager->diagnostic_count; i++) {
        kryon_free(manager->diagnostics[i].message);
        kryon_free(manager->diagnostics[i].suggestion);
        kryon_free(manager->diagnostics[i].source_filename);
    }
    
    // Reset counters
    manager->diagnostic_count = 0;
    manager->error_count = 0;
    manager->warning_count = 0;
    manager->note_count = 0;
}
