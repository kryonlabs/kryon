/**
 * Kryon UI Framework - C Frontend Implementation
 */

#define _POSIX_C_SOURCE 200809L  // For strdup()

#include "kryon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../../third_party/tomlc99/toml.h"
#include "../../backends/desktop/ir_desktop_renderer.h"
#include "../../ir/ir_executor.h"

// ============================================================================
// Internal State
// ============================================================================

// App state is now defined in kryon.h
KryonAppState g_app_state = {NULL, NULL, NULL, 800, 600};

// ============================================================================
// Event Handler Registry
// ============================================================================

typedef struct {
    uint32_t component_id;
    char* logic_id;
    KryonEventHandler handler;
} HandlerEntry;

static HandlerEntry* g_handler_registry = NULL;
static size_t g_handler_count = 0;
static uint32_t g_handler_id_counter = 0;

// Forward declaration - defined after g_c_metadata
static void update_metadata_logic_id(const char* function_name, const char* logic_id);

static void register_handler(uint32_t component_id, const char* event_type, KryonEventHandler handler, const char* handler_name) {
    // Generate unique logic_id
    char logic_id[128];
    snprintf(logic_id, sizeof(logic_id), "c_%s_%u_%u", event_type, component_id, g_handler_id_counter++);

    // Update g_c_metadata with the logic_id
    if (handler_name) {
        update_metadata_logic_id(handler_name, logic_id);
    }

    // Expand handler registry
    g_handler_registry = realloc(g_handler_registry, (g_handler_count + 1) * sizeof(HandlerEntry));
    if (!g_handler_registry) {
        fprintf(stderr, "[kryon] Error: Failed to allocate handler registry\n");
        return;
    }

    // Add entry
    g_handler_registry[g_handler_count].component_id = component_id;
    g_handler_registry[g_handler_count].logic_id = strdup(logic_id);
    g_handler_registry[g_handler_count].handler = handler;
    g_handler_count++;

    // Create IR event
    IREventType ir_event_type = IR_EVENT_CLICK;
    if (strcmp(event_type, "click") == 0) ir_event_type = IR_EVENT_CLICK;
    else if (strcmp(event_type, "change") == 0) ir_event_type = IR_EVENT_CLICK;  // Reuse click for now
    else if (strcmp(event_type, "hover") == 0) ir_event_type = IR_EVENT_HOVER;
    else if (strcmp(event_type, "focus") == 0) ir_event_type = IR_EVENT_FOCUS;

    // Store handler name in handler_data for round-trip serialization
    IREvent* event = ir_create_event(ir_event_type, logic_id, handler_name);
    IRComponent* component = ir_find_component_by_id(g_app_state.root, component_id);
    if (component) {
        ir_add_event(component, event);
    }
}

void kryon_c_event_bridge(const char* logic_id) {
    printf("[event_bridge] Called with logic_id: %s (registry has %zu handlers)\n", logic_id, g_handler_count);
    for (size_t i = 0; i < g_handler_count; i++) {
        printf("[event_bridge]   [%zu] %s -> %p\n", i, g_handler_registry[i].logic_id, (void*)g_handler_registry[i].handler);
        if (strcmp(g_handler_registry[i].logic_id, logic_id) == 0) {
            printf("[event_bridge] Found handler! Calling...\n");
            g_handler_registry[i].handler();
            printf("[event_bridge] Handler completed\n");
            return;
        }
    }
    fprintf(stderr, "[kryon] Warning: No handler found for logic_id: %s\n", logic_id);
}

static void cleanup_handlers(void) {
    for (size_t i = 0; i < g_handler_count; i++) {
        free(g_handler_registry[i].logic_id);
    }
    free(g_handler_registry);
    g_handler_registry = NULL;
    g_handler_count = 0;
}

// ============================================================================
// C Source Metadata (for round-trip conversion)
// ============================================================================

#include "../../ir/ir_c_metadata.h"

// Global metadata instance
CSourceMetadata g_c_metadata = {0};

