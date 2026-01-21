/**
 * Markdown Code Generator
 * Generates Markdown from KIR JSON (with logic_block support for HTML event handlers)
 */

#define _POSIX_C_SOURCE 200809L

#include "markdown_codegen.h"
#include "../codegen_common.h"
#include "../../ir/include/ir_serialization.h"
#include "../../ir/include/ir_logic.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// Helper to append string to buffer with reallocation
static bool append_string(char** buffer, size_t* size, size_t* capacity, const char* str) {
    size_t len = strlen(str);
    if (*size + len >= *capacity) {
        *capacity *= 2;
        char* new_buffer = realloc(*buffer, *capacity);
        if (!new_buffer) return false;
        *buffer = new_buffer;
    }
    strcpy(*buffer + *size, str);
    *size += len;
    return true;
}

// Helper to append formatted string
static bool append_fmt(char** buffer, size_t* size, size_t* capacity, const char* fmt, ...) {
    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    return append_string(buffer, size, capacity, temp);
}

// Context for markdown generation
typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
    IRLogicBlock* logic_block;  // For event handler lookup
    int list_depth;             // Track nesting level
} MarkdownContext;

// Initialize markdown context
static MarkdownContext* create_markdown_context(IRLogicBlock* logic_block) {
    MarkdownContext* ctx = malloc(sizeof(MarkdownContext));
    if (!ctx) return NULL;

    ctx->capacity = 4096;
    ctx->size = 0;
    ctx->buffer = malloc(ctx->capacity);
    if (!ctx->buffer) {
        free(ctx);
        return NULL;
    }
    ctx->buffer[0] = '\0';
    ctx->logic_block = logic_block;
    ctx->list_depth = 0;

    return ctx;
}

// Free markdown context (doesn't free logic_block)
static void destroy_markdown_context(MarkdownContext* ctx) {
    if (!ctx) return;
    free(ctx->buffer);
    free(ctx);
}

// Get event handler for component
static const char* get_event_handler_source(MarkdownContext* ctx, uint32_t component_id, const char* event_type) {
    if (!ctx->logic_block) return NULL;

    const char* handler_name = ir_logic_block_get_handler(ctx->logic_block, component_id, event_type);
    if (!handler_name) return NULL;

    IRLogicFunction* func = ir_logic_block_find_function(ctx->logic_block, handler_name);
    if (!func || func->source_count == 0) return NULL;

    // Prefer javascript source, fallback to first available
    for (int i = 0; i < func->source_count; i++) {
        if (strcmp(func->sources[i].language, "javascript") == 0) {
            return func->sources[i].source;
        }
    }

    return func->sources[0].source;
}

// Check if component has any event handlers
static bool has_event_handlers(MarkdownContext* ctx, uint32_t component_id) {
    if (!ctx->logic_block) return false;

    const char* events[] = {"click", "change", "submit", "input", NULL};
    for (int i = 0; events[i]; i++) {
        if (get_event_handler_source(ctx, component_id, events[i])) {
            return true;
        }
    }
    return false;
}

// Forward declaration
static bool generate_component(MarkdownContext* ctx, IRComponent* component);

// Generate markdown for heading
static bool generate_heading(MarkdownContext* ctx, IRComponent* component) {
    int level = 1;  // Default to H1

    // Determine level from component type
    if (component->type == IR_COMPONENT_HEADING) {
        // Try to get level from properties (would need to extract from component data)
        // For now, default to H1
        level = 1;
    }

    // Generate heading markers
    for (int i = 0; i < level; i++) {
        if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "#")) return false;
    }

    if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " ")) return false;

    // Generate heading text from children
    if (component->child_count > 0) {
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (!generate_component(ctx, component->children[i])) return false;
        }
    }

    if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "\n\n")) return false;
    return true;
}

// Generate markdown for text
static bool generate_text(MarkdownContext* ctx, IRComponent* component) {
    // Get text content directly from component
    const char* text = component->text_content;

    if (text) {
        if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, text)) return false;
    }

    return true;
}

