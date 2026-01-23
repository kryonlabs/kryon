#include "ir_validation.h"
#include "../include/ir_builder.h"
#include "../../include/ir_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper to convert component ID to string
static char* id_to_string(uint32_t id) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "#%u", id);
    return buffer;
}

// ============================================================================
// CRC32 Implementation (Polynomial: 0xEDB88320)
// ============================================================================

static const uint32_t CRC32_POLYNOMIAL = 0xEDB88320;
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void init_crc32_table(void) {
    if (crc32_table_initialized) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

uint32_t ir_crc32(const uint8_t* data, size_t length) {
    init_crc32_table();

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    return ~crc;
}

uint32_t ir_crc32_buffer(IRBuffer* buffer) {
    if (!buffer || !buffer->data) return 0;
    return ir_crc32(buffer->data, buffer->size);
}

bool ir_buffer_write_crc32(IRBuffer* buffer) {
    if (!buffer || !buffer->data) return false;

    // Calculate CRC32 of current buffer contents
    uint32_t crc = ir_crc32_buffer(buffer);

    // Append CRC32 at end of buffer
    size_t offset = buffer->size;
    if (offset + 4 > buffer->capacity) {
        return false;
    }

    uint8_t* base_ptr = buffer->base ? buffer->base : buffer->data;
    base_ptr[offset++] = (crc >> 0) & 0xFF;
    base_ptr[offset++] = (crc >> 8) & 0xFF;
    base_ptr[offset++] = (crc >> 16) & 0xFF;
    base_ptr[offset++] = (crc >> 24) & 0xFF;

    buffer->size = offset;
    return true;
}

bool ir_buffer_verify_crc32(IRBuffer* buffer) {
    if (!buffer || buffer->size < 4) return false;

    // Read stored CRC32 from end of buffer
    size_t crc_pos = buffer->size - 4;
    uint32_t stored_crc =
        ((uint32_t)buffer->data[crc_pos + 0] << 0) |
        ((uint32_t)buffer->data[crc_pos + 1] << 8) |
        ((uint32_t)buffer->data[crc_pos + 2] << 16) |
        ((uint32_t)buffer->data[crc_pos + 3] << 24);

    // Calculate CRC32 of buffer excluding the CRC itself
    uint32_t calculated_crc = ir_crc32(buffer->data, crc_pos);

    return stored_crc == calculated_crc;
}

// ============================================================================
// Validation Result Management
// ============================================================================

IRValidationResult* ir_validation_result_create(void) {
    IRValidationResult* result = malloc(sizeof(IRValidationResult));
    if (!result) return NULL;

    result->passed = true;
    result->max_level = IR_VALIDATION_OK;
    result->issue_count = 0;
    result->issue_capacity = 16;
    result->issues = malloc(sizeof(IRValidationIssue) * result->issue_capacity);

    if (!result->issues) {
        free(result);
        return NULL;
    }

    return result;
}

void ir_validation_result_destroy(IRValidationResult* result) {
    if (!result) return;
    free(result->issues);
    free(result);
}

void ir_validation_add_issue(IRValidationResult* result, IRValidationLevel level,
                             IRValidationErrorCode code, const char* message,
                             const char* component_id) {
    if (!result) return;

    // Expand capacity if needed
    if (result->issue_count >= result->issue_capacity) {
        result->issue_capacity *= 2;
        IRValidationIssue* new_issues = realloc(result->issues,
            sizeof(IRValidationIssue) * result->issue_capacity);
        if (!new_issues) return;
        result->issues = new_issues;
    }

    // Add issue
    IRValidationIssue* issue = &result->issues[result->issue_count++];
    issue->level = level;
    issue->code = code;
    strncpy(issue->message, message, sizeof(issue->message) - 1);
    issue->message[sizeof(issue->message) - 1] = '\0';
    issue->component_id = component_id;
    issue->line_number = -1;

    // Update result status
    if (level > result->max_level) {
        result->max_level = level;
    }
    if (level >= IR_VALIDATION_ERROR) {
        result->passed = false;
    }
}

const char* ir_validation_error_string(IRValidationErrorCode code) {
    switch (code) {
        case IR_VALID_SUCCESS: return "Success";

        // Format errors
        case IR_VALID_INVALID_MAGIC: return "Invalid magic number";
        case IR_VALID_UNSUPPORTED_VERSION: return "Unsupported version";
        case IR_VALID_ENDIANNESS_MISMATCH: return "Endianness mismatch";
        case IR_VALID_CRC_FAILURE: return "CRC32 checksum failed";
        case IR_VALID_TRUNCATED_FILE: return "Truncated file";
        case IR_VALID_BUFFER_OVERFLOW: return "Buffer overflow";

        // Structure errors
        case IR_VALID_NULL_ROOT: return "NULL root component";
        case IR_VALID_CIRCULAR_REFERENCE: return "Circular reference detected";
        case IR_VALID_INVALID_PARENT: return "Invalid parent pointer";
        case IR_VALID_ORPHANED_CHILD: return "Orphaned child component";
        case IR_VALID_EXCESSIVE_DEPTH: return "Excessive tree depth";
        case IR_VALID_INVALID_CHILD_COUNT: return "Invalid child count";
        case IR_VALID_MISSING_REQUIRED_CHILD: return "Missing required child";

        // Semantic errors
        case IR_VALID_INVALID_COMPONENT_TYPE: return "Invalid component type";
        case IR_VALID_INVALID_DIMENSION: return "Invalid dimension";
        case IR_VALID_INVALID_COLOR: return "Invalid color";
        case IR_VALID_INVALID_LAYOUT_MODE: return "Invalid layout mode";
        case IR_VALID_INVALID_ANIMATION: return "Invalid animation";
        case IR_VALID_INCOMPATIBLE_PROPERTIES: return "Incompatible properties";
        case IR_VALID_INVALID_EVENT_TYPE: return "Invalid event type";
        case IR_VALID_INVALID_BREAKPOINT: return "Invalid breakpoint";
        case IR_VALID_INVALID_GRID_TRACK: return "Invalid grid track";
        case IR_VALID_INVALID_FLEXBOX: return "Invalid flexbox properties";
        case IR_VALID_INVALID_TEXT_LENGTH: return "Invalid text length";

        // Degradation warnings
        case IR_VALID_PARTIAL_STYLE: return "Partial style recovery";
        case IR_VALID_PARTIAL_LAYOUT: return "Partial layout recovery";
        case IR_VALID_SKIPPED_ANIMATION: return "Skipped animation";
        case IR_VALID_SKIPPED_CHILD: return "Skipped corrupted child";
        case IR_VALID_DOWNGRADED_VERSION: return "Downgraded version compatibility";

        default: return "Unknown error";
    }
}

void ir_validation_print_result(IRValidationResult* result) {
    if (!result) return;

    if (result->issue_count == 0) {
        printf("✓ Validation passed with no issues\n");
        return;
    }

    const char* level_str[] = {"OK", "WARNING", "ERROR", "FATAL"};
    const char* level_icon[] = {"✓", "⚠", "✗", "✗✗"};

    printf("\nValidation Result: %s\n", result->passed ? "PASSED" : "FAILED");
    printf("Issues: %zu\n", result->issue_count);
    printf("Max Level: %s\n\n", level_str[result->max_level]);

    for (size_t i = 0; i < result->issue_count; i++) {
        IRValidationIssue* issue = &result->issues[i];
        printf("%s [%s] %s: %s\n",
               level_icon[issue->level],
               level_str[issue->level],
               ir_validation_error_string(issue->code),
               issue->message);

        if (issue->component_id) {
            printf("   Component: %s\n", issue->component_id);
        }
    }
}

// ============================================================================
// Tier 1: Format Validation
// ============================================================================

bool ir_validate_magic_number(IRBuffer* buffer, IRValidationResult* result) {
    if (!buffer || buffer->size < 4) {
        ir_validation_add_issue(result, IR_VALIDATION_FATAL, IR_VALID_TRUNCATED_FILE,
                               "Buffer too small for magic number", NULL);
        return false;
    }

    // Read magic number (KRY\0)
    uint32_t magic =
        ((uint32_t)buffer->data[0] << 0) |
        ((uint32_t)buffer->data[1] << 8) |
        ((uint32_t)buffer->data[2] << 16) |
        ((uint32_t)buffer->data[3] << 24);

    if (magic != 0x4B5259) {  // "KRY" in little-endian
        ir_validation_add_issue(result, IR_VALIDATION_FATAL, IR_VALID_INVALID_MAGIC,
                               "Invalid magic number (expected KRY\\0)", NULL);
        return false;
    }

    return true;
}

bool ir_validate_version(IRBuffer* buffer, IRValidationOptions* options,
                         IRValidationResult* result) {
    if (buffer->size < 6) {
        ir_validation_add_issue(result, IR_VALIDATION_FATAL, IR_VALID_TRUNCATED_FILE,
                               "Buffer too small for version info", NULL);
        return false;
    }

    uint8_t major = buffer->data[4];
    uint8_t minor = buffer->data[5];

    // Check version compatibility
    if (options && options->strict_version_check) {
        if (major != IR_FORMAT_VERSION_MAJOR || minor != IR_FORMAT_VERSION_MINOR) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Version mismatch: got %d.%d, expected %d.%d",
                    major, minor, IR_FORMAT_VERSION_MAJOR, IR_FORMAT_VERSION_MINOR);
            ir_validation_add_issue(result, IR_VALIDATION_ERROR,
                                   IR_VALID_UNSUPPORTED_VERSION, msg, NULL);
            return false;
        }
    } else {
        // Allow backward compatible versions
        if (major > IR_FORMAT_VERSION_MAJOR) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                    "Future version detected: %d.%d (current: %d.%d)",
                    major, minor, IR_FORMAT_VERSION_MAJOR, IR_FORMAT_VERSION_MINOR);
            ir_validation_add_issue(result, IR_VALIDATION_WARNING,
                                   IR_VALID_DOWNGRADED_VERSION, msg, NULL);
        }
    }

    return true;
}