static void update_metadata_logic_id(const char* function_name, const char* logic_id) {
    // Find matching event handler in g_c_metadata and update its logic_id
    for (size_t i = 0; i < g_c_metadata.event_handler_count; i++) {
        if (g_c_metadata.event_handlers[i].function_name &&
            strcmp(g_c_metadata.event_handlers[i].function_name, function_name) == 0) {
            // Update logic_id
            if (g_c_metadata.event_handlers[i].logic_id) {
                free(g_c_metadata.event_handlers[i].logic_id);
            }
            g_c_metadata.event_handlers[i].logic_id = strdup(logic_id);
            return;
        }
    }
}

void kryon_register_variable(const char* name, const char* type, const char* storage,
                              const char* initial_value, uint32_t component_id, int line_number) {
    if (!name || !type) return;
    if (g_c_metadata.variable_count >= g_c_metadata.variable_capacity) {
        size_t new_cap = g_c_metadata.variable_capacity == 0 ? 8 : g_c_metadata.variable_capacity * 2;
        g_c_metadata.variables = realloc(g_c_metadata.variables, new_cap * sizeof(CVariableDecl));
        if (!g_c_metadata.variables) return;
        g_c_metadata.variable_capacity = new_cap;
    }
    CVariableDecl* var = &g_c_metadata.variables[g_c_metadata.variable_count++];
    var->name = strdup(name);
    var->type = strdup(type);
    var->storage = storage ? strdup(storage) : NULL;
    var->initial_value = initial_value ? strdup(initial_value) : NULL;
    var->component_id = component_id;
    var->line_number = line_number;
}

void kryon_register_event_handler(const char* logic_id, const char* function_name,
                                   const char* return_type, const char* parameters,
                                   const char* body, int line_number) {
    if (!logic_id || !function_name) return;
    if (g_c_metadata.event_handler_count >= g_c_metadata.event_handler_capacity) {
        size_t new_cap = g_c_metadata.event_handler_capacity == 0 ? 8 : g_c_metadata.event_handler_capacity * 2;
        g_c_metadata.event_handlers = realloc(g_c_metadata.event_handlers, new_cap * sizeof(CEventHandlerDecl));
        if (!g_c_metadata.event_handlers) return;
        g_c_metadata.event_handler_capacity = new_cap;
    }
    CEventHandlerDecl* handler = &g_c_metadata.event_handlers[g_c_metadata.event_handler_count++];
    handler->logic_id = strdup(logic_id);
    handler->function_name = strdup(function_name);
    handler->return_type = return_type ? strdup(return_type) : strdup("void");
    handler->parameters = parameters ? strdup(parameters) : strdup("void");
    handler->body = body ? strdup(body) : NULL;
    handler->line_number = line_number;
}

void kryon_register_helper_function(const char* name, const char* return_type,
                                     const char* parameters, const char* body, int line_number) {
    if (!name || !return_type) return;
    if (g_c_metadata.helper_function_count >= g_c_metadata.helper_function_capacity) {
        size_t new_cap = g_c_metadata.helper_function_capacity == 0 ? 8 : g_c_metadata.helper_function_capacity * 2;
        g_c_metadata.helper_functions = realloc(g_c_metadata.helper_functions, new_cap * sizeof(CHelperFunction));
        if (!g_c_metadata.helper_functions) return;
        g_c_metadata.helper_function_capacity = new_cap;
    }
    CHelperFunction* func = &g_c_metadata.helper_functions[g_c_metadata.helper_function_count++];
    func->name = strdup(name);
    func->return_type = strdup(return_type);
    func->parameters = parameters ? strdup(parameters) : strdup("void");
    func->body = body ? strdup(body) : NULL;
    func->line_number = line_number;
}

void kryon_add_include(const char* include_string, bool is_system, int line_number) {
    if (!include_string) return;
    if (g_c_metadata.include_count >= g_c_metadata.include_capacity) {
        size_t new_cap = g_c_metadata.include_capacity == 0 ? 16 : g_c_metadata.include_capacity * 2;
        g_c_metadata.includes = realloc(g_c_metadata.includes, new_cap * sizeof(CInclude));
        if (!g_c_metadata.includes) return;
        g_c_metadata.include_capacity = new_cap;
    }
    CInclude* inc = &g_c_metadata.includes[g_c_metadata.include_count++];
    inc->include_string = strdup(include_string);
    inc->is_system = is_system;
    inc->line_number = line_number;
}

