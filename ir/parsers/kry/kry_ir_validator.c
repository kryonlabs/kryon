#include "kry_ir_validator.h"
#include "kry_parser.h"
#include "../../include/ir_core.h"

typedef struct {
    KryParser* parser;
    IRComponent* root;
    uint32_t component_count;
    uint32_t max_depth;
} KryIRValidator;

static bool validate_component(KryIRValidator* v, IRComponent* comp, uint32_t depth);
static bool validate_properties(KryIRValidator* v, IRComponent* comp);

bool kry_validate_ir(KryParser* parser, IRComponent* root) {
    if (!root) {
        if (parser) {
            kry_parser_add_error(parser, KRY_ERROR_ERROR,
                                KRY_ERROR_VALIDATION,
                                "Root component is NULL");
        }
        return false;
    }

    KryIRValidator validator = {
        .parser = parser,
        .root = root,
        .component_count = 0,
        .max_depth = 100
    };

    return validate_component(&validator, root, 0);
}

static bool validate_component(KryIRValidator* v, IRComponent* comp, uint32_t depth) {
    if (depth > v->max_depth) {
        if (v->parser) {
            kry_parser_add_error_fmt(v->parser, KRY_ERROR_ERROR,
                                    KRY_ERROR_VALIDATION,
                                    "Component tree exceeds maximum depth of %u",
                                    v->max_depth);
        }
        return false;
    }

    v->component_count++;

    // Validate component type
    if (comp->type == 0) {
        if (v->parser) {
            kry_parser_add_error_fmt(v->parser, KRY_ERROR_WARNING,
                                    KRY_ERROR_VALIDATION,
                                    "Component has type 0 (may be uninitialized)");
        }
    }

    // Validate properties
    validate_properties(v, comp);

    // Validate children recursively
    for (uint32_t i = 0; i < comp->child_count; i++) {
        if (comp->children[i]) {
            validate_component(v, comp->children[i], depth + 1);
        }
    }

    return true;
}

static bool validate_properties(KryIRValidator* v, IRComponent* comp) {
    // Component-specific validation
    switch (comp->type) {
        case IR_COMPONENT_TEXT:
            if (!comp->text_content && !comp->text_expression) {
                if (v->parser) {
                    kry_parser_add_error(v->parser, KRY_ERROR_WARNING,
                                        KRY_ERROR_VALIDATION,
                                        "Text component has no content");
                }
            }
            break;

        case IR_COMPONENT_BUTTON:
            if (!comp->text_content && !comp->events) {
                if (v->parser) {
                    kry_parser_add_error(v->parser, KRY_ERROR_WARNING,
                                        KRY_ERROR_VALIDATION,
                                        "Button has no text or events");
                }
            }
            break;

        // Can add more component types as needed
        default:
            break;
    }

    return true;
}