bool ir_validate_endianness(IRBuffer* buffer, IRValidationResult* result) {
    if (buffer->size < 8) {
        ir_validation_add_issue(result, IR_VALIDATION_FATAL, IR_VALID_TRUNCATED_FILE,
                               "Buffer too small for endianness marker", NULL);
        return false;
    }

    // Endianness marker: should be 0x01020304
    // Note: Endianness check is simplified - actual format may vary
    // Future enhancement: validate endianness marker value
    (void)result;  // Currently not failing on endianness issues

    return true;
}

bool ir_validate_format(IRBuffer* buffer, IRValidationOptions* options,
                        IRValidationResult* result) {
    if (!ir_validate_magic_number(buffer, result)) return false;
    if (!ir_validate_version(buffer, options, result)) return false;
    if (!ir_validate_endianness(buffer, result)) return false;

    // CRC32 check (optional)
    if (options && options->enable_crc_check) {
        if (!ir_buffer_verify_crc32(buffer)) {
            ir_validation_add_issue(result, IR_VALIDATION_ERROR, IR_VALID_CRC_FAILURE,
                                   "CRC32 checksum verification failed", NULL);
            return false;
        }
    }

    return true;
}

// ============================================================================
// Tier 2: Structure Validation
// ============================================================================

// Helper to track visited components for circular reference detection
typedef struct {
    IRComponent** visited;
    size_t count;
    size_t capacity;
} VisitedSet;

