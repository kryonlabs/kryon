/**
 * Tcl to KIR Converter
 * Converts Tcl AST to KIR JSON format
 */

#ifndef TCL_TO_IR_H
#define TCL_TO_IR_H

#include "tcl_ast.h"
#include "../../../third_party/cJSON/cJSON.h"

/* ============================================================================
 * Main Conversion API
 * ============================================================================ */

/**
 * Convert Tcl AST to KIR JSON
 *
 * @param ast Parsed Tcl AST
 * @return cJSON* KIR JSON object (caller must free), or NULL on error
 */
cJSON* tcl_ast_to_kir(TclAST* ast);

/**
 * Parse Tcl source directly to KIR (combines parsing + conversion)
 *
 * @param tcl_source Tcl/Tk source code
 * @return cJSON* KIR JSON object (caller must free), or NULL on error
 */
cJSON* tcl_parse_to_kir(const char* tcl_source);

/* ============================================================================
 * Conversion Options
 * ============================================================================ */

typedef struct {
    bool include_metadata;       // Add metadata (source file, line numbers, etc.)
    bool preserve_comments;      // Preserve comments in KIR metadata
    bool infer_layout;           // Infer layout from pack/grid commands
    bool extract_handlers;       // Extract event handlers to logic_block
} TclToKirOptions;

/**
 * Convert with options
 *
 * @param ast Parsed Tcl AST
 * @param options Conversion options
 * @return cJSON* KIR JSON object, or NULL on error
 */
cJSON* tcl_ast_to_kir_ex(TclAST* ast, const TclToKirOptions* options);

#endif /* TCL_TO_IR_H */
