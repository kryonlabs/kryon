#ifndef KRY_IR_VALIDATOR_H
#define KRY_IR_VALIDATOR_H

#include "kry_ast.h"
#include "../../include/ir_core.h"

// Validate IR component tree
// Returns true if valid (or has only warnings), false if fatal errors
bool kry_validate_ir(KryParser* parser, IRComponent* root);

#endif // KRY_IR_VALIDATOR_H