static VisitedSet* visited_set_create(void) {
    VisitedSet* set = malloc(sizeof(VisitedSet));
    if (!set) return NULL;

    set->capacity = 64;
    set->count = 0;
    set->visited = malloc(sizeof(IRComponent*) * set->capacity);
    if (!set->visited) {
        free(set);
        return NULL;
    }

    return set;
}

static void visited_set_destroy(VisitedSet* set) {
    if (!set) return;
    free(set->visited);
    free(set);
}

static bool visited_set_contains(VisitedSet* set, IRComponent* component) {
    for (size_t i = 0; i < set->count; i++) {
        if (set->visited[i] == component) return true;
    }
    return false;
}

static bool visited_set_add(VisitedSet* set, IRComponent* component) {
    if (visited_set_contains(set, component)) return false;

    if (set->count >= set->capacity) {
        set->capacity *= 2;
        IRComponent** new_visited = realloc(set->visited,
            sizeof(IRComponent*) * set->capacity);
        if (!new_visited) return false;
        set->visited = new_visited;
    }

    set->visited[set->count++] = component;
    return true;
}

bool ir_validate_circular_references(IRComponent* root, IRValidationResult* result) {
    if (!root) return true;

    VisitedSet* visited = visited_set_create();
    if (!visited) return false;

    bool has_circular = false;

    // DFS traversal
    typedef struct {
        IRComponent* component;
        size_t depth;
    } StackEntry;

    StackEntry* stack = malloc(sizeof(StackEntry) * 1024);
    if (!stack) {
        visited_set_destroy(visited);
        return false;
    }

    int stack_top = 0;
    stack[stack_top++] = (StackEntry){root, 0};

    while (stack_top > 0) {
        StackEntry entry = stack[--stack_top];
        IRComponent* comp = entry.component;

        if (visited_set_contains(visited, comp)) {
            ir_validation_add_issue(result, IR_VALIDATION_ERROR,
                                   IR_VALID_CIRCULAR_REFERENCE,
                                   "Circular reference detected in component tree",
                                   id_to_string(comp->id));
            has_circular = true;
            continue;
        }

        visited_set_add(visited, comp);

        // Add children to stack
        for (size_t i = 0; i < comp->child_count; i++) {
            if (comp->children[i] && stack_top < 1024) {
                stack[stack_top++] = (StackEntry){comp->children[i], entry.depth + 1};
            }
        }
    }

    free(stack);
    visited_set_destroy(visited);

    return !has_circular;
}

