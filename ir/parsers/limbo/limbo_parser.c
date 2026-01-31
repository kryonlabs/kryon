/**
 * Limbo Parser Implementation (Simplified)
 *
 * Basic parser for Limbo source code
 * Focuses on extracting GUI-related structures for KIR conversion
 */

#define _POSIX_C_SOURCE 200809L

#include "limbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Create parser
 */
LimboParser* limbo_parser_create(const char* source, size_t source_len) {
    if (!source) return NULL;

    LimboParser* parser = calloc(1, sizeof(LimboParser));
    if (!parser) return NULL;

    parser->lexer = limbo_lex_create(source, source_len);
    if (!parser->lexer) {
        free(parser);
        return NULL;
    }

    parser->has_error = false;
    parser->current_token = limbo_next_token(parser->lexer);

    return parser;
}

/**
 * Free parser
 */
void limbo_parser_free(LimboParser* parser) {
    if (parser) {
        if (parser->lexer) {
            limbo_lex_free(parser->lexer);
        }
        free(parser);
    }
}

/**
 * Get current token
 */
static LimboToken current_token(LimboParser* parser) {
    return parser->current_token;
}

/**
 * Check if current token is of type
 */
static bool check(LimboParser* parser, LimboTokenType type) {
    return current_token(parser).type == type;
}

/**
 * Advance to next token
 */
static LimboToken advance(LimboParser* parser) {
    LimboToken token = current_token(parser);
    parser->current_token = limbo_next_token(parser->lexer);
    return token;
}

/**
 * Consume token if it matches expected type, otherwise set error
 */
static bool consume(LimboParser* parser, LimboTokenType type, const char* error_msg) {
    if (check(parser, type)) {
        advance(parser);
        return true;
    }

    parser->has_error = true;
    snprintf(parser->error_message, sizeof(parser->error_message),
             "Error at line %d: %s (got %s)",
             parser->current_token.line,
             error_msg,
             limbo_token_name(parser->current_token.type));
    return false;
}

/**
 * Get last error message
 */
const char* limbo_parser_get_error(LimboParser* parser) {
    if (!parser || !parser->has_error) return NULL;
    return parser->error_message;
}

/**
 * Create module node
 */
LimboModule* limbo_ast_module_create(const char* name, LimboToken token) {
    LimboModule* module = calloc(1, sizeof(LimboModule));
    if (!module) return NULL;

    module->name = name ? strdup(name) : NULL;
    module->token = token;

    return module;
}

/**
 * Create function node
 */
LimboFunction* limbo_ast_function_create(const char* name, LimboToken token) {
    LimboFunction* func = calloc(1, sizeof(LimboFunction));
    if (!func) return NULL;

    func->name = name ? strdup(name) : NULL;
    func->token = token;

    return func;
}

/**
 * Create statement node
 */
LimboStatement* limbo_ast_statement_create(LimboNodeType type, LimboToken token) {
    LimboStatement* stmt = calloc(1, sizeof(LimboStatement));
    if (!stmt) return NULL;

    stmt->type = type;
    stmt->token = token;

    return stmt;
}

/**
 * Create expression node
 */
LimboExpression* limbo_ast_expression_create(LimboNodeType type, LimboToken token) {
    LimboExpression* expr = calloc(1, sizeof(LimboExpression));
    if (!expr) return NULL;

    expr->type = type;
    expr->token = token;

    return expr;
}

/**
 * Free expression
 */
void limbo_ast_expression_free(LimboExpression* expr) {
    if (!expr) return;

    switch (expr->type) {
        case NODE_BINARY_OP:
            limbo_ast_expression_free(expr->as.binary_op.left);
            limbo_ast_expression_free(expr->as.binary_op.right);
            break;

        case NODE_UNARY_OP:
            limbo_ast_expression_free(expr->as.unary_op.operand);
            break;

        case NODE_CALL:
            limbo_ast_expression_free(expr->as.call.function);
            for (size_t i = 0; i < expr->as.call.arg_count; i++) {
                limbo_ast_expression_free(expr->as.call.args[i]);
            }
            free(expr->as.call.args);
            break;

        case NODE_INDEX:
            limbo_ast_expression_free(expr->as.index.container);
            limbo_ast_expression_free(expr->as.index.index);
            break;

        case NODE_FIELD:
            limbo_ast_expression_free(expr->as.field.object);
            break;

        case NODE_LIST_LITERAL:
            for (size_t i = 0; i < expr->as.list_literal.element_count; i++) {
                limbo_ast_expression_free(expr->as.list_literal.elements[i]);
            }
            free(expr->as.list_literal.elements);
            break;

        default:
            break;
    }

    free(expr);
}

/**
 * Free statement
 */
void limbo_ast_statement_free(LimboStatement* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case NODE_BLOCK:
            for (size_t i = 0; i < stmt->as.block.statement_count; i++) {
                limbo_ast_statement_free(stmt->as.block.statements[i]);
            }
            free(stmt->as.block.statements);
            break;

        case NODE_IF:
            limbo_ast_expression_free(stmt->as.if_stmt.condition);
            limbo_ast_statement_free(stmt->as.if_stmt.then_block);
            limbo_ast_statement_free(stmt->as.if_stmt.else_block);
            break;

        case NODE_WHILE:
            limbo_ast_expression_free(stmt->as.while_stmt.condition);
            limbo_ast_statement_free(stmt->as.while_stmt.body);
            break;

        case NODE_DO_WHILE:
            limbo_ast_statement_free(stmt->as.do_while_stmt.body);
            limbo_ast_expression_free(stmt->as.do_while_stmt.condition);
            break;

        case NODE_FOR:
            limbo_ast_statement_free(stmt->as.for_stmt.init);
            limbo_ast_expression_free(stmt->as.for_stmt.condition);
            limbo_ast_statement_free(stmt->as.for_stmt.post);
            limbo_ast_statement_free(stmt->as.for_stmt.body);
            break;

        case NODE_RETURN:
            limbo_ast_expression_free(stmt->as.return_stmt.value);
            break;

        case NODE_EXPRESSION_STMT:
            limbo_ast_expression_free(stmt->as.expression_stmt.expr);
            break;

        default:
            break;
    }

    free(stmt);
}