void kryon_add_preprocessor_directive(const char* directive_type, const char* condition,
                                       const char* value, int line_number) {
    if (!directive_type) return;
    if (g_c_metadata.preprocessor_directive_count >= g_c_metadata.preprocessor_directive_capacity) {
        size_t new_cap = g_c_metadata.preprocessor_directive_capacity == 0 ? 16 : g_c_metadata.preprocessor_directive_capacity * 2;
        g_c_metadata.preprocessor_directives = realloc(g_c_metadata.preprocessor_directives, new_cap * sizeof(CPreprocessorDirective));
        if (!g_c_metadata.preprocessor_directives) return;
        g_c_metadata.preprocessor_directive_capacity = new_cap;
    }
    CPreprocessorDirective* dir = &g_c_metadata.preprocessor_directives[g_c_metadata.preprocessor_directive_count++];
    dir->directive_type = strdup(directive_type);
    dir->condition = condition ? strdup(condition) : NULL;
    dir->value = value ? strdup(value) : NULL;
    dir->line_number = line_number;
}

void kryon_add_source_file(const char* filename, const char* full_path, const char* content) {
    if (!filename) return;
    if (g_c_metadata.source_file_count >= g_c_metadata.source_file_capacity) {
        size_t new_cap = g_c_metadata.source_file_capacity == 0 ? 4 : g_c_metadata.source_file_capacity * 2;
        g_c_metadata.source_files = realloc(g_c_metadata.source_files, new_cap * sizeof(CSourceFile));
        if (!g_c_metadata.source_files) return;
        g_c_metadata.source_file_capacity = new_cap;
    }
    CSourceFile* file = &g_c_metadata.source_files[g_c_metadata.source_file_count++];
    file->filename = strdup(filename);
    file->full_path = full_path ? strdup(full_path) : NULL;
    file->content = content ? strdup(content) : NULL;
}

void kryon_set_main_source_file(const char* filename) {
    if (g_c_metadata.main_source_file) free(g_c_metadata.main_source_file);
    g_c_metadata.main_source_file = filename ? strdup(filename) : NULL;
}

static void cleanup_metadata(void) {
    for (size_t i = 0; i < g_c_metadata.variable_count; i++) {
        free(g_c_metadata.variables[i].name);
        free(g_c_metadata.variables[i].type);
        free(g_c_metadata.variables[i].storage);
        free(g_c_metadata.variables[i].initial_value);
    }
    free(g_c_metadata.variables);

    for (size_t i = 0; i < g_c_metadata.event_handler_count; i++) {
        free(g_c_metadata.event_handlers[i].logic_id);
        free(g_c_metadata.event_handlers[i].function_name);
        free(g_c_metadata.event_handlers[i].return_type);
        free(g_c_metadata.event_handlers[i].parameters);
        free(g_c_metadata.event_handlers[i].body);
    }
    free(g_c_metadata.event_handlers);

    for (size_t i = 0; i < g_c_metadata.helper_function_count; i++) {
        free(g_c_metadata.helper_functions[i].name);
        free(g_c_metadata.helper_functions[i].return_type);
        free(g_c_metadata.helper_functions[i].parameters);
        free(g_c_metadata.helper_functions[i].body);
    }
    free(g_c_metadata.helper_functions);

    for (size_t i = 0; i < g_c_metadata.include_count; i++) {
        free(g_c_metadata.includes[i].include_string);
    }
    free(g_c_metadata.includes);

    for (size_t i = 0; i < g_c_metadata.preprocessor_directive_count; i++) {
        free(g_c_metadata.preprocessor_directives[i].directive_type);
        free(g_c_metadata.preprocessor_directives[i].condition);
        free(g_c_metadata.preprocessor_directives[i].value);
    }
    free(g_c_metadata.preprocessor_directives);

    for (size_t i = 0; i < g_c_metadata.source_file_count; i++) {
        free(g_c_metadata.source_files[i].filename);
        free(g_c_metadata.source_files[i].full_path);
        free(g_c_metadata.source_files[i].content);
    }
    free(g_c_metadata.source_files);

    free(g_c_metadata.main_source_file);
    memset(&g_c_metadata, 0, sizeof(CSourceMetadata));
}