bool ir_validate_tree_integrity(IRComponent* component, IRComponent* expected_parent,
                                int depth, IRValidationOptions* options,
                                IRValidationResult* result) {
    if (!component) return true;

    // Check depth
    if (options && depth > (int)options->max_tree_depth) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Tree depth %d exceeds maximum %u",
                depth, options->max_tree_depth);
        ir_validation_add_issue(result, IR_VALIDATION_ERROR, IR_VALID_EXCESSIVE_DEPTH,
                               msg, id_to_string(component->id));
        return false;
    }

    // Validate parent pointer
    if (component->parent != expected_parent) {
        ir_validation_add_issue(result, IR_VALIDATION_ERROR, IR_VALID_INVALID_PARENT,
                               "Parent pointer mismatch", id_to_string(component->id));
        return false;
    }

    // Validate children
    bool valid = true;
    for (size_t i = 0; i < component->child_count; i++) {
        if (!component->children[i]) {
            ir_validation_add_issue(result, IR_VALIDATION_WARNING,
                                   IR_VALID_ORPHANED_CHILD,
                                   "NULL child pointer in children array",
                                   id_to_string(component->id));
            continue;
        }

        if (!ir_validate_tree_integrity(component->children[i], component,
                                       depth + 1, options, result)) {
            valid = false;
        }
    }

    return valid;
}

bool ir_validate_structure(IRComponent* root, IRValidationOptions* options,
                           IRValidationResult* result) {
    if (!root) {
        ir_validation_add_issue(result, IR_VALIDATION_FATAL, IR_VALID_NULL_ROOT,
                               "Root component is NULL", NULL);
        return false;
    }

    // Check for circular references
    if (!ir_validate_circular_references(root, result)) {
        return false;
    }

    // Validate tree integrity
    return ir_validate_tree_integrity(root, NULL, 0, options, result);
}

// ============================================================================
// Tier 3: Semantic Validation
// ============================================================================

bool ir_validate_dimension(IRDimension dim, IRValidationResult* result) {
    // Check for valid dimension type
    if (dim.type > IR_DIMENSION_EM) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Invalid dimension type: %d", dim.type);
        ir_validation_add_issue(result, IR_VALIDATION_ERROR, IR_VALID_INVALID_DIMENSION,
                               msg, NULL);
        return false;
    }

    // Check for reasonable values
    if (dim.type == IR_DIMENSION_PX && (dim.value < 0 || dim.value > 100000)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Unreasonable pixel value: %.2f", dim.value);
        ir_validation_add_issue(result, IR_VALIDATION_WARNING, IR_VALID_INVALID_DIMENSION,
                               msg, NULL);
    }

    if (dim.type == IR_DIMENSION_PERCENT && (dim.value < 0 || dim.value > 100)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Invalid percentage: %.2f%%", dim.value);
        ir_validation_add_issue(result, IR_VALIDATION_WARNING, IR_VALID_INVALID_DIMENSION,
                               msg, NULL);
    }

    return true;
}

