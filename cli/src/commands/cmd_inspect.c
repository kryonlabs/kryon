/**
 * Inspect Command
 * Inspect and analyze KIR (Kryon Intermediate Representation) files
 */

#include "kryon_cli.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_serialization.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ANSI Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

// Combined styles
#define COLOR_HEADER  COLOR_BOLD COLOR_CYAN
#define COLOR_KEY     COLOR_YELLOW
#define COLOR_VALUE   COLOR_GREEN
#define COLOR_TYPE    COLOR_BLUE
#define COLOR_COMMENT COLOR_DIM

// External IR functions
extern IRComponent* ir_read_json_file(const char* filename);
extern char* ir_serialize_json(IRComponent* root, IRReactiveManifest* manifest);
extern void ir_print_component_info(IRComponent* component, int depth);
extern bool ir_validate_component(IRComponent* component);
extern void ir_pool_free_component(IRComponent* component);

/**
 * Load complete KIR file including logic and reactive manifest
 * Caller must free with cJSON_Delete()
 */
static cJSON* load_kir_json(const char* file_path) {
    FILE* file = fopen(file_path, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(content, 1, file_size, file);
    content[bytes_read] = '\0';
    fclose(file);

    cJSON* root = cJSON_Parse(content);
    free(content);

    return root;
}

/**
 * Recursively find component by ID
 */
static IRComponent* find_component_by_id(IRComponent* root, uint32_t target_id) {
    if (!root) return NULL;
    if (root->id == target_id) return root;

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* found = find_component_by_id(root->children[i], target_id);
        if (found) return found;
    }
    return NULL;
}

/**
 * Convert dimension type to string
 */
static const char* dimension_type_to_string(IRDimensionType type) {
    switch (type) {
        case IR_DIMENSION_PX: return "px";
        case IR_DIMENSION_PERCENT: return "%";
        case IR_DIMENSION_FLEX: return "flex";
        case IR_DIMENSION_VW: return "vw";
        case IR_DIMENSION_VH: return "vh";
        case IR_DIMENSION_REM: return "rem";
        case IR_DIMENSION_EM: return "em";
        default: return "";
    }
}

/**
 * Print event handlers and bindings
 */
static void print_events(cJSON* kir_root, IRComponent* component_root, bool use_color) {
    const char* header_color = use_color ? COLOR_HEADER : "";
    const char* key_color = use_color ? COLOR_KEY : "";
    const char* value_color = use_color ? COLOR_VALUE : "";
    const char* type_color = use_color ? COLOR_TYPE : "";
    const char* reset = use_color ? COLOR_RESET : "";

    printf("\n");
    printf("%s═══════════════════════════════════════════════════════════%s\n", header_color, reset);
    printf("%s  Event Handlers & Bindings%s\n", header_color, reset);
    printf("%s═══════════════════════════════════════════════════════════%s\n", header_color, reset);
    printf("\n");

    // Get logic block
    cJSON* logic = cJSON_GetObjectItem(kir_root, "logic");
    if (!logic) {
        logic = cJSON_GetObjectItem(kir_root, "logic_block");
    }

    if (!logic) {
        printf("  %sNo logic block found%s\n", key_color, reset);
        return;
    }

    // Get event_bindings array
    cJSON* event_bindings = cJSON_GetObjectItem(logic, "event_bindings");
    if (!event_bindings || !cJSON_IsArray(event_bindings)) {
        printf("  %sNo event bindings found%s\n", key_color, reset);
        return;
    }

    int binding_count = cJSON_GetArraySize(event_bindings);
    printf("  %sTotal Bindings:%s %s%d%s\n\n", key_color, reset, value_color, binding_count, reset);

    if (binding_count == 0) {
        return;
    }

    // Iterate through bindings
    for (int i = 0; i < binding_count; i++) {
        cJSON* binding = cJSON_GetArrayItem(event_bindings, i);
        if (!binding) continue;

        cJSON* comp_id_json = cJSON_GetObjectItem(binding, "component_id");
        cJSON* event_type_json = cJSON_GetObjectItem(binding, "event_type");
        cJSON* handler_name_json = cJSON_GetObjectItem(binding, "handler_name");

        if (!comp_id_json || !event_type_json || !handler_name_json) continue;

        uint32_t comp_id = (uint32_t)comp_id_json->valueint;
        const char* event_type = event_type_json->valuestring;
        const char* handler_name = handler_name_json->valuestring;

        // Find component in tree
        IRComponent* comp = find_component_by_id(component_root, comp_id);
        const char* comp_type = comp ? ir_component_type_to_string(comp->type) : "Unknown";

        printf("  %s[%d]%s %sComponent ID:%s %s%u%s  %sEvent:%s %s%s%s  %sHandler:%s %s%s%s\n",
               key_color, i + 1, reset,
               key_color, reset, value_color, comp_id, reset,
               key_color, reset, type_color, event_type, reset,
               key_color, reset, value_color, handler_name, reset);
        printf("       %s↳ Component Type:%s %s%s%s\n",
               key_color, reset, type_color, comp_type, reset);
    }

    printf("\n");
}