// ============================================================================
// Dimension Helpers
// ============================================================================

static IRDimensionType parse_unit(const char* unit) {
    if (!unit || strcmp(unit, "px") == 0) return IR_DIMENSION_PX;
    if (strcmp(unit, "%") == 0) return IR_DIMENSION_PERCENT;
    if (strcmp(unit, "auto") == 0) return IR_DIMENSION_AUTO;
    if (strcmp(unit, "fr") == 0 || strcmp(unit, "flex") == 0) return IR_DIMENSION_FLEX;
    return IR_DIMENSION_PX;  // Default
}

// ============================================================================
// Initialization & App Management
// ============================================================================

void kryon_init(const char* title, int width, int height) {
    // Create IR context
    g_app_state.context = ir_create_context();
    ir_set_context(g_app_state.context);

    // Create and set global executor for event handling
    IRExecutorContext* executor = ir_executor_create();
    if (executor) {
        ir_executor_set_global(executor);
        printf("[kryon] Executor initialized\n");
    } else {
        fprintf(stderr, "[kryon] Warning: Failed to create executor\n");
    }

    // Store window config
    g_app_state.window_title = title ? strdup(title) : strdup("Kryon App");
    g_app_state.window_width = width > 0 ? width : 800;
    g_app_state.window_height = height > 0 ? height : 600;

    // Create root container
    g_app_state.root = ir_create_component(IR_COMPONENT_CONTAINER);
    ir_set_root(g_app_state.root);
}

IRComponent* kryon_get_root(void) {
    return g_app_state.root;
}

bool kryon_finalize(const char* output_path) {
    if (!g_app_state.context || !g_app_state.root) {
        fprintf(stderr, "[kryon] Error: Must call kryon_finalize() before kryon_finalize()\n");
        return false;
    }

    // Check for KRYON_SERIALIZE_IR environment variable (overrides output_path)
    const char* env_output = getenv("KRYON_SERIALIZE_IR");
    const char* final_output = env_output ? env_output : output_path;

    // Serialize to .kir JSON file (v3 format)
    bool success = ir_write_json_file(g_app_state.root, NULL, final_output);

    if (!success) {
        fprintf(stderr, "[kryon] Error: Failed to write .kir file to %s\n", output_path);
    }

    return success;
}

void kryon_cleanup(void) {
    if (g_app_state.context) {
        ir_destroy_context(g_app_state.context);
        g_app_state.context = NULL;
    }
    if (g_app_state.window_title) {
        free(g_app_state.window_title);
        g_app_state.window_title = NULL;
    }
    cleanup_handlers();
    cleanup_metadata();
    g_app_state.root = NULL;
}

int kryon_get_renderer_backend_from_config(void) {
    // Search for kryon.toml in current directory and parent directories
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        return -1;
    }

    char config_path[PATH_MAX];
    char search_dir[PATH_MAX];
    strncpy(search_dir, cwd, sizeof(search_dir) - 1);

    // Search up to 5 parent directories
    for (int i = 0; i < 5; i++) {
        snprintf(config_path, sizeof(config_path), "%s/kryon.toml", search_dir);

        FILE* f = fopen(config_path, "r");
        if (f) {
            fclose(f);
            break;  // Found it
        }

        // Go up one directory
        char* last_slash = strrchr(search_dir, '/');
        if (!last_slash || last_slash == search_dir) {
            return -1;  // Reached root without finding config
        }
        *last_slash = '\0';
    }

    // Parse the TOML file
    FILE* fp = fopen(config_path, "r");
    if (!fp) {
        return -1;
    }

    char errbuf[200];
    toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!conf) {
        return -1;
    }

    // Get [desktop] table
    toml_table_t* desktop = toml_table_in(conf, "desktop");
    if (!desktop) {
        toml_free(conf);
        return -1;
    }

    // Get renderer value
    toml_datum_t renderer = toml_string_in(desktop, "renderer");
    if (!renderer.ok) {
        toml_free(conf);
        return -1;
    }

    // Map renderer string to backend enum
    int backend = -1;
    if (strcmp(renderer.u.s, "raylib") == 0) {
        backend = DESKTOP_BACKEND_RAYLIB;
    } else if (strcmp(renderer.u.s, "sdl3") == 0) {
        backend = DESKTOP_BACKEND_SDL3;
    } else if (strcmp(renderer.u.s, "glfw") == 0) {
        backend = DESKTOP_BACKEND_GLFW;
    }

    free(renderer.u.s);
    toml_free(conf);
    return backend;
}

