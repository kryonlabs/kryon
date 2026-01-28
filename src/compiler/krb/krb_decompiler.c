/**
 * @file krb_decompiler.c
 * @brief KRB Decompiler Implementation
 */

#include "krb_decompiler.h"
#include "krb_format.h"
#include "../../shared/kryon_mappings.h"
#include "memory.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// =============================================================================
// INTERNAL STRUCTURES
// =============================================================================

#define MAX_DECOMPILER_ERRORS 100

struct KryonKrbDecompiler {
    KryonDecompilerConfig config;
    KryonDecompilerStats stats;
    char **errors;
    size_t error_count;
    size_t error_capacity;

    // Decompilation context
    const KryonKrbFile *krb_file;
    KryonASTNode **element_map;     // Map KRB element ID → AST node
    size_t element_map_size;
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static void add_error(KryonKrbDecompiler *decompiler, const char *format, ...) {
    if (!decompiler || decompiler->error_count >= MAX_DECOMPILER_ERRORS) {
        return;
    }

    if (decompiler->error_count >= decompiler->error_capacity) {
        size_t new_capacity = decompiler->error_capacity == 0 ? 10 : decompiler->error_capacity * 2;
        char **new_errors = realloc(decompiler->errors, new_capacity * sizeof(char*));
        if (!new_errors) return;
        decompiler->errors = new_errors;
        decompiler->error_capacity = new_capacity;
    }

    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    decompiler->errors[decompiler->error_count] = strdup(buffer);
    if (decompiler->errors[decompiler->error_count]) {
        decompiler->error_count++;
    }
}

static KryonASTNode *create_ast_node(KryonASTNodeType type) {
    KryonASTNode *node = calloc(1, sizeof(KryonASTNode));
    if (!node) return NULL;

    node->type = type;
    node->location.line = 1;
    node->location.column = 1;
    node->location.filename = "decompiled.krb";
    node->location.offset = 0;
    node->location.length = 0;

    return node;
}

static const char *get_string_from_table(const KryonKrbFile *krb_file, uint16_t string_id) {
    if (!krb_file || !krb_file->string_table || string_id >= krb_file->string_count) {
        return "";
    }
    return krb_file->string_table[string_id];
}

// =============================================================================
// VALUE RECONSTRUCTION
// =============================================================================

static KryonASTNode *decompile_property_value(KryonKrbDecompiler *decompiler,
                                               const KryonKrbProperty *property) {
    KryonASTNode *value_node = NULL;

    switch (property->type) {
        case KRYON_PROPERTY_STRING: {
            value_node = create_ast_node(KRYON_AST_LITERAL);
            if (!value_node) return NULL;

            const char *str = property->value.string_value;
            value_node->data.literal.value = kryon_ast_value_string(str ? str : "");
            break;
        }

        case KRYON_PROPERTY_INTEGER: {
            value_node = create_ast_node(KRYON_AST_LITERAL);
            if (!value_node) return NULL;

            value_node->data.literal.value = kryon_ast_value_integer(property->value.int_value);
            break;
        }

        case KRYON_PROPERTY_FLOAT: {
            value_node = create_ast_node(KRYON_AST_LITERAL);
            if (!value_node) return NULL;

            value_node->data.literal.value = kryon_ast_value_float(property->value.float_value);
            break;
        }

        case KRYON_PROPERTY_BOOLEAN: {
            value_node = create_ast_node(KRYON_AST_LITERAL);
            if (!value_node) return NULL;

            value_node->data.literal.value = kryon_ast_value_boolean(property->value.bool_value);
            break;
        }

        case KRYON_PROPERTY_COLOR: {
            value_node = create_ast_node(KRYON_AST_LITERAL);
            if (!value_node) return NULL;

            value_node->data.literal.value = kryon_ast_value_color(property->value.color_value);
            break;
        }

        case KRYON_PROPERTY_SIZE: {
            // Reconstruct as object literal with width/height
            value_node = create_ast_node(KRYON_AST_OBJECT_LITERAL);
            if (!value_node) return NULL;

            value_node->data.object_literal.properties = NULL;
            value_node->data.object_literal.property_count = 0;
            value_node->data.object_literal.property_capacity = 0;

            // Create width property
            KryonASTNode *width_prop = create_ast_node(KRYON_AST_PROPERTY);
            if (!width_prop) {
                free(value_node);
                return NULL;
            }
            width_prop->data.property.name = strdup("width");

            KryonASTNode *width_value = create_ast_node(KRYON_AST_LITERAL);
            if (!width_value) {
                free(width_prop->data.property.name);
                free(width_prop);
                free(value_node);
                return NULL;
            }
            width_value->data.literal.value = kryon_ast_value_float(property->value.size_value.width);
            width_prop->data.property.value = width_value;
            width_value->parent = width_prop;

            // Create height property
            KryonASTNode *height_prop = create_ast_node(KRYON_AST_PROPERTY);
            if (!height_prop) {
                free(width_value);
                free(width_prop->data.property.name);
                free(width_prop);
                free(value_node);
                return NULL;
            }
            height_prop->data.property.name = strdup("height");

            KryonASTNode *height_value = create_ast_node(KRYON_AST_LITERAL);
            if (!height_value) {
                free(height_prop->data.property.name);
                free(height_prop);
                free(width_value);
                free(width_prop->data.property.name);
                free(width_prop);
                free(value_node);
                return NULL;
            }
            height_value->data.literal.value = kryon_ast_value_float(property->value.size_value.height);
            height_prop->data.property.value = height_value;
            height_value->parent = height_prop;

            // Add properties to object literal
            value_node->data.object_literal.property_capacity = 2;
            value_node->data.object_literal.property_count = 2;
            value_node->data.object_literal.properties = malloc(2 * sizeof(KryonASTNode*));
            if (!value_node->data.object_literal.properties) {
                free(height_value);
                free(height_prop->data.property.name);
                free(height_prop);
                free(width_value);
                free(width_prop->data.property.name);
                free(width_prop);
                free(value_node);
                return NULL;
            }
            value_node->data.object_literal.properties[0] = width_prop;
            value_node->data.object_literal.properties[1] = height_prop;
            width_prop->parent = value_node;
            height_prop->parent = value_node;

            break;
        }

        case KRYON_PROPERTY_EVENT_HANDLER: {
            // Event handler stored as function name string
            value_node = create_ast_node(KRYON_AST_IDENTIFIER);
            if (!value_node) return NULL;

            const char *handler_name = property->value.string_value;
            value_node->data.identifier.name = handler_name ? strdup(handler_name) : strdup("");
            break;
        }

        default:
            add_error(decompiler, "Unsupported property type: %d", property->type);
            return NULL;
    }

    return value_node;
}

// =============================================================================
// ELEMENT RECONSTRUCTION
// =============================================================================

static KryonASTNode *decompile_element(KryonKrbDecompiler *decompiler,
                                        const KryonKrbElement *krb_element) {
    if (!krb_element) {
        add_error(decompiler, "NULL KRB element");
        return NULL;
    }

    // Create element AST node
    KryonASTNode *element_node = create_ast_node(KRYON_AST_ELEMENT);
    if (!element_node) {
        add_error(decompiler, "Failed to allocate element node");
        return NULL;
    }

    // Get element type name from hex code
    const char *element_type_name = kryon_get_element_name(krb_element->type);
    if (!element_type_name) {
        char hex_str[16];
        snprintf(hex_str, sizeof(hex_str), "0x%04X", krb_element->type);
        element_type_name = hex_str;
    }

    element_node->data.element.element_type = strdup(element_type_name);

    // Initialize properties array
    if (krb_element->property_count > 0) {
        element_node->data.element.property_capacity = krb_element->property_count;
        element_node->data.element.properties = calloc(krb_element->property_count,
                                                        sizeof(KryonASTNode*));
        if (!element_node->data.element.properties) {
            add_error(decompiler, "Failed to allocate properties array");
            free(element_node);
            return NULL;
        }
    }

    // Decompile properties
    for (uint16_t i = 0; i < krb_element->property_count; i++) {
        const KryonKrbProperty *krb_prop = &krb_element->properties[i];

        // Create property AST node
        KryonASTNode *prop_node = create_ast_node(KRYON_AST_PROPERTY);
        if (!prop_node) {
            add_error(decompiler, "Failed to allocate property node");
            continue;
        }

        // Get property name from string table or hex code
        const char *prop_name = get_string_from_table(decompiler->krb_file, krb_prop->name_id);
        if (!prop_name || strlen(prop_name) == 0) {
            // Try to resolve from property hex code
            prop_name = kryon_get_property_name(krb_prop->name_id);
        }

        prop_node->data.property.name = prop_name ? strdup(prop_name) : strdup("unknown");

        // Decompile property value
        KryonASTNode *value_node = decompile_property_value(decompiler, krb_prop);
        if (value_node) {
            prop_node->data.property.value = value_node;
            element_node->data.element.properties[element_node->data.element.property_count++] = prop_node;
            decompiler->stats.properties_decompiled++;
        } else {
            free(prop_node);
        }
    }

    // Initialize children array
    if (krb_element->child_count > 0) {
        element_node->data.element.child_capacity = krb_element->child_count;
        element_node->data.element.children = calloc(krb_element->child_count,
                                                      sizeof(KryonASTNode*));
        if (!element_node->data.element.children) {
            add_error(decompiler, "Failed to allocate children array");
        }
    }

    decompiler->stats.elements_decompiled++;
    return element_node;
}

// =============================================================================
// TREE RECONSTRUCTION
// =============================================================================

static bool build_element_tree(KryonKrbDecompiler *decompiler, KryonASTNode *root_node) {
    const KryonKrbFile *krb_file = decompiler->krb_file;

    // First pass: Create AST nodes for all elements and store in map
    decompiler->element_map_size = krb_file->header.element_count;
    decompiler->element_map = calloc(decompiler->element_map_size, sizeof(KryonASTNode*));
    if (!decompiler->element_map) {
        add_error(decompiler, "Failed to allocate element map");
        return false;
    }

    // Decompile all elements
    for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
        const KryonKrbElement *krb_elem = &krb_file->elements[i];
        KryonASTNode *elem_node = decompile_element(decompiler, krb_elem);

        if (elem_node && krb_elem->id < decompiler->element_map_size) {
            decompiler->element_map[krb_elem->id] = elem_node;
        }
    }