/**
 * Print logic functions and handler code
 */
static void print_logic(cJSON* kir_root, bool use_color) {
    const char* header_color = use_color ? COLOR_HEADER : "";
    const char* key_color = use_color ? COLOR_KEY : "";
    const char* value_color = use_color ? COLOR_VALUE : "";
    const char* type_color = use_color ? COLOR_TYPE : "";
    const char* reset = use_color ? COLOR_RESET : "";

    printf("\n");
    printf("%s═══════════════════════════════════════════════════════════%s\n", header_color, reset);
    printf("%s  Logic Functions%s\n", header_color, reset);
    printf("%s═══════════════════════════════════════════════════════════%s\n", header_color, reset);
    printf("\n");

    // Get logic block
    cJSON* logic = cJSON_GetObjectItem(kir_root, "logic");
    if (!logic) {
        logic = cJSON_GetObjectItem(kir_root, "logic_block");
    }

    if (!logic) {
        printf("  %sNo logic block found%s\n", key_color, reset);
        return;
    }

    // Get functions object
    cJSON* functions = cJSON_GetObjectItem(logic, "functions");
    if (!functions || !cJSON_IsObject(functions)) {
        printf("  %sNo functions found%s\n", key_color, reset);
        return;
    }

    int function_count = cJSON_GetArraySize(functions);
    printf("  %sTotal Functions:%s %s%d%s\n\n", key_color, reset, value_color, function_count, reset);

    if (function_count == 0) {
        return;
    }

    // Iterate through functions
    int idx = 1;
    cJSON* func = functions->child;
    while (func) {
        printf("  %s[%d] Function:%s %s%s%s\n", key_color, idx, reset, value_color, func->string, reset);

        // Check for universal statements
        cJSON* has_universal = cJSON_GetObjectItem(func, "has_universal");
        if (has_universal && cJSON_IsTrue(has_universal)) {
            cJSON* universal_stmts = cJSON_GetObjectItem(func, "universal_statements");
            if (universal_stmts && cJSON_IsArray(universal_stmts)) {
                int stmt_count = cJSON_GetArraySize(universal_stmts);
                printf("      %sUniversal Statements:%s %s%d%s\n", key_color, reset, value_color, stmt_count, reset);

                for (int i = 0; i < stmt_count; i++) {
                    cJSON* stmt = cJSON_GetArrayItem(universal_stmts, i);
                    cJSON* type = cJSON_GetObjectItem(stmt, "type");
                    if (type && type->valuestring) {
                        printf("        %s%d.%s %s%s%s\n", key_color, i + 1, reset, type_color, type->valuestring, reset);
                    }
                }
            }
        }

        // Check for embedded sources
        cJSON* sources = cJSON_GetObjectItem(func, "sources");
        if (sources && cJSON_IsArray(sources)) {
            int source_count = cJSON_GetArraySize(sources);
            if (source_count > 0) {
                printf("      %sEmbedded Sources:%s\n", key_color, reset);

                for (int i = 0; i < source_count; i++) {
                    cJSON* source = cJSON_GetArrayItem(sources, i);
                    cJSON* lang = cJSON_GetObjectItem(source, "lang");
                    cJSON* code = cJSON_GetObjectItem(source, "code");

                    if (lang && code && lang->valuestring && code->valuestring) {
                        printf("        %s[%s]%s\n", type_color, lang->valuestring, reset);
                        // Print code with indentation
                        const char* code_str = code->valuestring;
                        const char* line_start = code_str;
                        while (*code_str) {
                            if (*code_str == '\n') {
                                printf("          %.*s\n", (int)(code_str - line_start), line_start);
                                line_start = code_str + 1;
                            }
                            code_str++;
                        }
                        if (line_start < code_str) {
                            printf("          %s\n", line_start);
                        }
                    }
                }
            }
        }

        printf("\n");
        func = func->next;
        idx++;
    }
}

