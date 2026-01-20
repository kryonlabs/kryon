#include "kry_ir_validator.h"
#include "kry_parser.h"

// Simple validation - just checks if root exists
bool kry_validate_ir(KryParser* parser, IRComponent* root) {
    if (!root) {
        if (parser) {
            kry_parser_add_error(parser, KRY_ERROR_ERROR,
                                KRY_ERROR_VALIDATION,
                                "Root component is NULL");
        }
        return false;
    }

    // Basic validation passed
    return true;
}