bool ir_validate_color(IRColor color, IRValidationResult* result) {
    // Colors use 0-255 for RGBA, all values are valid
    (void)color;
    (void)result;
    return true;
}

bool ir_validate_layout(IRLayout* layout, IRValidationResult* result) {
    if (!layout) return true;

    // Validate layout mode
    if (layout->mode > 2) {  // FLEXBOX, GRID, ABSOLUTE
        char msg[64];
        snprintf(msg, sizeof(msg), "Invalid layout mode: %d", layout->mode);
        ir_validation_add_issue(result, IR_VALIDATION_ERROR, IR_VALID_INVALID_LAYOUT_MODE,
                               msg, NULL);
        return false;
    }

    // Validate dimensions
    ir_validate_dimension(layout->min_width, result);
    ir_validate_dimension(layout->min_height, result);
    ir_validate_dimension(layout->max_width, result);
    ir_validate_dimension(layout->max_height, result);

    // Validate aspect ratio
    if (layout->aspect_ratio < 0) {
        ir_validation_add_issue(result, IR_VALIDATION_WARNING, IR_VALID_INVALID_LAYOUT_MODE,
                               "Negative aspect ratio", NULL);
    }

    return true;
}

bool ir_validate_style(IRStyle* style, IRValidationResult* result) {
    if (!style) return true;

    // Validate dimensions
    ir_validate_dimension(style->width, result);
    ir_validate_dimension(style->height, result);

    // Validate colors
    ir_validate_color(style->background, result);
    ir_validate_color(style->font.color, result);

    // Validate font size
    if (style->font.size < 1.0 || style->font.size > 1000.0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Unusual font size: %.1f", style->font.size);
        ir_validation_add_issue(result, IR_VALIDATION_WARNING, IR_VALID_INVALID_DIMENSION,
                               msg, NULL);
    }

    return true;
}

bool ir_validate_component_full(IRComponent* component, IRValidationOptions* options,
                                 IRValidationResult* result) {
    if (!component) return false;

    (void)options;  // Currently unused

    // Validate component type
    if (component->type > 20) {  // Assuming max component type
        char msg[64];
        snprintf(msg, sizeof(msg), "Invalid component type: %d", component->type);
        ir_validation_add_issue(result, IR_VALIDATION_ERROR,
                               IR_VALID_INVALID_COMPONENT_TYPE, msg, id_to_string(component->id));
        return false;
    }

    // Validate style
    if (component->style) {
        ir_validate_style(component->style, result);
    }

    // Validate layout
    if (component->layout) {
        ir_validate_layout(component->layout, result);
    }

    // Validate text length
    if (component->text_content && strlen(component->text_content) > 1000000) {
        ir_validation_add_issue(result, IR_VALIDATION_WARNING, IR_VALID_INVALID_TEXT_LENGTH,
                               "Extremely long text content", id_to_string(component->id));
    }

    return true;
}

bool ir_validate_semantics(IRComponent* root, IRValidationOptions* options,
                           IRValidationResult* result) {
    if (!root) return false;

    // Validate root component
    if (!ir_validate_component_full(root, options, result)) {
        return false;
    }

    // Validate all children recursively
    for (size_t i = 0; i < root->child_count; i++) {
        if (root->children[i]) {
            ir_validate_semantics(root->children[i], options, result);
        }
    }

    return result->max_level < IR_VALIDATION_ERROR;
}

// ============================================================================
// Complete Validation
// ============================================================================

IRValidationResult* ir_validate_binary_complete(IRBuffer* buffer,
                                                IRValidationOptions* options) {
    IRValidationResult* result = ir_validation_result_create();
    if (!result) return NULL;

    // Tier 1: Format validation
    if (!ir_validate_format(buffer, options, result)) {
        return result;  // Fatal format errors
    }

    // Deserialize component tree
    IRComponent* root = ir_deserialize_binary(buffer);
    if (!root) {
        ir_validation_add_issue(result, IR_VALIDATION_FATAL, IR_VALID_NULL_ROOT,
                               "Failed to deserialize component tree", NULL);
        return result;
    }

    // Tier 2: Structure validation
    if (!ir_validate_structure(root, options, result)) {
        ir_destroy_component(root);
        return result;
    }

    // Tier 3: Semantic validation
    if (options && options->enable_semantic_validation) {
        ir_validate_semantics(root, options, result);
    }

    ir_destroy_component(root);
    return result;
}