/**
 * Print reactive state information
 */
static void print_state(cJSON* kir_root, bool use_color) {
    const char* header_color = use_color ? COLOR_HEADER : "";
    const char* key_color = use_color ? COLOR_KEY : "";
    const char* value_color = use_color ? COLOR_VALUE : "";
    const char* type_color = use_color ? COLOR_TYPE : "";
    const char* reset = use_color ? COLOR_RESET : "";

    printf("\n");
    printf("%s═══════════════════════════════════════════════════════════%s\n", header_color, reset);
    printf("%s  Reactive State%s\n", header_color, reset);
    printf("%s═══════════════════════════════════════════════════════════%s\n", header_color, reset);
    printf("\n");

    // Get reactive_manifest
    cJSON* reactive_manifest = cJSON_GetObjectItem(kir_root, "reactive_manifest");
    if (!reactive_manifest) {
        printf("  %sNo reactive manifest found%s\n", key_color, reset);
        return;
    }

    // Print variables
    cJSON* variables = cJSON_GetObjectItem(reactive_manifest, "variables");
    if (variables && cJSON_IsArray(variables)) {
        int var_count = cJSON_GetArraySize(variables);
        printf("  %sReactive Variables:%s %s%d%s\n\n", key_color, reset, value_color, var_count, reset);

        for (int i = 0; i < var_count; i++) {
            cJSON* var = cJSON_GetArrayItem(variables, i);
            cJSON* id = cJSON_GetObjectItem(var, "id");
            cJSON* name = cJSON_GetObjectItem(var, "name");
            cJSON* type = cJSON_GetObjectItem(var, "type");
            cJSON* type_string = cJSON_GetObjectItem(var, "type_string");
            cJSON* initial_value = cJSON_GetObjectItem(var, "initial_value_json");
            cJSON* scope = cJSON_GetObjectItem(var, "scope");

            if (name && name->valuestring) {
                printf("  %s[%d]%s %s%s%s", key_color, i + 1, reset, value_color, name->valuestring, reset);

                if (type_string && type_string->valuestring) {
                    printf(" %s(%s%s%s)%s", key_color, type_color, type_string->valuestring, key_color, reset);
                } else if (type && type->valuestring) {
                    printf(" %s(%s%s%s)%s", key_color, type_color, type->valuestring, key_color, reset);
                }

                if (initial_value && initial_value->valuestring) {
                    printf(" %s=%s %s%s%s", key_color, reset, value_color, initial_value->valuestring, reset);
                }

                if (id) {
                    printf(" %s[ID: %d]%s", key_color, id->valueint, reset);
                }

                if (scope && scope->valuestring) {
                    printf(" %s{%s}%s", key_color, scope->valuestring, reset);
                }

                printf("\n");
            }
        }
        printf("\n");
    } else {
        printf("  %sReactive Variables:%s %s0%s\n\n", key_color, reset, value_color, reset);
    }

    // Print bindings
    cJSON* bindings = cJSON_GetObjectItem(reactive_manifest, "bindings");
    if (bindings && cJSON_IsArray(bindings)) {
        int binding_count = cJSON_GetArraySize(bindings);
        printf("  %sReactive Bindings:%s %s%d%s\n\n", key_color, reset, value_color, binding_count, reset);

        for (int i = 0; i < binding_count; i++) {
            cJSON* binding = cJSON_GetArrayItem(bindings, i);
            cJSON* comp_id = cJSON_GetObjectItem(binding, "component_id");
            cJSON* var_id = cJSON_GetObjectItem(binding, "reactive_var_id");
            cJSON* binding_type = cJSON_GetObjectItem(binding, "binding_type");
            cJSON* expression = cJSON_GetObjectItem(binding, "expression");

            if (comp_id && var_id) {
                printf("  %s[%d]%s Comp#%s%d%s → Var#%s%d%s",
                       key_color, i + 1, reset,
                       value_color, comp_id->valueint, reset,
                       value_color, var_id->valueint, reset);

                if (binding_type && binding_type->valuestring) {
                    printf(" %s[%s%s%s]%s",
                           key_color, type_color, binding_type->valuestring, key_color, reset);
                }

                if (expression && expression->valuestring) {
                    printf(" %s'%s%s%s'%s",
                           key_color, value_color, expression->valuestring, key_color, reset);
                }

                printf("\n");
            }
        }
        printf("\n");
    } else {
        printf("  %sReactive Bindings:%s %s0%s\n\n", key_color, reset, value_color, reset);
    }

    // Print conditionals
    cJSON* conditionals = cJSON_GetObjectItem(reactive_manifest, "conditionals");
    if (conditionals && cJSON_IsArray(conditionals)) {
        int cond_count = cJSON_GetArraySize(conditionals);
        printf("  %sConditionals:%s %s%d%s\n", key_color, reset, value_color, cond_count, reset);

        if (cond_count > 0) {
            printf("\n");
            for (int i = 0; i < cond_count; i++) {
                cJSON* cond = cJSON_GetArrayItem(conditionals, i);
                cJSON* comp_id = cJSON_GetObjectItem(cond, "component_id");
                cJSON* condition = cJSON_GetObjectItem(cond, "condition");

                if (comp_id) {
                    printf("  %s[%d]%s Comp#%s%d%s",
                           key_color, i + 1, reset,
                           value_color, comp_id->valueint, reset);

                    if (condition && condition->valuestring) {
                        printf(" %s'%s%s%s'%s",
                               key_color, value_color, condition->valuestring, key_color, reset);
                    }

                    printf("\n");
                }
            }
            printf("\n");
        }
    } else {
        printf("  %sConditionals:%s %s0%s\n", key_color, reset, value_color, reset);
    }

    // Print for loops
    cJSON* for_loops = cJSON_GetObjectItem(reactive_manifest, "for_loops");
    if (for_loops && cJSON_IsArray(for_loops)) {
        int loop_count = cJSON_GetArraySize(for_loops);
        printf("  %sFor Loops:%s %s%d%s\n", key_color, reset, value_color, loop_count, reset);

        if (loop_count > 0) {
            printf("\n");
            for (int i = 0; i < loop_count; i++) {
                cJSON* loop = cJSON_GetArrayItem(for_loops, i);
                cJSON* parent_id = cJSON_GetObjectItem(loop, "parent_component_id");
                cJSON* collection = cJSON_GetObjectItem(loop, "collection_expr");
                cJSON* loop_var = cJSON_GetObjectItem(loop, "loop_variable_name");

                if (parent_id) {
                    printf("  %s[%d]%s ParentComp#%s%d%s",
                           key_color, i + 1, reset,
                           value_color, parent_id->valueint, reset);

                    if (loop_var && loop_var->valuestring) {
                        printf(" %s%s%s", type_color, loop_var->valuestring, reset);
                    }

                    if (collection && collection->valuestring) {
                        printf(" %sin%s %s%s%s",
                               key_color, reset, value_color, collection->valuestring, reset);
                    }

                    printf("\n");
                }
            }
            printf("\n");
        }
    } else {
        printf("  %sFor Loops:%s %s0%s\n\n", key_color, reset, value_color, reset);
    }
}