    // Second pass: Build parent-child relationships
    for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
        const KryonKrbElement *krb_elem = &krb_file->elements[i];

        if (krb_elem->id >= decompiler->element_map_size) continue;
        KryonASTNode *elem_node = decompiler->element_map[krb_elem->id];
        if (!elem_node) continue;

        // If this is a root element (parent_id == 0), add to root
        if (krb_elem->parent_id == 0) {
            // Add to root node's children
            if (root_node->data.element.child_count < root_node->data.element.child_capacity) {
                root_node->data.element.children[root_node->data.element.child_count++] = elem_node;
            }
        } else {
            // Find parent and add as child
            if (krb_elem->parent_id < decompiler->element_map_size) {
                KryonASTNode *parent_node = decompiler->element_map[krb_elem->parent_id];
                if (parent_node && parent_node->data.element.child_count < parent_node->data.element.child_capacity) {
                    parent_node->data.element.children[parent_node->data.element.child_count++] = elem_node;
                }
            }
        }
    }

    return true;
}

// =============================================================================
// MAIN DECOMPILATION
// =============================================================================

static bool decompile_krb_internal(KryonKrbDecompiler *decompiler,
                                    const KryonKrbFile *krb_file,
                                    KryonASTNode **out_ast) {
    if (!decompiler || !krb_file || !out_ast) {
        return false;
    }

    // Reset stats
    memset(&decompiler->stats, 0, sizeof(decompiler->stats));
    decompiler->krb_file = krb_file;

    // Create root AST node
    KryonASTNode *root_node = create_ast_node(KRYON_AST_ROOT);
    if (!root_node) {
        add_error(decompiler, "Failed to create root node");
        return false;
    }

    // Allocate root children array (we don't know exact count, estimate)
    root_node->data.element.child_capacity = krb_file->header.element_count;
    root_node->data.element.children = calloc(root_node->data.element.child_capacity,
                                               sizeof(KryonASTNode*));
    if (!root_node->data.element.children) {
        add_error(decompiler, "Failed to allocate root children array");
        free(root_node);
        return false;
    }

    // Build element tree
    if (!build_element_tree(decompiler, root_node)) {
        add_error(decompiler, "Failed to build element tree");
        free(root_node);
        return false;
    }

    // Update stats
    decompiler->stats.strings_recovered = krb_file->string_count;
    decompiler->stats.total_ast_nodes = decompiler->stats.elements_decompiled +
                                         decompiler->stats.properties_decompiled + 1;

    *out_ast = root_node;
    return true;
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

KryonDecompilerConfig kryon_decompiler_default_config(void) {
    KryonDecompilerConfig config = {
        .preserve_ids = true,
        .reconstruct_locations = false,
        .expand_templates = false,
        .include_metadata = true,
        .source_hint = NULL
    };
    return config;
}

KryonKrbDecompiler *kryon_decompiler_create(const KryonDecompilerConfig *config) {
    KryonKrbDecompiler *decompiler = calloc(1, sizeof(KryonKrbDecompiler));
    if (!decompiler) {
        return NULL;
    }

    if (config) {
        decompiler->config = *config;
    } else {
        decompiler->config = kryon_decompiler_default_config();
    }

    decompiler->errors = NULL;
    decompiler->error_count = 0;
    decompiler->error_capacity = 0;

    return decompiler;
}

void kryon_decompiler_destroy(KryonKrbDecompiler *decompiler) {
    if (!decompiler) return;

    // Free errors
    for (size_t i = 0; i < decompiler->error_count; i++) {
        free(decompiler->errors[i]);
    }
    free(decompiler->errors);

    // Free element map
    if (decompiler->element_map) {
        free(decompiler->element_map);
    }

    free(decompiler);
}

bool kryon_decompile_file(KryonKrbDecompiler *decompiler,
                          const char *krb_file_path,
                          KryonASTNode **out_ast) {
    if (!decompiler || !krb_file_path || !out_ast) {
        return false;
    }

    // Use existing KRB reader to parse the binary file
    KryonKrbReader *reader = kryon_krb_reader_create_file(krb_file_path);
    if (!reader) {
        add_error(decompiler, "Failed to create KRB reader for file: %s", krb_file_path);
        return false;
    }

    KryonKrbFile *krb_file = kryon_krb_reader_parse(reader);
    if (!krb_file) {
        add_error(decompiler, "Failed to parse KRB file: %s", kryon_krb_reader_get_error(reader));
        kryon_krb_reader_destroy(reader);
        return false;
    }

    bool result = decompile_krb_internal(decompiler, krb_file, out_ast);

    kryon_krb_reader_destroy(reader);
    return result;
}

bool kryon_decompile_memory(KryonKrbDecompiler *decompiler,
                            const uint8_t *krb_data,
                            size_t krb_size,
                            KryonASTNode **out_ast) {
    if (!decompiler || !krb_data || krb_size == 0 || !out_ast) {
        return false;
    }

    KryonKrbReader *reader = kryon_krb_reader_create_memory(krb_data, krb_size);
    if (!reader) {
        add_error(decompiler, "Failed to create KRB reader for memory buffer");
        return false;
    }

    KryonKrbFile *krb_file = kryon_krb_reader_parse(reader);
    if (!krb_file) {
        add_error(decompiler, "Failed to parse KRB from memory: %s", kryon_krb_reader_get_error(reader));
        kryon_krb_reader_destroy(reader);
        return false;
    }

    bool result = decompile_krb_internal(decompiler, krb_file, out_ast);

    kryon_krb_reader_destroy(reader);
    return result;
}

bool kryon_decompile_krb(KryonKrbDecompiler *decompiler,
                         const KryonKrbFile *krb_file,
                         KryonASTNode **out_ast) {
    return decompile_krb_internal(decompiler, krb_file, out_ast);
}

const char **kryon_decompiler_get_errors(const KryonKrbDecompiler *decompiler,
                                         size_t *count) {
    if (!decompiler || !count) {
        if (count) *count = 0;
        return NULL;
    }

    *count = decompiler->error_count;
    return (const char **)decompiler->errors;
}

const KryonDecompilerStats *kryon_decompiler_get_stats(const KryonKrbDecompiler *decompiler) {
    return decompiler ? &decompiler->stats : NULL;
}

bool kryon_decompile_simple(const char *krb_file_path, KryonASTNode **out_ast) {
    if (!krb_file_path || !out_ast) {
        return false;
    }

    KryonKrbDecompiler *decompiler = kryon_decompiler_create(NULL);
    if (!decompiler) {
        return false;
    }

    bool result = kryon_decompile_file(decompiler, krb_file_path, out_ast);
    kryon_decompiler_destroy(decompiler);

    return result;
}