// Generate HTML for button with events
static bool generate_button_html(MarkdownContext* ctx, IRComponent* component) {
    if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "<button")) return false;

    // Add event handlers
    const char* onclick = get_event_handler_source(ctx, component->id, "click");
    if (onclick) {
        if (!append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " onclick=\"%s\"", onclick)) return false;
    }

    if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, ">")) return false;

    // Button text from children
    if (component->child_count > 0) {
        for (uint32_t i = 0; i < component->child_count; i++) {
            if (!generate_component(ctx, component->children[i])) return false;
        }
    }

    if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "</button>\n\n")) return false;
    return true;
}

// Generate HTML for input with events
static bool generate_input_html(MarkdownContext* ctx, IRComponent* component) {
    if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "<input type=\"text\"")) return false;

    // Add event handlers
    const char* onchange = get_event_handler_source(ctx, component->id, "change");
    if (onchange) {
        if (!append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " onchange=\"%s\"", onchange)) return false;
    }

    const char* oninput = get_event_handler_source(ctx, component->id, "input");
    if (oninput) {
        if (!append_fmt(&ctx->buffer, &ctx->size, &ctx->capacity, " oninput=\"%s\"", oninput)) return false;
    }

    if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, " />\n\n")) return false;
    return true;
}

// Generate markdown for component
static bool generate_component(MarkdownContext* ctx, IRComponent* component) {
    if (!component) return true;

    switch (component->type) {
        case IR_COMPONENT_HEADING:
            return generate_heading(ctx, component);

        case IR_COMPONENT_TEXT:
            return generate_text(ctx, component);

        case IR_COMPONENT_BUTTON:
            // If button has event handlers, generate HTML; otherwise generate markdown
            if (has_event_handlers(ctx, component->id)) {
                return generate_button_html(ctx, component);
            } else {
                // Simple markdown representation
                if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "[")) return false;
                if (component->child_count > 0) {
                    for (uint32_t i = 0; i < component->child_count; i++) {
                        if (!generate_component(ctx, component->children[i])) return false;
                    }
                }
                if (!append_string(&ctx->buffer, &ctx->size, &ctx->capacity, "]\n\n")) return false;
            }
            return true;

        case IR_COMPONENT_INPUT:
            // Inputs with events must be HTML
            if (has_event_handlers(ctx, component->id)) {
                return generate_input_html(ctx, component);
            }
            return true;

        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            // Generate children (containers are implicit in markdown)
            if (component->child_count > 0) {
                for (uint32_t i = 0; i < component->child_count; i++) {
                    if (!generate_component(ctx, component->children[i])) return false;
                }
            }
            return true;

        default:
            // Generate children for unknown types
            if (component->child_count > 0) {
                for (uint32_t i = 0; i < component->child_count; i++) {
                    if (!generate_component(ctx, component->children[i])) return false;
                }
            }
            return true;
    }
}

// Main codegen function
char* markdown_codegen_from_json(const char* kir_json) {
    if (!kir_json) return NULL;

    // Parse KIR JSON
    cJSON* kir = cJSON_Parse(kir_json);
    if (!kir) {
        fprintf(stderr, "Error: Failed to parse KIR JSON\n");
        return NULL;
    }

    // Deserialize IR component tree
    extern IRComponent* ir_deserialize_json(const char* json);
    IRComponent* root = ir_deserialize_json(kir_json);
    if (!root) {
        fprintf(stderr, "Error: Failed to deserialize IR from JSON\n");
        cJSON_Delete(kir);
        return NULL;
    }

    // Extract logic_block if present
    IRLogicBlock* logic_block = NULL;
    cJSON* logic_json = cJSON_GetObjectItem(kir, "logic_block");
    if (logic_json) {
        extern IRLogicBlock* ir_logic_block_from_json(cJSON* json);
        logic_block = ir_logic_block_from_json(logic_json);
    }

    cJSON_Delete(kir);

    // Create markdown context
    MarkdownContext* ctx = create_markdown_context(logic_block);
    if (!ctx) {
        extern void ir_destroy_component(IRComponent* component);
        ir_destroy_component(root);
        if (logic_block) {
            extern void ir_logic_block_free(IRLogicBlock* block);
            ir_logic_block_free(logic_block);
        }
        return NULL;
    }

    // Generate markdown
    bool success = generate_component(ctx, root);

    // Clean up
    char* result = NULL;
    if (success) {
        result = strdup(ctx->buffer);
    }

    destroy_markdown_context(ctx);
    extern void ir_destroy_component(IRComponent* component);
    ir_destroy_component(root);
    if (logic_block) {
        extern void ir_logic_block_free(IRLogicBlock* block);
        ir_logic_block_free(logic_block);
    }

    return result;
}