/**
 * Recursively print component styles
 */
static void print_styles_recursive(IRComponent* component, int depth, bool use_color) {
    if (!component) return;

    const char* key_color = use_color ? COLOR_KEY : "";
    const char* value_color = use_color ? COLOR_VALUE : "";
    const char* type_color = use_color ? COLOR_TYPE : "";
    const char* reset = use_color ? COLOR_RESET : "";

    // Print indentation
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    // Print component type and ID
    printf("%s%s%s %s(ID: %u)%s\n",
           type_color, ir_component_type_to_string(component->type), reset,
           key_color, component->id, reset);

    // Print styles if present
    if (component->style) {
        IRStyle* s = component->style;

        // Dimensions
        if (s->width.value > 0) {
            for (int i = 0; i <= depth; i++) printf("  ");
            printf("%swidth:%s %s%.0f%s%s\n",
                   key_color, reset, value_color, s->width.value,
                   dimension_type_to_string(s->width.type), reset);
        }

        if (s->height.value > 0) {
            for (int i = 0; i <= depth; i++) printf("  ");
            printf("%sheight:%s %s%.0f%s%s\n",
                   key_color, reset, value_color, s->height.value,
                   dimension_type_to_string(s->height.type), reset);
        }

        // Background color
        if (s->background.type == IR_COLOR_SOLID && s->background.data.a > 0) {
            for (int i = 0; i <= depth; i++) printf("  ");
            printf("%sbackground:%s %s#%02x%02x%02x%s",
                   key_color, reset, value_color,
                   s->background.data.r,
                   s->background.data.g,
                   s->background.data.b, reset);
            if (s->background.data.a < 255) {
                printf(" %s(alpha: %d)%s", key_color, s->background.data.a, reset);
            }
            printf("\n");
        }

        // Border
        if (s->border.width > 0) {
            for (int i = 0; i <= depth; i++) printf("  ");
            printf("%sborder:%s %s%.0fpx%s",
                   key_color, reset, value_color, s->border.width, reset);

            if (s->border.color.type == IR_COLOR_SOLID) {
                printf(" %s#%02x%02x%02x%s",
                       value_color,
                       s->border.color.data.r,
                       s->border.color.data.g,
                       s->border.color.data.b, reset);
            }

            if (s->border.radius > 0) {
                printf(" %sradius: %d%s", key_color, s->border.radius, reset);
            }
            printf("\n");
        }

        // Padding
        if (s->padding.top > 0 || s->padding.right > 0 ||
            s->padding.bottom > 0 || s->padding.left > 0) {
            for (int i = 0; i <= depth; i++) printf("  ");
            printf("%spadding:%s %s%.0f %.0f %.0f %.0f%s\n",
                   key_color, reset, value_color,
                   s->padding.top, s->padding.right,
                   s->padding.bottom, s->padding.left, reset);
        }

        // Margin
        if (s->margin.top > 0 || s->margin.right > 0 ||
            s->margin.bottom > 0 || s->margin.left > 0) {
            for (int i = 0; i <= depth; i++) printf("  ");
            printf("%smargin:%s %s%.0f %.0f %.0f %.0f%s\n",
                   key_color, reset, value_color,
                   s->margin.top, s->margin.right,
                   s->margin.bottom, s->margin.left, reset);
        }

        // Typography
        if (s->font.size > 0) {
            for (int i = 0; i <= depth; i++) printf("  ");
            printf("%sfontSize:%s %s%.0f%s", key_color, reset, value_color, s->font.size, reset);

            if (s->font.color.type == IR_COLOR_SOLID) {
                printf(" %scolor:%s %s#%02x%02x%02x%s",
                       key_color, reset, value_color,
                       s->font.color.data.r,
                       s->font.color.data.g,
                       s->font.color.data.b, reset);
            }

            if (s->font.bold) {
                printf(" %sbold%s", key_color, reset);
            }

            if (s->font.italic) {
                printf(" %sitalic%s", key_color, reset);
            }

            printf("\n");
        }

        // Opacity
        if (s->opacity < 1.0f && s->opacity > 0.0f) {
            for (int i = 0; i <= depth; i++) printf("  ");
            printf("%sopacity:%s %s%.2f%s\n",
                   key_color, reset, value_color, s->opacity, reset);
        }
    }

    // Recursively print children
    for (uint32_t i = 0; i < component->child_count; i++) {
        print_styles_recursive(component->children[i], depth + 1, use_color);
    }
}

