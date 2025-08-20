/**
 * @file types.h
 * @brief Shared types for the Kryon engine
 */

#ifndef KRYON_INTERNAL_TYPES_H
#define KRYON_INTERNAL_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "events.h"

// =============================================================================
// BASIC TYPES
// =============================================================================

typedef struct {
    float x, y;
} KryonVec2;

typedef struct {
    float r, g, b, a;
} KryonVec4;

typedef KryonVec4 KryonColor;

typedef struct {
    float x, y, width, height;
} KryonRect;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonElement KryonElement;
typedef struct KryonProperty KryonProperty;
typedef struct KryonStyle KryonStyle;
typedef struct KryonComponentInstance KryonComponentInstance;
typedef struct ElementEventHandler ElementEventHandler;
typedef struct KryonState KryonState;
typedef struct KryonComponentDefinition KryonComponentDefinition;

// =============================================================================
// ENUMS
// =============================================================================

typedef enum {
    KRYON_ELEMENT_STATE_NORMAL,
    KRYON_ELEMENT_STATE_HOVERED,
    KRYON_ELEMENT_STATE_PRESSED,
    KRYON_ELEMENT_STATE_FOCUSED,
    KRYON_ELEMENT_STATE_DISABLED,
	KRYON_ELEMENT_STATE_CREATED,
	KRYON_ELEMENT_STATE_MOUNTING,
	KRYON_ELEMENT_STATE_MOUNTED,
	KRYON_ELEMENT_STATE_UPDATING,
	KRYON_ELEMENT_STATE_UNMOUNTING,
	KRYON_ELEMENT_STATE_UNMOUNTED,
	KRYON_ELEMENT_STATE_DESTROYED
} KryonElementState;

typedef enum {
    KRYON_RUNTIME_PROP_STRING = 0,
    KRYON_RUNTIME_PROP_INTEGER,
    KRYON_RUNTIME_PROP_FLOAT,
    KRYON_RUNTIME_PROP_BOOLEAN,
    KRYON_RUNTIME_PROP_COLOR,
    KRYON_RUNTIME_PROP_REFERENCE,
    KRYON_RUNTIME_PROP_EXPRESSION,
    KRYON_RUNTIME_PROP_FUNCTION,
    KRYON_RUNTIME_PROP_ARRAY,
    KRYON_RUNTIME_PROP_TEMPLATE,
    KRYON_RUNTIME_PROP_AST_EXPRESSION
} KryonRuntimePropertyType;

// =============================================================================
// EXPRESSION EVALUATION TYPES
// =============================================================================

typedef enum {
    KRYON_EXPR_VALUE_NUMBER,
    KRYON_EXPR_VALUE_STRING,
    KRYON_EXPR_VALUE_BOOLEAN,
    KRYON_EXPR_VALUE_VARIABLE_REF
} KryonExpressionValueType;

typedef struct {
    KryonExpressionValueType type;
    union {
        double number_value;
        char *string_value;
        bool boolean_value;
        char *variable_name;
    } data;
} KryonExpressionValue;

typedef enum {
    KRYON_EXPR_OP_ADD,
    KRYON_EXPR_OP_SUBTRACT,
    KRYON_EXPR_OP_MULTIPLY,
    KRYON_EXPR_OP_DIVIDE,
    KRYON_EXPR_OP_MODULO,
    KRYON_EXPR_OP_EQUAL,
    KRYON_EXPR_OP_NOT_EQUAL,
    KRYON_EXPR_OP_LESS_THAN,
    KRYON_EXPR_OP_GREATER_THAN,
    KRYON_EXPR_OP_LESS_EQUAL,
    KRYON_EXPR_OP_GREATER_EQUAL,
    KRYON_EXPR_OP_LOGICAL_AND,
    KRYON_EXPR_OP_LOGICAL_OR,
    KRYON_EXPR_OP_NEGATE,
    KRYON_EXPR_OP_NOT
} KryonExpressionOperator;

typedef struct KryonExpressionNode {
    enum {
        KRYON_EXPR_NODE_VALUE,
        KRYON_EXPR_NODE_BINARY_OP,
        KRYON_EXPR_NODE_UNARY_OP,
        KRYON_EXPR_NODE_TERNARY_OP
    } type;
    
    union {
        KryonExpressionValue value;
        struct {
            KryonExpressionOperator operator;
            struct KryonExpressionNode *left;
            struct KryonExpressionNode *right;
        } binary_op;
        struct {
            KryonExpressionOperator operator;
            struct KryonExpressionNode *operand;
        } unary_op;
        struct {
            struct KryonExpressionNode *condition;
            struct KryonExpressionNode *true_value;
            struct KryonExpressionNode *false_value;
        } ternary_op;
    } data;
} KryonExpressionNode;

// Template segment types
typedef enum {
    KRYON_TEMPLATE_SEGMENT_LITERAL = 0,
    KRYON_TEMPLATE_SEGMENT_VARIABLE
} KryonTemplateSegmentType;

typedef enum {
    KRYON_STATE_NULL = 0,
    KRYON_STATE_BOOLEAN,
    KRYON_STATE_INTEGER,
    KRYON_STATE_FLOAT,
    KRYON_STATE_STRING,
    KRYON_STATE_OBJECT,
    KRYON_STATE_ARRAY
} KryonStateType;

// =============================================================================
// STRUCTS
// =============================================================================

typedef struct {
    char* text;
    char* value;
} KryonDropdownItem;

// Template segment structure
typedef struct {
    KryonTemplateSegmentType type;
    union {
        char *literal_text;      // For LITERAL segments
        char *variable_name;     // For VARIABLE segments  
    } data;
} KryonTemplateSegment;

struct KryonProperty {
    uint16_t id;
    char *name;
    KryonRuntimePropertyType type;
    union {
        char *string_value;
        int64_t int_value;
        double float_value;
        bool bool_value;
        uint32_t color_value;
        uint32_t ref_id;
        void *expression;
        void *function;
        struct {
            size_t count;
            char **values;
        } array_value;
        struct {
            size_t segment_count;
            KryonTemplateSegment *segments;
        } template_value;
        KryonExpressionNode *ast_expression;
    } value;
    bool is_bound;
    char *binding_path;
    void *binding_context;
};

struct KryonState {
    char *key;
    KryonStateType type;
    union {
        bool bool_value;
        int64_t int_value;
        double float_value;
        char *string_value;
        struct {
            KryonState **items;
            size_t count;
            size_t capacity;
        } array;
        struct {
            KryonState **properties;
            size_t count;
            size_t capacity;
        } object;
    } value;
    void **observers;
    size_t observer_count;
    size_t observer_capacity;
    KryonState *parent;
};

typedef struct {
    char *name;
    char *language;
    uint8_t *bytecode;
    size_t bytecode_size;
} KryonComponentFunction;

typedef struct {
    char *name;
    char *default_value;
    uint8_t type;
} KryonComponentStateVar;

struct KryonComponentDefinition {
    char *name;
    char **parameters;
    char **param_defaults;
    size_t parameter_count;
    KryonComponentStateVar *state_vars;
    size_t state_count;
    KryonComponentFunction *functions;
    size_t function_count;
    KryonElement *ui_template;
};

struct KryonComponentInstance {
    KryonComponentDefinition *definition;
    uint32_t instance_id;
    char **state_values;
    size_t state_count;
    char **param_values;
    size_t param_count;
    KryonElement *ui_root;
};

struct ElementEventHandler {
    KryonEventType type;
    KryonEventHandler handler;
    void *user_data;
    bool capture;
};

#endif // KRYON_INTERNAL_TYPES_H