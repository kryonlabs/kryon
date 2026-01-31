/**
 * Tcl Abstract Syntax Tree Definitions
 * Defines the AST structure for parsed Tcl/Tk code
 */

#ifndef TCL_AST_H
#define TCL_AST_H

#include <stdbool.h>

/* Forward declare cJSON to avoid circular dependency */
typedef struct cJSON cJSON;

/* ============================================================================
 * Token Types
 * ============================================================================ */

/**
 * Tcl token types
 */
typedef enum {
    TCL_TOKEN_EOF = 0,

    // Literals
    TCL_TOKEN_WORD,           // command or argument
    TCL_TOKEN_STRING,         // quoted string: "..." or {...}
    TCL_TOKEN_NUMBER,         // integer or float
    TCL_TOKEN_BRACKET,        // [command substitution]

    // Special characters
    TCL_TOKEN_DOLLAR,         // $variable
    TCL_TOKEN_POUND,          // #comment
    TCL_TOKEN_BACKSLASH,      // line continuation or escape
    TCL_TOKEN_SEMICOLON,      // command separator

    // Braces
    TCL_TOKEN_LBRACE,         // {
    TCL_TOKEN_RBRACE,         // }

    // Unknown
    TCL_TOKEN_UNKNOWN
} TclTokenType;

/**
 * Tcl token
 */
typedef struct {
    TclTokenType type;
    char* value;              // Token text (owned, must be freed)
    int line;                 // Line number (1-based)
    int column;               // Column number (1-based)
    int position;             // Position in source (0-based)
} TclToken;

/* ============================================================================
 * AST Node Types
 * ============================================================================ */

/**
 * Tcl statement types
 */
typedef enum {
    TCL_STMT_COMMAND,         // Regular command call
    TCL_STMT_WIDGET,          // Widget creation (button, label, etc.)
    TCL_STMT_PACK,            // pack command
    TCL_STMT_GRID,            // grid command
    TCL_STMT_PLACE,           // place command
    TCL_STMT_BIND,            // bind command (event handlers)
    TCL_STMT_PROC,            // proc definition
    TCL_STMT_SET,             // set variable
    TCL_STMT_UNKNOWN
} TclStatementType;

/**
 * Tcl AST node base
 */
typedef struct TclNode {
    int node_type;            // TclStatementType
    int line;
} TclNode;

/**
 * Tcl command statement
 * Represents: command arg1 arg2 ...
 */
typedef struct {
    TclNode base;
    char* command_name;        // e.g., "button", "pack", "grid"
    char** arguments;          // Array of argument strings
    int argument_count;
} TclCommand;

/**
 * Tcl widget creation
 * Represents: button .path -option value -option2 value2 ...
 */
typedef struct {
    TclCommand base;          // command_name = widget type
    char* widget_path;        // e.g., ".w1", ".w1.w2"
    cJSON* options;           // Parsed options as JSON object
    char* pack_command;       // Pack command if inline (can be NULL)
    char* grid_command;       // Grid command if inline (can be NULL)
    char* place_command;      // Place command if inline (can be NULL)
} TclWidget;

/**
 * Tcl pack layout command
 * Represents: pack .w1 .w2 -side left -fill both
 */
typedef struct {
    TclCommand base;          // command_name = "pack"
    char** widget_paths;      // Array of widget paths being packed
    int widget_count;
    cJSON* options;           // Pack options as JSON
} TclPackCommand;

/**
 * Tcl grid layout command
 * Represents: grid .w1 -row 0 -column 1
 */
typedef struct {
    TclCommand base;          // command_name = "grid"
    char** widget_paths;      // Array of widget paths being gridded
    int widget_count;
    cJSON* options;           // Grid options as JSON
} TclGridCommand;

/**
 * Tcl place layout command
 * Represents: place .w1 -x 10 -y 20
 */
typedef struct {
    TclCommand base;          // command_name = "place"
    char** widget_paths;      // Array of widget paths being placed
    int widget_count;
    cJSON* options;           // Place options as JSON
} TclPlaceCommand;