/**
 * Print detailed styling information
 */
static void print_styles(IRComponent* root, bool use_color) {
    const char* header_color = use_color ? COLOR_HEADER : "";
    const char* reset = use_color ? COLOR_RESET : "";

    printf("\n");
    printf("%s═══════════════════════════════════════════════════════════%s\n", header_color, reset);
    printf("%s  Detailed Styling Information%s\n", header_color, reset);
    printf("%s═══════════════════════════════════════════════════════════%s\n", header_color, reset);
    printf("\n");

    print_styles_recursive(root, 0, use_color);

    printf("\n");
}

static void print_usage(void) {
    printf("Usage: kryon inspect [options] <file.kir>\n\n");
    printf("Options:\n");
    printf("  --tree, -t         Show component tree structure\n");
    printf("  --json, -j         Output as formatted JSON\n");
    printf("  --validate, -v     Validate IR structure\n");
    printf("  --stats, -s        Show statistics (default)\n");
    printf("  --events, -e       Show event handlers and bindings\n");
    printf("  --logic, -l        Display logic functions and handler code\n");
    printf("  --state, -r        Show reactive variables and bindings\n");
    printf("  --styles           Display detailed styling information\n");
    printf("  --full, -f         Show everything (all inspection modes)\n");
    printf("  --no-color         Disable colored output\n");
    printf("  --help, -h         Show this help\n\n");
    printf("Examples:\n");
    printf("  kryon inspect app.kir              # Show statistics\n");
    printf("  kryon inspect --tree app.kir       # Show tree structure\n");
    printf("  kryon inspect --state app.kir      # Show reactive state\n");
    printf("  kryon inspect --full app.kir       # Show everything\n");
}