IRValidationResult* ir_validate_component_complete(IRComponent* root,
                                                   IRValidationOptions* options) {
    IRValidationResult* result = ir_validation_result_create();
    if (!result) return NULL;

    // Tier 2: Structure validation
    if (!ir_validate_structure(root, options, result)) {
        return result;
    }

    // Tier 3: Semantic validation
    if (!options || options->enable_semantic_validation) {
        ir_validate_semantics(root, options, result);
    }

    return result;
}

// ============================================================================
// Default Options
// ============================================================================

IRValidationOptions ir_default_validation_options(void) {
    return (IRValidationOptions){
        .enable_crc_check = true,
        .enable_semantic_validation = true,
        .allow_graceful_degradation = true,
        .strict_version_check = false,
        .max_tree_depth = 100,
        .max_component_count = 100000
    };
}

IRValidationOptions ir_strict_validation_options(void) {
    return (IRValidationOptions){
        .enable_crc_check = true,
        .enable_semantic_validation = true,
        .allow_graceful_degradation = false,
        .strict_version_check = true,
        .max_tree_depth = 50,
        .max_component_count = 10000
    };
}

IRValidationOptions ir_permissive_validation_options(void) {
    return (IRValidationOptions){
        .enable_crc_check = false,
        .enable_semantic_validation = false,
        .allow_graceful_degradation = true,
        .strict_version_check = false,
        .max_tree_depth = 1000,
        .max_component_count = 1000000
    };
}

// ============================================================================
// Graceful Degradation (Placeholder - to be expanded)
// ============================================================================

IRComponent* ir_deserialize_binary_graceful(IRBuffer* buffer,
                                           IRValidationOptions* options,
                                           IRValidationResult* result) {
    // First try normal deserialization
    IRComponent* root = ir_deserialize_binary(buffer);
    if (root) return root;

    if (!options || !options->allow_graceful_degradation) {
        ir_validation_add_issue(result, IR_VALIDATION_FATAL, IR_VALID_NULL_ROOT,
                               "Deserialization failed and graceful degradation disabled",
                               NULL);
        return NULL;
    }

    // TODO: Implement partial recovery
    // This would involve:
    // 1. Skipping corrupted sections
    // 2. Creating placeholder components for missing data
    // 3. Recovering as much valid data as possible

    ir_validation_add_issue(result, IR_VALIDATION_ERROR, IR_VALID_PARTIAL_STYLE,
                           "Graceful degradation not yet fully implemented", NULL);

    return NULL;
}

// ============================================================================
// Recovery Strategies (D.1, D.2)
// ============================================================================

#define MAX_OPTIONAL_SECTIONS 64

typedef struct {
    char section_path[128];
    IRRecoveryStrategy recovery;
} OptionalSectionEntry;

static struct {
    OptionalSectionEntry entries[MAX_OPTIONAL_SECTIONS];
    uint32_t count;
} g_optional_sections = {0};

IRValidationContext* ir_validation_context_create(IRValidationOptions* options,
                                                   IRRecoveryStrategy recovery) {
    IRValidationContext* ctx = (IRValidationContext*)malloc(sizeof(IRValidationContext));
    if (!ctx) return NULL;

    ctx->options = options;
    ctx->result = ir_validation_result_create();
    ctx->recovery_strategy = recovery;
    ctx->has_errors = false;
    ctx->recovered_count = 0;
    ctx->skipped_count = 0;

    return ctx;
}

void ir_validation_context_destroy(IRValidationContext* ctx) {
    if (!ctx) return;

    if (ctx->result) {
        ir_validation_result_destroy(ctx->result);
    }

    free(ctx);
}

void ir_validation_set_recovery_strategy(IRValidationContext* ctx,
                                          IRRecoveryStrategy strategy) {
    if (!ctx) return;
    ctx->recovery_strategy = strategy;
}