/**
 * Tcl bind command (event handlers)
 * Represents: bind .w1 <Button-1> { handler_code }
 */
typedef struct {
    TclCommand base;          // command_name = "bind"
    char* widget_path;        // Widget being bound
    char* event_pattern;      // e.g., "<Button-1>", "<Key-Return>"
    char* handler_code;       // Handler body (can be a command or proc name)
} TclBindCommand;

/**
 * Tcl proc definition
 * Represents: proc name {args} { body }
 */
typedef struct {
    TclCommand base;          // command_name = "proc"
    char* proc_name;
    char** parameters;        // Array of parameter names
    int parameter_count;
    char* body;               // Proc body
} TclProc;

/**
 * Tcl set command
 * Represents: set var value
 */
typedef struct {
    TclCommand base;          // command_name = "set"
    char* var_name;
    char* value;              // Can be expression or literal
} TclSetCommand;

/* ============================================================================
 * AST Structure
 * ============================================================================ */

/**
 * Complete Tcl AST
 */
typedef struct {
    TclCommand** statements;  // Array of all top-level statements
    int statement_count;
    TclWidget** widgets;      // Array of all widget creations
    int widget_count;
    TclPackCommand** pack_commands;  // Array of all pack commands
    int pack_command_count;
    TclGridCommand** grid_commands;  // Array of all grid commands
    int grid_command_count;
    TclPlaceCommand** place_commands; // Array of all place commands
    int place_command_count;
    TclBindCommand** bind_commands;   // Array of all bind commands
    int bind_command_count;
    TclProc** procs;          // Array of all proc definitions
    int proc_count;
} TclAST;

/* ============================================================================
 * Token Stream
 * ============================================================================ */

/**
 * Tcl token stream (result of lexing)
 */
typedef struct {
    TclToken* tokens;
    int token_count;
    int capacity;
    int current;              // Current position (for parser)
} TclTokenStream;

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/**
 * Free a single token
 */
void tcl_token_free(TclToken* token);

/**
 * Free token stream (all tokens)
 */
void tcl_token_stream_free(TclTokenStream* stream);

/**
 * Free AST node (any type)
 */
void tcl_node_free(TclNode* node);

/**
 * Free complete AST
 */
void tcl_ast_free(TclAST* ast);

/**
 * Create new AST
 */
TclAST* tcl_ast_create(void);

/* ============================================================================
 * Factory Functions
 * ============================================================================ */

/**
 * Create a token
 */
TclToken* tcl_token_create(TclTokenType type, const char* value, int line, int column);

/**
 * Create a command statement
 */
TclCommand* tcl_command_create(const char* command_name, int line);

/**
 * Create a widget statement
 */
TclWidget* tcl_widget_create(const char* widget_type, const char* widget_path, int line);

/**
 * Create a pack command
 */
TclPackCommand* tcl_pack_command_create(int line);

/**
 * Create a grid command
 */
TclGridCommand* tcl_grid_command_create(int line);

/**
 * Create a bind command
 */
TclBindCommand* tcl_bind_command_create(const char* widget_path, const char* event, int line);

/**
 * Create a proc definition
 */
TclProc* tcl_proc_create(const char* proc_name, int line);

/* ============================================================================
 * Lexer API (from tcl_lexer.c)
 * ============================================================================ */

/**
 * Tokenize Tcl source code
 */
TclTokenStream* tcl_lex(const char* source);

/**
 * Peek at token in stream
 */
TclToken* tcl_token_stream_peek(TclTokenStream* stream, int offset);

/**
 * Consume next token from stream
 */
TclToken* tcl_token_stream_consume(TclTokenStream* stream);

/* ============================================================================
 * Parser API (from tcl_parser.c)
 * ============================================================================ */

/**
 * Parse Tcl source code to AST
 */
TclAST* tcl_parse(const char* source);

#endif /* TCL_AST_H */