static int count_components(IRComponent* component) {
    if (!component) return 0;

    int count = 1;
    for (uint32_t i = 0; i < component->child_count; i++) {
        count += count_components(component->children[i]);
    }
    return count;
}

static int get_max_depth(IRComponent* component, int current_depth) {
    if (!component) return current_depth;

    int max = current_depth;
    for (uint32_t i = 0; i < component->child_count; i++) {
        int child_depth = get_max_depth(component->children[i], current_depth + 1);
        if (child_depth > max) max = child_depth;
    }
    return max;
}

static void print_stats(IRComponent* root) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  KIR Statistics\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    int total_components = count_components(root);
    int max_depth = get_max_depth(root, 0);

    printf("  Total components: %d\n", total_components);
    printf("  Maximum depth:    %d\n", max_depth);
    printf("  Root type:        %s\n", ir_component_type_to_string(root->type));

    if (root->id > 0) {
        printf("  Root ID:          %u\n", root->id);
    }

    if (root->style) {
        printf("\n  Root styling:\n");
        if (root->style->width.value > 0) {
            printf("    Width:  %.0f (type %d)\n", root->style->width.value, root->style->width.type);
        }
        if (root->style->height.value > 0) {
            printf("    Height: %.0f (type %d)\n", root->style->height.value, root->style->height.type);
        }
    }

    printf("\n");
}