// ============================================================================
// Component Creation
// ============================================================================

IRComponent* kryon_container(void) {
    return ir_create_component(IR_COMPONENT_CONTAINER);
}

IRComponent* kryon_row(void) {
    return ir_create_component(IR_COMPONENT_ROW);
}

IRComponent* kryon_column(void) {
    return ir_create_component(IR_COMPONENT_COLUMN);
}

IRComponent* kryon_center(void) {
    return ir_create_component(IR_COMPONENT_CENTER);
}

IRComponent* kryon_text(const char* content) {
    IRComponent* c = ir_create_component(IR_COMPONENT_TEXT);
    if (content) {
        ir_set_text_content(c, content);
    }
    return c;
}

IRComponent* kryon_button(const char* label) {
    IRComponent* c = ir_create_component(IR_COMPONENT_BUTTON);
    if (label) {
        ir_set_text_content(c, label);
    }
    return c;
}

IRComponent* kryon_input(const char* placeholder) {
    IRComponent* c = ir_create_component(IR_COMPONENT_INPUT);
    if (placeholder) {
        ir_set_text_content(c, placeholder);  // Placeholder stored in text_content
    }
    return c;
}

IRComponent* kryon_checkbox(const char* label, bool checked) {
    IRComponent* c = ir_create_component(IR_COMPONENT_CHECKBOX);
    if (label) {
        ir_set_text_content(c, label);
    }
    ir_set_checkbox_state(c, checked);
    return c;
}

IRComponent* kryon_dropdown(const char* placeholder) {
    IRComponent* c = ir_create_component(IR_COMPONENT_DROPDOWN);
    if (placeholder) {
        ir_set_text_content(c, placeholder);
    }
    return c;
}

IRComponent* kryon_image(const char* src, const char* alt) {
    IRComponent* c = ir_create_component(IR_COMPONENT_IMAGE);
    if (src) {
        ir_set_custom_data(c, src);  // src stored in custom_data
    }
    if (alt) {
        ir_set_text_content(c, alt);  // alt stored in text_content
    }
    return c;
}

// ============================================================================
// Tab Components
// ============================================================================

IRComponent* kryon_tab_group(void) {
    return ir_create_component(IR_COMPONENT_TAB_GROUP);
}

IRComponent* kryon_tab_bar(void) {
    return ir_create_component(IR_COMPONENT_TAB_BAR);
}

IRComponent* kryon_tab(const char* title) {
    IRComponent* c = ir_create_component(IR_COMPONENT_TAB);
    if (title) {
        ir_set_text_content(c, title);
    }
    return c;
}

IRComponent* kryon_tab_content(void) {
    return ir_create_component(IR_COMPONENT_TAB_CONTENT);
}

IRComponent* kryon_tab_panel(void) {
    return ir_create_component(IR_COMPONENT_TAB_PANEL);
}

// ============================================================================
// Table Components
// ============================================================================

IRComponent* kryon_table(void) {
    return ir_create_component(IR_COMPONENT_TABLE);
}

IRComponent* kryon_table_head(void) {
    return ir_create_component(IR_COMPONENT_TABLE_HEAD);
}

IRComponent* kryon_table_body(void) {
    return ir_create_component(IR_COMPONENT_TABLE_BODY);
}

IRComponent* kryon_table_row(void) {
    return ir_create_component(IR_COMPONENT_TABLE_ROW);
}

IRComponent* kryon_table_cell(const char* content) {
    IRComponent* c = ir_create_component(IR_COMPONENT_TABLE_CELL);
    if (content) {
        ir_set_text_content(c, content);
    }
    return c;
}

IRComponent* kryon_table_header_cell(const char* content) {
    IRComponent* c = ir_create_component(IR_COMPONENT_TABLE_HEADER_CELL);
    if (content) {
        ir_set_text_content(c, content);
    }
    return c;
}