// Generate from file
bool markdown_codegen_generate(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) return false;

    // Set error prefix for this codegen
    codegen_set_error_prefix("Markdown");

    // Read KIR file using shared utility
    char* kir_json = codegen_read_kir_file(kir_path, NULL);
    if (!kir_json) {
        return false;
    }

    // Generate markdown
    char* markdown = markdown_codegen_from_json(kir_json);
    free(kir_json);

    if (!markdown) {
        codegen_error("Failed to generate Markdown from KIR");
        return false;
    }

    // Write output using shared utility
    bool success = codegen_write_output_file(output_path, markdown);
    free(markdown);

    return success;
}

// Generate with options
bool markdown_codegen_generate_with_options(const char* kir_path,
                                             const char* output_path,
                                             MarkdownCodegenOptions* options) {
    // For now, ignore options and use default behavior
    (void)options;
    return markdown_codegen_generate(kir_path, output_path);
}

/**
 * Recursively process a module and its transitive imports for Markdown
 */
static int markdown_process_module_recursive(const char* module_id, const char* kir_dir,
                                             const char* output_dir, CodegenProcessedModules* processed,
                                             char** toc_entries, int* toc_count) {
    // Skip if already processed
    if (codegen_processed_modules_contains(processed, module_id)) return 0;
    codegen_processed_modules_add(processed, module_id);

    // Skip internal modules
    if (codegen_is_internal_module(module_id)) return 0;

    // Skip external plugins (they're runtime dependencies, not source)
    if (codegen_is_external_plugin(module_id)) return 0;

    // Build path to component's KIR file
    char component_kir_path[2048];
    snprintf(component_kir_path, sizeof(component_kir_path),
             "%s/%s.kir", kir_dir, module_id);

    // Read component's KIR
    char* component_kir_json = codegen_read_kir_file(component_kir_path, NULL);
    if (!component_kir_json) {
        fprintf(stderr, "Warning: Cannot find KIR for '%s' at %s\n",
                module_id, component_kir_path);
        return 0;
    }

    // Parse to get transitive imports before generating
    cJSON* component_root = cJSON_Parse(component_kir_json);
    int files_written = 0;

    // Generate Markdown from KIR
    char* component_md = markdown_codegen_from_json(component_kir_json);
    free(component_kir_json);

    if (component_md) {
        // Write to output directory maintaining hierarchy
        char output_path[2048];
        snprintf(output_path, sizeof(output_path),
                 "%s/%s.md", output_dir, module_id);

        if (codegen_write_file_with_mkdir(output_path, component_md)) {
            printf("✓ Generated: %s.md\n", module_id);
            files_written++;

            // Add to TOC entries
            if (toc_entries && toc_count && *toc_count < 256) {
                char toc_entry[512];
                snprintf(toc_entry, sizeof(toc_entry), "- [%s](./%s.md)\n", module_id, module_id);
                toc_entries[*toc_count] = strdup(toc_entry);
                (*toc_count)++;
            }
        }
        free(component_md);
    } else {
        fprintf(stderr, "Warning: Failed to generate Markdown for '%s'\n", module_id);
    }

    // Process transitive imports from this component
    if (component_root) {
        cJSON* imports = cJSON_GetObjectItem(component_root, "imports");
        if (imports && cJSON_IsArray(imports)) {
            cJSON* import_item = NULL;
            cJSON_ArrayForEach(import_item, imports) {
                if (!cJSON_IsString(import_item)) continue;
                const char* sub_module_id = cJSON_GetStringValue(import_item);
                if (sub_module_id) {
                    files_written += markdown_process_module_recursive(sub_module_id, kir_dir,
                                                                       output_dir, processed,
                                                                       toc_entries, toc_count);
                }
            }
        }
        cJSON_Delete(component_root);
    }

    return files_written;
}

