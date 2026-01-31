/**
 * Tcl Parser Implementation
 * Parses Tcl/Tk source code into an Abstract Syntax Tree (AST)
 */

#define _POSIX_C_SOURCE 200809L

#include "tcl_parser.h"
#include "tcl_ast.h"
#include "tcl_widget_registry.h"
#include "../../../codegens/codegen_common.h"
#include "../../../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ============================================================================
 * Error Handling
 * ============================================================================ */

static char last_error_message[1024] = {0};
static TclParseError last_error_code = TCL_PARSE_OK;

void set_error(TclParseError code, const char* message) {
    last_error_code = code;
    snprintf(last_error_message, sizeof(last_error_message), "%s", message);
}

const char* tcl_parse_get_last_error(void) {
    return last_error_message;
}

TclParseError tcl_parse_get_last_error_code(void) {
    return last_error_code;
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

TclAST* tcl_ast_create(void) {
    TclAST* ast = calloc(1, sizeof(TclAST));
    return ast;
}

void tcl_node_free(TclNode* node) {
    if (!node) return;

    switch (node->node_type) {
        case TCL_STMT_COMMAND: {
            TclCommand* cmd = (TclCommand*)node;
            if (cmd->command_name) free(cmd->command_name);
            for (int i = 0; i < cmd->argument_count; i++) {
                free(cmd->arguments[i]);
            }
            free(cmd->arguments);
            break;
        }
        case TCL_STMT_WIDGET: {
            TclWidget* widget = (TclWidget*)node;
            if (widget->base.command_name) free(((TclCommand*)widget)->command_name);
            for (int i = 0; i < widget->base.argument_count; i++) {
                free(widget->base.arguments[i]);
            }
            free(widget->base.arguments);
            if (widget->widget_path) free(widget->widget_path);
            if (widget->options) cJSON_Delete(widget->options);
            if (widget->pack_command) free(widget->pack_command);
            if (widget->grid_command) free(widget->grid_command);
            if (widget->place_command) free(widget->place_command);
            break;
        }
        // ... other statement types
        default:
            break;
    }

    free(node);
}

void tcl_ast_free(TclAST* ast) {
    if (!ast) return;

    for (int i = 0; i < ast->statement_count; i++) {
        tcl_node_free((TclNode*)ast->statements[i]);
    }
    free(ast->statements);

    for (int i = 0; i < ast->widget_count; i++) {
        // Widgets are already freed via statements
    }
    free(ast->widgets);

    for (int i = 0; i < ast->pack_command_count; i++) {
        free(ast->pack_commands[i]);
    }
    free(ast->pack_commands);

    for (int i = 0; i < ast->grid_command_count; i++) {
        free(ast->grid_commands[i]);
    }
    free(ast->grid_commands);

    for (int i = 0; i < ast->place_command_count; i++) {
        free(ast->place_commands[i]);
    }
    free(ast->place_commands);

    for (int i = 0; i < ast->bind_command_count; i++) {
        free(ast->bind_commands[i]);
    }
    free(ast->bind_commands);

    for (int i = 0; i < ast->proc_count; i++) {
        free(ast->procs[i]);
    }
    free(ast->procs);

    free(ast);
}

/* ============================================================================
 * Factory Functions
 * ============================================================================ */

TclToken* tcl_token_create(TclTokenType type, const char* value, int line, int column) {
    TclToken* token = calloc(1, sizeof(TclToken));
    if (!token) return NULL;

    token->type = type;
    token->value = value ? strdup(value) : NULL;
    token->line = line;
    token->column = column;

    return token;
}

TclCommand* tcl_command_create(const char* command_name, int line) {
    TclCommand* cmd = calloc(1, sizeof(TclCommand));
    if (!cmd) return NULL;

    cmd->base.node_type = TCL_STMT_COMMAND;
    cmd->base.line = line;
    cmd->command_name = strdup(command_name);
    cmd->arguments = NULL;
    cmd->argument_count = 0;

    return cmd;
}

TclWidget* tcl_widget_create(const char* widget_type, const char* widget_path, int line) {
    TclWidget* widget = calloc(1, sizeof(TclWidget));
    if (!widget) return NULL;

    widget->base.base.node_type = TCL_STMT_WIDGET;
    widget->base.base.line = line;
    widget->base.command_name = strdup(widget_type);
    widget->widget_path = strdup(widget_path);
    widget->options = cJSON_CreateObject();
    widget->pack_command = NULL;
    widget->grid_command = NULL;
    widget->place_command = NULL;

    return widget;
}

/* ============================================================================
 * Parser Implementation
 * ============================================================================ */

/**
 * Parse a Tcl command from token stream
 * Returns NULL on error
 */
static TclCommand* parse_command(TclTokenStream* stream) {
    TclToken* first = tcl_token_stream_peek(stream, 0);
    if (!first || first->type == TCL_TOKEN_EOF) {
        return NULL;
    }

    // First token is the command name
    const char* cmd_name = NULL;
    if (first->type == TCL_TOKEN_WORD || first->type == TCL_TOKEN_STRING) {
        cmd_name = first->value;
    } else if (first->type == TCL_TOKEN_POUND) {
        // Skip comment
        tcl_token_stream_consume(stream);
        return NULL;
    } else {
        set_error(TCL_PARSE_ERROR_SYNTAX, "Expected command name");
        return NULL;
    }

    TclCommand* cmd = tcl_command_create(cmd_name, first->line);
    tcl_token_stream_consume(stream);

    // Parse arguments
    while (true) {
        TclToken* tok = tcl_token_stream_peek(stream, 0);
        if (!tok || tok->type == TCL_TOKEN_EOF || tok->type == TCL_TOKEN_SEMICOLON) {
            break;
        }

        if (tok->type == TCL_TOKEN_SEMICOLON) {
            tcl_token_stream_consume(stream);
            break;
        }

        // Stop at newline (command boundary in Tcl)
        // If the next token is on a different line, it's a new command
        if (tok->line > cmd->base.line) {
            break;
        }

        if (tok->type == TCL_TOKEN_WORD || tok->type == TCL_TOKEN_STRING ||
            tok->type == TCL_TOKEN_NUMBER || tok->type == TCL_TOKEN_DOLLAR ||
            tok->type == TCL_TOKEN_BRACKET) {

            // Add argument
            const char* arg_value = tok->value ? tok->value : "";
            cmd->argument_count++;
            cmd->arguments = realloc(cmd->arguments, cmd->argument_count * sizeof(char*));
            cmd->arguments[cmd->argument_count - 1] = strdup(arg_value);
            tcl_token_stream_consume(stream);
        } else {
            break;
        }
    }

    return cmd;
}

/**
 * Check if a command is a widget creation
 */
static bool is_widget_creation(const char* cmd_name) {
    return tcl_is_valid_widget_type(cmd_name);
}

/**
 * Parse widget options from command arguments
 * Format: -option value -option2 value2
 */
static cJSON* parse_widget_options(char** arguments, int arg_count, int start_idx) {
    cJSON* options = cJSON_CreateObject();
    if (!options) return NULL;

    bool debug = getenv("DEBUG_TCL_PARSER") != NULL;

    for (int i = start_idx; i < arg_count; i++) {
        const char* arg = arguments[i];
        if (debug) printf("    Parsing arg[%d] = '%s'\n", i, arg ? arg : "(null)");

        // Check if this is an option (starts with -)
        if (arg && arg[0] == '-') {
            const char* opt_name = arg;
            const char* opt_value = NULL;

            // Next token is the value
            if (i + 1 < arg_count) {
                opt_value = arguments[i + 1];
                i++;  // Skip value
            }

            if (debug) printf("      Option: '%s' = '%s'\n", opt_name, opt_value ? opt_value : "(null)");

            // Map to KIR property name
            const char* kir_prop = tcl_option_to_kir_property(opt_name);

            if (kir_prop && opt_value) {
                cJSON_AddStringToObject(options, kir_prop, opt_value);
            } else if (opt_value) {
                // Keep original option name if mapping not found
                cJSON_AddStringToObject(options, opt_name, opt_value);
            }
        }
    }

    return options;
}

/**
 * Parse a widget creation command
 */
static TclWidget* parse_widget_command(TclCommand* cmd) {
    if (!cmd || cmd->argument_count < 1) {
        return NULL;
    }

    // First argument is widget path
    const char* widget_path = cmd->arguments[0];
    if (!widget_path) {
        return NULL;
    }

    TclWidget* widget = tcl_widget_create(cmd->command_name, widget_path, cmd->base.line);

    // Parse options
    widget->options = parse_widget_options(cmd->arguments, cmd->argument_count, 1);

    return widget;
}

/**
 * Main parse function
 */
TclAST* tcl_parse(const char* source) {
    bool debug = false;  // Disable debug for now
    if (getenv("DEBUG_TCL_PARSER")) debug = true;
    if (!source) {
        set_error(TCL_PARSE_ERROR_SYNTAX, "NULL source");
        return NULL;
    }

    // Reset error state
    last_error_code = TCL_PARSE_OK;
    last_error_message[0] = '\0';

    TclAST* ast = tcl_ast_create();
    if (!ast) {
        set_error(TCL_PARSE_ERROR_MEMORY, "Failed to allocate AST");
        return NULL;
    }

    // Tokenize
    TclTokenStream* stream = tcl_lex(source);
    if (!stream) {
        tcl_ast_free(ast);
        set_error(TCL_PARSE_ERROR_LEXICAL, "Failed to tokenize source");
        return NULL;
    }

    // Parse statements
    while (true) {
        TclToken* tok = tcl_token_stream_peek(stream, 0);
        if (!tok || tok->type == TCL_TOKEN_EOF) {
            break;
        }

        TclCommand* cmd = parse_command(stream);
        if (!cmd) {
            continue;  // Skip comments, empty lines
        }

        if (debug) {
            printf("Command: '%s' (args: %d)\n", cmd->command_name, cmd->argument_count);
        }

        // Check command type
        if (is_widget_creation(cmd->command_name)) {
            if (debug) printf("  -> Recognized as widget!\n");
            // Widget creation
            TclWidget* widget = parse_widget_command(cmd);
            if (widget) {
                ast->widget_count++;
                ast->widgets = realloc(ast->widgets, ast->widget_count * sizeof(TclWidget*));
                ast->widgets[ast->widget_count - 1] = widget;
            }
        } else if (strcmp(cmd->command_name, "pack") == 0) {
            // Pack command - TODO
        } else if (strcmp(cmd->command_name, "grid") == 0) {
            // Grid command - TODO
        } else if (strcmp(cmd->command_name, "place") == 0) {
            // Place command - TODO
        } else if (strcmp(cmd->command_name, "bind") == 0) {
            // Bind command - TODO
        } else if (strcmp(cmd->command_name, "proc") == 0) {
            // Proc definition - TODO
        }

        // Add to statements
        ast->statement_count++;
        ast->statements = realloc(ast->statements, ast->statement_count * sizeof(TclCommand*));
        ast->statements[ast->statement_count - 1] = cmd;
    }

    tcl_token_stream_free(stream);

    return ast;
}

/* ============================================================================
 * External declarations for lexer
 * ============================================================================ */

extern TclTokenStream* tcl_lex(const char* source);
extern TclToken* tcl_token_stream_peek(TclTokenStream* stream, int offset);
extern TclToken* tcl_token_stream_consume(TclTokenStream* stream);
extern void tcl_token_stream_free(TclTokenStream* stream);

/* ============================================================================
 * Native IR API Wrapper Functions
 * ============================================================================ */

// Forward declarations
extern cJSON* tcl_parse_to_kir(const char* tcl_source);
extern IRComponent* ir_deserialize_json(const char* json_string);

/**
 * Parse Tcl/Tk source to IR component tree
 */
IRComponent* ir_tcl_parse(const char* source, size_t length) {
    (void)length;  // Length parameter for consistency with KRY API, but we use null-terminated strings

    // Parse to KIR JSON
    cJSON* kir_json = tcl_parse_to_kir(source);
    if (!kir_json) {
        return NULL;
    }

    // Convert to JSON string
    char* json_string = cJSON_Print(kir_json);
    cJSON_Delete(kir_json);

    if (!json_string) {
        return NULL;
    }

    // Deserialize to native IR
    IRComponent* root = ir_deserialize_json(json_string);
    free(json_string);

    return root;
}

/**
 * Convert Tcl/Tk source to KIR JSON string
 */
char* ir_tcl_to_kir(const char* source, size_t length) {
    (void)length;

    // Parse to KIR JSON
    cJSON* kir_json = tcl_parse_to_kir(source);
    if (!kir_json) {
        return NULL;
    }

    // Convert to JSON string
    char* json_string = cJSON_Print(kir_json);
    cJSON_Delete(kir_json);

    return json_string;
}

/**
 * Parse Tcl/Tk and report errors
 */
IRComponent* ir_tcl_parse_with_errors(const char* source, size_t length,
                                     char** error_message,
                                     uint32_t* error_line,
                                     uint32_t* error_column) {
    (void)length;

    // Set error output parameters to defaults
    if (error_message) *error_message = NULL;
    if (error_line) *error_line = 0;
    if (error_column) *error_column = 0;

    // Parse to KIR JSON
    cJSON* kir_json = tcl_parse_to_kir(source);
    if (!kir_json) {
        // Get error from parser
        if (error_message) {
            const char* err = tcl_parse_get_last_error();
            if (err) {
                *error_message = strdup(err);
            }
        }
        return NULL;
    }

    // Convert to JSON string
    char* json_string = cJSON_Print(kir_json);
    cJSON_Delete(kir_json);

    if (!json_string) {
        if (error_message) {
            *error_message = strdup("Failed to serialize KIR JSON");
        }
        return NULL;
    }

    // Deserialize to native IR
    IRComponent* root = ir_deserialize_json(json_string);
    if (!root) {
        if (error_message) {
            *error_message = strdup("Failed to deserialize KIR to native IR");
        }
    }

    free(json_string);
    return root;
}

/**
 * Get parser version string
 */
const char* ir_tcl_parser_version(void) {
    return "1.0.0";
}

/**
 * Convert Tcl/Tk file to KIR JSON string
 *
 * Convenience function that reads a file and converts to KIR JSON.
 * Useful for CLI tools.
 *
 * @param filepath Path to Tcl/Tk file
 * @return char* JSON string in KIR format (caller must free), or NULL on error
 */
char* ir_tcl_to_kir_file(const char* filepath) {
    if (!filepath) {
        return NULL;
    }

    // Read file
    FILE* f = fopen(filepath, "r");
    if (!f) {
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read content
    char* source = (char*)malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(source, 1, (size_t)size, f);
    source[bytes_read] = '\0';
    fclose(f);

    // Convert to KIR
    char* kir_json = ir_tcl_to_kir(source, bytes_read);
    free(source);

    return kir_json;
}
