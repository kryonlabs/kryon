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
    KRYON_RUNTIME_PROP_ARRAY
} KryonRuntimePropertyType;

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