int cmd_inspect(int argc, char** argv) {
    bool show_tree = false;
    bool show_json = false;
    bool validate = false;
    bool show_stats = false;
    bool show_events = false;
    bool show_logic = false;
    bool show_state = false;
    bool show_styles = false;
    bool show_full = false;
    bool use_color = true;
    const char* file_path = NULL;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "--tree") == 0 || strcmp(argv[i], "-t") == 0) {
            show_tree = true;
        } else if (strcmp(argv[i], "--json") == 0 || strcmp(argv[i], "-j") == 0) {
            show_json = true;
        } else if (strcmp(argv[i], "--validate") == 0 || strcmp(argv[i], "-v") == 0) {
            validate = true;
        } else if (strcmp(argv[i], "--stats") == 0 || strcmp(argv[i], "-s") == 0) {
            show_stats = true;
        } else if (strcmp(argv[i], "--events") == 0 || strcmp(argv[i], "-e") == 0) {
            show_events = true;
        } else if (strcmp(argv[i], "--logic") == 0 || strcmp(argv[i], "-l") == 0) {
            show_logic = true;
        } else if (strcmp(argv[i], "--state") == 0 || strcmp(argv[i], "-r") == 0) {
            show_state = true;
        } else if (strcmp(argv[i], "--styles") == 0) {
            show_styles = true;
        } else if (strcmp(argv[i], "--full") == 0 || strcmp(argv[i], "-f") == 0) {
            show_full = true;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            use_color = false;
        } else if (argv[i][0] != '-') {
            file_path = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    // Handle --full flag
    if (show_full) {
        show_tree = show_stats = show_events = show_logic = show_state = show_styles = true;
    }

    // Check if file is provided
    if (!file_path) {
        fprintf(stderr, "Error: No file specified\n\n");
        print_usage();
        return 1;
    }

    // Check if file exists
    if (!file_exists(file_path)) {
        fprintf(stderr, "Error: File not found: %s\n", file_path);
        return 1;
    }

    // Load KIR file
    printf("Loading KIR file: %s\n", file_path);
    IRComponent* root = ir_read_json_file(file_path);

    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file\n");
        return 1;
    }

    printf("✓ Successfully loaded KIR file\n");

    // Load complete JSON if needed for events, logic, or state
    cJSON* kir_json = NULL;
    if (show_events || show_logic || show_state) {
        kir_json = load_kir_json(file_path);
        if (!kir_json) {
            fprintf(stderr, "Warning: Failed to load complete KIR JSON\n");
        }
    }

    // Validate if requested
    if (validate) {
        printf("\nValidating IR structure...\n");
        bool is_valid = ir_validate_component(root);

        if (is_valid) {
            printf("✓ IR structure is valid\n");
        } else {
            fprintf(stderr, "✗ IR structure validation failed\n");
            if (kir_json) cJSON_Delete(kir_json);
            ir_pool_free_component(root);
            return 1;
        }
    }

    // Show stats if requested or as default
    if (show_stats || (!show_tree && !show_json && !show_events && !show_logic && !show_state && !show_styles)) {
        print_stats(root);
    }

    // Show tree if requested
    if (show_tree) {
        printf("\n");
        printf("═══════════════════════════════════════════════════════════\n");
        printf("  Component Tree\n");
        printf("═══════════════════════════════════════════════════════════\n\n");
        ir_print_component_info(root, 0);
        printf("\n");
    }

    // Show events if requested
    if (show_events && kir_json) {
        print_events(kir_json, root, use_color);
    }

    // Show logic if requested
    if (show_logic && kir_json) {
        print_logic(kir_json, use_color);
    }

    // Show state if requested
    if (show_state && kir_json) {
        print_state(kir_json, use_color);
    }

    // Show styles if requested
    if (show_styles) {
        print_styles(root, use_color);
    }

    // Show JSON if requested
    if (show_json) {
        char* json = ir_serialize_json(root, NULL);
        if (json) {
            printf("\n");
            printf("═══════════════════════════════════════════════════════════\n");
            printf("  JSON Output\n");
            printf("═══════════════════════════════════════════════════════════\n\n");
            printf("%s\n", json);
            free(json);
        } else {
            fprintf(stderr, "Error: Failed to serialize to JSON\n");
        }
    }

    // Cleanup
    if (kir_json) cJSON_Delete(kir_json);
    ir_pool_free_component(root);
    return 0;
}