bool markdown_codegen_generate_multi(const char* kir_path, const char* output_dir) {
    if (!kir_path || !output_dir) {
        fprintf(stderr, "Error: Invalid arguments to markdown_codegen_generate_multi\n");
        return false;
    }

    // Set error prefix for this codegen
    codegen_set_error_prefix("Markdown");

    // Read main KIR file
    char* main_kir_json = codegen_read_kir_file(kir_path, NULL);
    if (!main_kir_json) {
        return false;
    }

    // Parse main KIR JSON
    cJSON* main_root = cJSON_Parse(main_kir_json);
    if (!main_root) {
        fprintf(stderr, "Error: Failed to parse main KIR JSON\n");
        free(main_kir_json);
        return false;
    }

    // Create output directory if it doesn't exist
    if (!codegen_mkdir_p(output_dir)) {
        fprintf(stderr, "Error: Could not create output directory: %s\n", output_dir);
        cJSON_Delete(main_root);
        free(main_kir_json);
        return false;
    }

    int files_written = 0;

    // Track TOC entries for index.md
    char* toc_entries[256] = {0};
    int toc_count = 0;

    // 1. Generate index.md from main.kir
    char* main_md = markdown_codegen_from_json(main_kir_json);
    free(main_kir_json);

    if (main_md) {
        char main_output_path[2048];
        snprintf(main_output_path, sizeof(main_output_path), "%s/index.md", output_dir);

        if (codegen_write_file_with_mkdir(main_output_path, main_md)) {
            printf("✓ Generated: index.md\n");
            files_written++;
        }
        free(main_md);
    } else {
        fprintf(stderr, "Warning: Failed to generate index.md from KIR\n");
    }

    // 2. Get the KIR directory (parent of kir_path)
    char kir_dir[2048];
    codegen_get_parent_dir(kir_path, kir_dir, sizeof(kir_dir));

    // 3. Track processed modules to avoid duplicates
    CodegenProcessedModules processed = {0};
    codegen_processed_modules_add(&processed, "main");  // Mark main as processed

    // 4. Process each import recursively (including transitive imports)
    cJSON* imports = cJSON_GetObjectItem(main_root, "imports");
    if (imports && cJSON_IsArray(imports)) {
        cJSON* import_item = NULL;
        cJSON_ArrayForEach(import_item, imports) {
            if (!cJSON_IsString(import_item)) continue;

            const char* module_id = cJSON_GetStringValue(import_item);
            if (module_id) {
                files_written += markdown_process_module_recursive(module_id, kir_dir,
                                                                   output_dir, &processed,
                                                                   toc_entries, &toc_count);
            }
        }
    }

    // 5. Append TOC to index.md if we have entries
    if (toc_count > 0) {
        char index_path[2048];
        snprintf(index_path, sizeof(index_path), "%s/index.md", output_dir);

        FILE* f = fopen(index_path, "a");
        if (f) {
            fprintf(f, "\n## Components\n\n");
            for (int i = 0; i < toc_count; i++) {
                fprintf(f, "%s", toc_entries[i]);
                free(toc_entries[i]);
            }
            fclose(f);
        }
    }

    codegen_processed_modules_free(&processed);
    cJSON_Delete(main_root);

    if (files_written == 0) {
        fprintf(stderr, "Warning: No Markdown files were generated\n");
        return false;
    }

    printf("✓ Generated %d Markdown files in %s\n", files_written, output_dir);
    return true;
}