/**
 * Free function
 */
void limbo_ast_function_free(LimboFunction* func) {
    if (!func) return;

    if (func->name) free((void*)func->name);
    if (func->return_type) free((void*)func->return_type);

    for (size_t i = 0; i < func->parameters.count; i++) {
        if (func->parameters.names[i]) free((void*)func->parameters.names[i]);
        if (func->parameters.types[i]) free((void*)func->parameters.types[i]);
    }
    free(func->parameters.names);
    free(func->parameters.types);

    if (func->body) limbo_ast_statement_free(func->body);
    if (func->receiver_type) free((void*)func->receiver_type);
    if (func->receiver_name) free((void*)func->receiver_name);

    free(func);
}

/**
 * Free module
 */
void limbo_ast_module_free(LimboModule* module) {
    if (!module) return;

    if (module->name) free((void*)module->name);

    for (size_t i = 0; i < module->import_count; i++) {
        if (module->imports[i]) {
            if (module->imports[i]->module_path) free((void*)module->imports[i]->module_path);
            if (module->imports[i]->alias) free((void*)module->imports[i]->alias);
            free(module->imports[i]);
        }
    }
    free(module->imports);

    for (size_t i = 0; i < module->constant_count; i++) {
        if (module->constants[i]) {
            if (module->constants[i]->name) free((void*)module->constants[i]->name);
            if (module->constants[i]->type) free((void*)module->constants[i]->type);
            if (module->constants[i]->value) limbo_ast_expression_free(module->constants[i]->value);
            free(module->constants[i]);
        }
    }
    free(module->constants);

    for (size_t i = 0; i < module->function_count; i++) {
        limbo_ast_function_free(module->functions[i]);
    }
    free(module->functions);

    for (size_t i = 0; i < module->variable_count; i++) {
        if (module->variables[i]) {
            if (module->variables[i]->name) free((void*)module->variables[i]->name);
            if (module->variables[i]->type) free((void*)module->variables[i]->type);
            if (module->variables[i]->initializer) limbo_ast_expression_free(module->variables[i]->initializer);
            free(module->variables[i]);
        }
    }
    free(module->variables);

    if (module->interface.function_names) {
        for (size_t i = 0; i < module->interface.function_count; i++) {
            if (module->interface.function_names[i]) {
                free((void*)module->interface.function_names[i]);
            }
        }
        free(module->interface.function_names);
    }

    free(module);
}

/**
 * Parse module (simplified - just extracts name and basic structure)
 */
static LimboModule* parse_module(LimboParser* parser) {
    LimboModule* module = NULL;

    // Look for: module name { ... }
    if (check(parser, TOKEN_MODULE)) {
        advance(parser);  // consume 'module'

        if (check(parser, TOKEN_IDENTIFIER)) {
            LimboToken name_token = advance(parser);
            char name[256];
            snprintf(name, sizeof(name), "%.*s", (int)name_token.length, name_token.text);
            module = limbo_ast_module_create(name, name_token);
        } else {
            parser->has_error = true;
            snprintf(parser->error_message, sizeof(parser->error_message),
                     "Expected module name at line %d", parser->current_token.line);
            return NULL;
        }

        if (!consume(parser, TOKEN_LBRACE, "Expected '{' after module name")) {
            limbo_ast_module_free(module);
            return NULL;
        }
    } else {
        // No module declaration, create anonymous module
        LimboToken token = {0};
        module = limbo_ast_module_create("anonymous", token);
    }

    // Parse until end of file or closing brace
    int brace_count = check(parser, TOKEN_MODULE) ? 1 : 0;

    while (parser->current_token.type != TOKEN_EOF && brace_count >= 0) {
        if (check(parser, TOKEN_LBRACE)) {
            brace_count++;
            advance(parser);
        } else if (check(parser, TOKEN_RBRACE)) {
            brace_count--;
            if (brace_count < 0) break;
            advance(parser);
        } else {
            advance(parser);  // Skip tokens we don't parse yet
        }
    }

    return module;
}

/**
 * Parse Limbo source
 */
LimboModule* limbo_parser_parse(LimboParser* parser) {
    if (!parser) return NULL;

    return parse_module(parser);
}

/**
 * Parse Limbo source directly
 */
LimboModule* limbo_parse(const char* source, size_t source_len) {
    LimboParser* parser = limbo_parser_create(source, source_len);
    if (!parser) return NULL;

    LimboModule* module = limbo_parser_parse(parser);

    const char* error = limbo_parser_get_error(parser);
    if (error) {
        fprintf(stderr, "%s\n", error);
    }

    limbo_parser_free(parser);

    return module;
}

/**
 * Parse Limbo file
 */
LimboModule* limbo_parse_file(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    LimboModule* module = limbo_parse(source, size);
    free(source);

    return module;
}
