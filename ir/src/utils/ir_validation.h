#ifndef IR_VALIDATION_H
#define IR_VALIDATION_H

#include "../../include/ir_core.h"
#include "../../include/ir_serialization.h"
#include <stdbool.h>
#include <stdint.h>

// Validation Result Types
typedef enum {
    IR_VALIDATION_OK = 0,
    IR_VALIDATION_WARNING = 1,
    IR_VALIDATION_ERROR = 2,
    IR_VALIDATION_FATAL = 3
} IRValidationLevel;

// Validation Error Codes
typedef enum {
    IR_VALID_SUCCESS = 0,

    // Format Validation Errors (100-199)
    IR_VALID_INVALID_MAGIC = 100,
    IR_VALID_UNSUPPORTED_VERSION = 101,
    IR_VALID_ENDIANNESS_MISMATCH = 102,
    IR_VALID_CRC_FAILURE = 103,
    IR_VALID_TRUNCATED_FILE = 104,
    IR_VALID_BUFFER_OVERFLOW = 105,

    // Structure Validation Errors (200-299)
    IR_VALID_NULL_ROOT = 200,
    IR_VALID_CIRCULAR_REFERENCE = 201,
    IR_VALID_INVALID_PARENT = 202,
    IR_VALID_ORPHANED_CHILD = 203,
    IR_VALID_EXCESSIVE_DEPTH = 204,
    IR_VALID_INVALID_CHILD_COUNT = 205,
    IR_VALID_MISSING_REQUIRED_CHILD = 206,

    // Semantic Validation Errors (300-399)
    IR_VALID_INVALID_COMPONENT_TYPE = 300,
    IR_VALID_INVALID_DIMENSION = 301,
    IR_VALID_INVALID_COLOR = 302,
    IR_VALID_INVALID_LAYOUT_MODE = 303,
    IR_VALID_INVALID_ANIMATION = 304,
    IR_VALID_INCOMPATIBLE_PROPERTIES = 305,
    IR_VALID_INVALID_EVENT_TYPE = 306,
    IR_VALID_INVALID_BREAKPOINT = 307,
    IR_VALID_INVALID_GRID_TRACK = 308,
    IR_VALID_INVALID_FLEXBOX = 309,
    IR_VALID_INVALID_TEXT_LENGTH = 310,

    // Graceful Degradation Warnings (400-499)
    IR_VALID_PARTIAL_STYLE = 400,
    IR_VALID_PARTIAL_LAYOUT = 401,
    IR_VALID_SKIPPED_ANIMATION = 402,
    IR_VALID_SKIPPED_CHILD = 403,
    IR_VALID_DOWNGRADED_VERSION = 404
} IRValidationErrorCode;

// Validation Issue (for reporting)
typedef struct {
    IRValidationLevel level;
    IRValidationErrorCode code;
    char message[256];
    const char* component_id;  // NULL if not component-specific
    int line_number;           // For file-based validation
} IRValidationIssue;

// Validation Result
typedef struct {
    bool passed;
    IRValidationLevel max_level;
    IRValidationIssue* issues;
    size_t issue_count;
    size_t issue_capacity;
} IRValidationResult;

// Validation Options
typedef struct {
    bool enable_crc_check;
    bool enable_semantic_validation;
    bool allow_graceful_degradation;
    bool strict_version_check;
    uint32_t max_tree_depth;
    uint32_t max_component_count;
} IRValidationOptions;

// ============================================================================
// Recovery Strategies (D.1, D.2)
// ============================================================================

/**
 * Recovery strategy for validation errors
 */
typedef enum {
    IR_RECOVERY_NONE,        // Fail on error (default)
    IR_RECOVERY_SKIP,        // Skip the invalid element/section
    IR_RECOVERY_DEFAULT,     // Use default value
    IR_RECOVERY_SANITIZE,    // Clean/sanitize the value
    IR_RECOVERY_COERCE,      // Type coercion
    IR_RECOVERY_CLAMP        // Clamp to valid range
} IRRecoveryStrategy;

/**
 * Validation context with recovery support
 */
typedef struct {
    IRValidationOptions* options;
    IRValidationResult* result;
    IRRecoveryStrategy recovery_strategy;
    bool has_errors;
    uint32_t recovered_count;
    uint32_t skipped_count;
} IRValidationContext;

/**
 * Create a validation context with recovery support
 */
IRValidationContext* ir_validation_context_create(IRValidationOptions* options,
                                                   IRRecoveryStrategy recovery);

/**
 * Destroy validation context
 */
void ir_validation_context_destroy(IRValidationContext* ctx);

/**
 * Set recovery strategy
 */
void ir_validation_set_recovery_strategy(IRValidationContext* ctx,
                                          IRRecoveryStrategy strategy);

/**
 * Get recovery strategy name
 */
const char* ir_recovery_strategy_name(IRRecoveryStrategy strategy);

/**
 * Attempt to recover a field based on recovery strategy
 * @return true if recovery was successful, false otherwise
 */