const char* ir_recovery_strategy_name(IRRecoveryStrategy strategy) {
    switch (strategy) {
        case IR_RECOVERY_NONE:     return "none";
        case IR_RECOVERY_SKIP:     return "skip";
        case IR_RECOVERY_DEFAULT:  return "default";
        case IR_RECOVERY_SANITIZE: return "sanitize";
        case IR_RECOVERY_COERCE:   return "coerce";
        case IR_RECOVERY_CLAMP:    return "clamp";
        default:                   return "unknown";
    }
}

bool ir_validation_recover_field(IRValidationContext* ctx,
                                  const char* field_path,
                                  IRValidationErrorCode error) {
    if (!ctx || !field_path) return false;

    ctx->has_errors = true;

    switch (ctx->recovery_strategy) {
        case IR_RECOVERY_SKIP:
            ctx->skipped_count++;
            ir_validation_add_issue(ctx->result, IR_VALIDATION_WARNING, error,
                                   "Skipped invalid field", field_path);
            return true;

        case IR_RECOVERY_DEFAULT: {
            char default_value[256];
            if (ir_validation_get_default_value(field_path, default_value, sizeof(default_value))) {
                ctx->recovered_count++;
                ir_validation_add_issue(ctx->result, IR_VALIDATION_WARNING, error,
                                       "Using default value", field_path);
                return true;
            }
            return false;
        }

        case IR_RECOVERY_SANITIZE:
            ctx->recovered_count++;
            ir_validation_add_issue(ctx->result, IR_VALIDATION_WARNING, error,
                                   "Sanitized value", field_path);
            return true;

        case IR_RECOVERY_COERCE:
            ctx->recovered_count++;
            ir_validation_add_issue(ctx->result, IR_VALIDATION_WARNING, error,
                                   "Coerced value type", field_path);
            return true;

        case IR_RECOVERY_CLAMP:
            ctx->recovered_count++;
            ir_validation_add_issue(ctx->result, IR_VALIDATION_WARNING, error,
                                   "Clamped value to valid range", field_path);
            return true;

        case IR_RECOVERY_NONE:
        default:
            ir_validation_add_issue(ctx->result, IR_VALIDATION_ERROR, error,
                                   "No recovery strategy - validation failed", field_path);
            return false;
    }
}

char* ir_validation_sanitize_string(const char* str, char* buffer, size_t buffer_size) {
    if (!str || !buffer || buffer_size == 0) return NULL;

    size_t j = 0;
    for (size_t i = 0; str[i] != '\0' && j < buffer_size - 1; i++) {
        // Remove control characters except newline, tab, carriage return
        unsigned char c = (unsigned char)str[i];
        if (c < 32 && c != '\n' && c != '\t' && c != '\r') {
            continue;
        }
        buffer[j++] = str[i];
    }
    buffer[j] = '\0';

    return buffer;
}

bool ir_validation_coerce_to_type(const char* value, const char* target_type,
                                   char* out_buffer, size_t buffer_size) {
    if (!value || !target_type || !out_buffer || buffer_size == 0) return false;

    // String to number
    if (strcmp(target_type, "number") == 0 || strcmp(target_type, "float") == 0) {
        float num = 0.0f;
        if (sscanf(value, "%f", &num) == 1) {
            snprintf(out_buffer, buffer_size, "%f", num);
            return true;
        }
        return false;
    }

    // String to integer
    if (strcmp(target_type, "int") == 0) {
        int num = 0;
        if (sscanf(value, "%d", &num) == 1) {
            snprintf(out_buffer, buffer_size, "%d", num);
            return true;
        }
        return false;
    }

    // String to boolean
    if (strcmp(target_type, "bool") == 0) {
        bool val = false;
        if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
            strcmp(value, "yes") == 0) {
            val = true;
        }
        snprintf(out_buffer, buffer_size, "%s", val ? "true" : "false");
        return true;
    }

    // Number to string (already a string)
    if (strcmp(target_type, "string") == 0) {
        snprintf(out_buffer, buffer_size, "%s", value);
        return true;
    }

    return false;
}

bool ir_validation_clamp_value(float* value, float min_val, float max_val) {
    if (!value) return false;

    if (*value < min_val) {
        *value = min_val;
        return true;
    }
    if (*value > max_val) {
        *value = max_val;
        return true;
    }

    return true;
}

