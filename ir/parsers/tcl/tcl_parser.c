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

    for (int i = start_idx; i < arg_count; i++) {
        const char* arg = arguments[i];

        // Check if this is an option (starts with -)
        if (arg && arg[0] == '-') {
            const char* opt_name = arg;
            const char* opt_value = NULL;

            // Next token is the value
            if (i + 1 < arg_count) {
                opt_value = arguments[i + 1];
                i++;  // Skip value
            }

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

        // Check command type
        if (is_widget_creation(cmd->command_name)) {
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