// ============================================================================
// Helper Functions
// ============================================================================

static IRStyle* get_or_create_style(IRComponent* c) {
    if (!c) return NULL;
    IRStyle* style = ir_get_style(c);
    if (!style) {
        style = ir_create_style();
        ir_set_style(c, style);
    }
    return style;
}

static IRLayout* get_or_create_layout(IRComponent* c) {
    if (!c) return NULL;
    IRLayout* layout = ir_get_layout(c);
    if (!layout) {
        layout = ir_create_layout();
        ir_set_layout(c, layout);
    }
    return layout;
}

// ============================================================================
// Component Properties - Dimensions
// ============================================================================

void kryon_set_width(IRComponent* c, float value, const char* unit) {
    if (!c) return;
    get_or_create_style(c);  // Ensure style exists
    ir_set_width(c, parse_unit(unit), value);
}

void kryon_set_height(IRComponent* c, float value, const char* unit) {
    if (!c) return;
    get_or_create_style(c);  // Ensure style exists
    ir_set_height(c, parse_unit(unit), value);
}

void kryon_set_min_width(IRComponent* c, float value, const char* unit) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_min_width(layout, parse_unit(unit), value);
}

void kryon_set_min_height(IRComponent* c, float value, const char* unit) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_min_height(layout, parse_unit(unit), value);
}

void kryon_set_max_width(IRComponent* c, float value, const char* unit) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_max_width(layout, parse_unit(unit), value);
}

void kryon_set_max_height(IRComponent* c, float value, const char* unit) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_max_height(layout, parse_unit(unit), value);
}

// ============================================================================
// Component Properties - Colors
// ============================================================================

void kryon_set_background(IRComponent* c, uint32_t color) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint8_t a = (color >> 24) & 0xFF;
    if (a == 0) a = 255;  // Default to opaque if alpha not specified
    ir_set_background_color(style, r, g, b, a);
}

void kryon_set_color(IRComponent* c, uint32_t color) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint8_t a = (color >> 24) & 0xFF;
    if (a == 0) a = 255;
    ir_set_font_color(style, r, g, b, a);
}

void kryon_set_border_color(IRComponent* c, uint32_t color) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint8_t a = (color >> 24) & 0xFF;
    if (a == 0) a = 255;
    ir_set_border_color(style, r, g, b, a);
}

// ============================================================================
// Component Properties - Spacing
// ============================================================================

void kryon_set_padding(IRComponent* c, float value) {
    if (!c) return;
    get_or_create_style(c);  // Ensure style exists
    ir_set_padding(c, value, value, value, value);
}

void kryon_set_padding_sides(IRComponent* c, float top, float right, float bottom, float left) {
    if (!c) return;
    get_or_create_style(c);  // Ensure style exists
    ir_set_padding(c, top, right, bottom, left);
}

void kryon_set_margin(IRComponent* c, float value) {
    if (!c) return;
    get_or_create_style(c);  // Ensure style exists
    ir_set_margin(c, value, value, value, value);
}

void kryon_set_margin_sides(IRComponent* c, float top, float right, float bottom, float left) {
    if (!c) return;
    get_or_create_style(c);  // Ensure style exists
    ir_set_margin(c, top, right, bottom, left);
}

void kryon_set_gap(IRComponent* c, float value) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_flexbox(layout, false, (uint32_t)value, IR_ALIGNMENT_START, IR_ALIGNMENT_START);
}

// ============================================================================
// Component Properties - Typography
// ============================================================================

void kryon_set_font_size(IRComponent* c, float size) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_font_size(style, size);
}

void kryon_set_font_family(IRComponent* c, const char* family) {
    if (!c || !family) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_font_family(style, family);
}

void kryon_set_font_weight(IRComponent* c, uint16_t weight) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_font_weight(style, weight);
}

void kryon_set_font_bold(IRComponent* c, bool bold) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_font_style(style, bold, style->font.italic);
}

void kryon_set_font_italic(IRComponent* c, bool italic) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_font_style(style, style->font.bold, italic);
}