bool ir_validation_recover_field(IRValidationContext* ctx,
                                  const char* field_path,
                                  IRValidationErrorCode error);

/**
 * Sanitize a string value (remove invalid characters)
 */
char* ir_validation_sanitize_string(const char* str, char* buffer, size_t buffer_size);

/**
 * Coerce value to type
 */
bool ir_validation_coerce_to_type(const char* value, const char* target_type,
                                   char* out_buffer, size_t buffer_size);

/**
 * Clamp numeric value to range
 */
bool ir_validation_clamp_value(float* value, float min_val, float max_val);

/**
 * Get default value for field path
 */
bool ir_validation_get_default_value(const char* field_path, char* out_value,
                                      size_t value_size);

// ============================================================================

// CRC32 Functions
uint32_t ir_crc32(const uint8_t* data, size_t length);
uint32_t ir_crc32_buffer(IRBuffer* buffer);
bool ir_buffer_write_crc32(IRBuffer* buffer);
bool ir_buffer_verify_crc32(IRBuffer* buffer);

// Validation Result Management
IRValidationResult* ir_validation_result_create(void);
void ir_validation_result_destroy(IRValidationResult* result);
void ir_validation_add_issue(IRValidationResult* result, IRValidationLevel level,
                             IRValidationErrorCode code, const char* message,
                             const char* component_id);
void ir_validation_print_result(IRValidationResult* result);
const char* ir_validation_error_string(IRValidationErrorCode code);

// 3-Tier Validation Functions

// Tier 1: Format Validation
bool ir_validate_format(IRBuffer* buffer, IRValidationOptions* options,
                        IRValidationResult* result);
bool ir_validate_magic_number(IRBuffer* buffer, IRValidationResult* result);
bool ir_validate_version(IRBuffer* buffer, IRValidationOptions* options,
                         IRValidationResult* result);
bool ir_validate_endianness(IRBuffer* buffer, IRValidationResult* result);

// Tier 2: Structure Validation
bool ir_validate_structure(IRComponent* root, IRValidationOptions* options,
                           IRValidationResult* result);
bool ir_validate_tree_integrity(IRComponent* component, IRComponent* expected_parent,
                               int depth, IRValidationOptions* options,
                               IRValidationResult* result);
bool ir_validate_circular_references(IRComponent* root, IRValidationResult* result);

// Tier 3: Semantic Validation
bool ir_validate_semantics(IRComponent* root, IRValidationOptions* options,
                           IRValidationResult* result);
bool ir_validate_component_full(IRComponent* component, IRValidationOptions* options,
                                 IRValidationResult* result);
bool ir_validate_style(IRStyle* style, IRValidationResult* result);
bool ir_validate_layout(IRLayout* layout, IRValidationResult* result);
bool ir_validate_dimension(IRDimension dim, IRValidationResult* result);
bool ir_validate_color(IRColor color, IRValidationResult* result);

// Complete Validation (All Tiers)
IRValidationResult* ir_validate_binary_complete(IRBuffer* buffer,
                                                IRValidationOptions* options);
IRValidationResult* ir_validate_component_complete(IRComponent* root,
                                                   IRValidationOptions* options);

// Graceful Degradation Functions
IRComponent* ir_deserialize_binary_graceful(IRBuffer* buffer,
                                           IRValidationOptions* options,
                                           IRValidationResult* result);
bool ir_recover_from_error(IRBuffer* buffer, IRValidationErrorCode error,
                          IRValidationResult* result);
bool ir_skip_corrupted_section(IRBuffer* buffer, IRValidationResult* result);

// Default Options
IRValidationOptions ir_default_validation_options(void);
IRValidationOptions ir_strict_validation_options(void);
IRValidationOptions ir_permissive_validation_options(void);

// ============================================================================
// Section Skipping (D.3)
// ============================================================================

/**
 * Optional section for validation
 */
typedef struct {
    const char* section_path;
    bool optional;
    IRRecoveryStrategy recovery;
} IRValidationSection;

/**
 * Mark a section as optional during validation
 * @param ctx Validation context
 * @param section_path Path to section (e.g., "root.animations")
 */
void ir_validation_mark_section_optional(IRValidationContext* ctx,
                                          const char* section_path);

/**
 * Check if a section is marked as optional
 * @return true if optional
 */
bool ir_validation_is_section_optional(IRValidationContext* ctx,
                                       const char* section_path);

/**
 * Add optional section to validation context
 */
void ir_validation_add_optional_section(IRValidationContext* ctx,
                                        const char* section_path,
                                        IRRecoveryStrategy recovery);

/**
 * Clear all optional sections
 */
void ir_validation_clear_optional_sections(IRValidationContext* ctx);

/**
 * Skip validation of a section
 * @return true if section was skipped
 */
bool ir_validation_skip_section(IRValidationContext* ctx,
                                const char* section_path);

#endif // IR_VALIDATION_H