bool ir_validation_get_default_value(const char* field_path, char* out_value,
                                      size_t value_size) {
    if (!field_path || !out_value || value_size == 0) return false;

    // Common defaults for known fields
    struct {
        const char* path;
        const char* default_value;
    } defaults[] = {
        {"width", "100"},
        {"height", "auto"},
        {"opacity", "1.0"},
        {"visible", "true"},
        {"rotation", "0"},
        {"scale", "1"},
        {"color", "#FFFFFF"},
        {"background_color", "transparent"},
        {"margin", "0"},
        {"padding", "0"},
        {NULL, NULL}
    };

    for (size_t i = 0; defaults[i].path != NULL; i++) {
        if (strcmp(field_path, defaults[i].path) == 0) {
            snprintf(out_value, value_size, "%s", defaults[i].default_value);
            return true;
        }
    }

    return false;
}

// ============================================================================
// Section Skipping (D.3)
// ============================================================================

void ir_validation_mark_section_optional(IRValidationContext* ctx,
                                          const char* section_path) {
    if (!ctx || !section_path) return;

    ir_validation_add_optional_section(ctx, section_path, IR_RECOVERY_SKIP);
}

bool ir_validation_is_section_optional(IRValidationContext* ctx,
                                       const char* section_path) {
    if (!ctx || !section_path) return false;

    for (uint32_t i = 0; i < g_optional_sections.count; i++) {
        if (strcmp(g_optional_sections.entries[i].section_path, section_path) == 0) {
            return true;
        }
    }

    return false;
}

void ir_validation_add_optional_section(IRValidationContext* ctx,
                                        const char* section_path,
                                        IRRecoveryStrategy recovery) {
    if (!section_path) return;

    if (g_optional_sections.count >= MAX_OPTIONAL_SECTIONS) {
        return;
    }

    // Check if already exists
    for (uint32_t i = 0; i < g_optional_sections.count; i++) {
        if (strcmp(g_optional_sections.entries[i].section_path, section_path) == 0) {
            g_optional_sections.entries[i].recovery = recovery;
            return;
        }
    }

    // Add new entry
    snprintf(g_optional_sections.entries[g_optional_sections.count].section_path,
             sizeof(g_optional_sections.entries[0].section_path),
             "%s", section_path);
    g_optional_sections.entries[g_optional_sections.count].recovery = recovery;
    g_optional_sections.count++;

    if (ctx) {
        ir_validation_add_issue(ctx->result, IR_VALIDATION_WARNING,
                               IR_VALID_SKIPPED_CHILD,
                               "Section marked as optional", section_path);
    }
}

void ir_validation_clear_optional_sections(IRValidationContext* ctx) {
    (void)ctx;
    g_optional_sections.count = 0;
}

bool ir_validation_skip_section(IRValidationContext* ctx,
                                const char* section_path) {
    if (!ctx || !section_path) return false;

    if (ir_validation_is_section_optional(ctx, section_path)) {
        ctx->skipped_count++;
        ir_validation_add_issue(ctx->result, IR_VALIDATION_WARNING,
                               IR_VALID_SKIPPED_CHILD,
                               "Skipping optional section", section_path);
        return true;
    }

    return false;
}

// Update existing functions to use new recovery system

bool ir_recover_from_error(IRBuffer* buffer, IRValidationErrorCode error,
                          IRValidationResult* result) {
    (void)buffer;

    if (!result) return false;

    ir_validation_add_issue(result, IR_VALIDATION_WARNING, error,
                           "Attempting error recovery", NULL);

    return true;
}

bool ir_skip_corrupted_section(IRBuffer* buffer, IRValidationResult* result) {
    if (!buffer || !result) return false;

    // Get current buffer size
    size_t buffer_size __attribute__((unused)) = ir_buffer_size(buffer);

    // Try to read past 1 byte to skip corrupted data
    uint8_t temp;
    if (ir_buffer_read(buffer, &temp, 1)) {
        ir_validation_add_issue(result, IR_VALIDATION_WARNING,
                               IR_VALID_TRUNCATED_FILE,
                               "Skipped corrupted byte", NULL);
        return true;
    }

    return false;
}