void kryon_set_line_height(IRComponent* c, float line_height) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_line_height(style, line_height);
}

void kryon_set_letter_spacing(IRComponent* c, float spacing) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_letter_spacing(style, spacing);
}

void kryon_set_text_align(IRComponent* c, IRTextAlign align) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_text_align(style, align);
}

// ============================================================================
// Component Properties - Border & Radius
// ============================================================================

void kryon_set_border_width(IRComponent* c, float width) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_border_width(style, width);
}

void kryon_set_border_radius(IRComponent* c, float radius) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_border_radius(style, (uint8_t)radius);
}

// ============================================================================
// Component Properties - Effects
// ============================================================================

void kryon_set_opacity(IRComponent* c, float opacity) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_opacity(style, opacity);
}

void kryon_set_z_index(IRComponent* c, uint32_t z_index) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_z_index(style, z_index);
}

void kryon_set_visible(IRComponent* c, bool visible) {
    if (!c) return;
    IRStyle* style = get_or_create_style(c);
    ir_set_visible(style, visible);
}

// ============================================================================
// Layout Properties
// ============================================================================

void kryon_set_justify_content(IRComponent* c, IRAlignment align) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_justify_content(layout, align);
}

void kryon_set_align_items(IRComponent* c, IRAlignment align) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_align_items(layout, align);
}

void kryon_set_flex_grow(IRComponent* c, uint8_t grow) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_flex_properties(layout, grow, 0, 0);  // grow only
}

void kryon_set_flex_shrink(IRComponent* c, uint8_t shrink) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_flex_properties(layout, 0, shrink, 0);  // shrink only
}

void kryon_set_flex_wrap(IRComponent* c, bool wrap) {
    if (!c) return;
    IRLayout* layout = get_or_create_layout(c);
    ir_set_flexbox(layout, wrap, 0, IR_ALIGNMENT_START, IR_ALIGNMENT_START);
}

// ============================================================================
// Tree Management
// ============================================================================

void kryon_add_child(IRComponent* parent, IRComponent* child) {
    if (!parent || !child) return;
    ir_add_child(parent, child);
}

void kryon_remove_child(IRComponent* parent, IRComponent* child) {
    if (!parent || !child) return;
    ir_remove_child(parent, child);
}

void kryon_insert_child(IRComponent* parent, IRComponent* child, uint32_t index) {
    if (!parent || !child) return;
    ir_insert_child(parent, child, index);
}

// ============================================================================
// Event Handlers
// ============================================================================

void kryon_on_click(IRComponent* component, KryonEventHandler handler, const char* handler_name) {
    if (!component || !handler) return;
    register_handler(component->id, "click", handler, handler_name);
}

void kryon_on_change(IRComponent* component, KryonEventHandler handler, const char* handler_name) {
    if (!component || !handler) return;
    register_handler(component->id, "change", handler, handler_name);
}

void kryon_on_hover(IRComponent* component, KryonEventHandler handler, const char* handler_name) {
    if (!component || !handler) return;
    register_handler(component->id, "hover", handler, handler_name);
}

void kryon_on_focus(IRComponent* component, KryonEventHandler handler, const char* handler_name) {
    if (!component || !handler) return;
    register_handler(component->id, "focus", handler, handler_name);
}

// ============================================================================
// Animation
// ============================================================================

IRAnimation* kryon_animation_create(const char* name, float duration_ms) {
    return ir_animation_create_keyframe(name, duration_ms);
}

void kryon_animation_add_keyframe(IRAnimation* anim, float offset, IRAnimationProperty property, float value) {
    if (!anim) return;
    IRKeyframe* kf = ir_animation_add_keyframe(anim, offset);
    ir_keyframe_set_property(kf, property, value);
}

void kryon_animation_set_iterations(IRAnimation* anim, int32_t count) {
    if (!anim) return;
    ir_animation_set_iterations(anim, count);
}

void kryon_animation_set_alternate(IRAnimation* anim, bool alternate) {
    if (!anim) return;
    ir_animation_set_alternate(anim, alternate);
}

void kryon_component_add_animation(IRComponent* c, IRAnimation* anim) {
    if (!c || !anim) return;
    ir_component_add_animation(c, anim);
}
