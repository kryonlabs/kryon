#ifndef IR_KRB_H
#define IR_KRB_H

#include "../../include/ir_core.h"
#include "../../include/ir_logic.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// KRB FORMAT - Kryon Bytecode
// ============================================================================
// .krb files are executable bytecode bundles that run on the Kryon VM.
// They contain:
//   - UI component tree (from .kir/.kirb)
//   - Compiled bytecode for event handlers
//   - String constants and initial values
//   - Optional debug symbols
//
// Unlike .kir/.kirb (which are intermediate representations), .krb is
// designed for execution with a pluggable renderer backend.
// ============================================================================

// Magic number: "KRBY" in little-endian
#define KRB_MAGIC 0x5942524B

// Format version
#define KRB_VERSION_MAJOR 1
#define KRB_VERSION_MINOR 0

// Section types
typedef enum {
    KRB_SECTION_UI      = 0x01,  // Component tree
    KRB_SECTION_CODE    = 0x02,  // Bytecode
    KRB_SECTION_DATA    = 0x03,  // Constants, strings
    KRB_SECTION_META    = 0x04,  // Manifest, debug info
    KRB_SECTION_STRINGS = 0x05,  // String table
    KRB_SECTION_FUNCS   = 0x06,  // Function table
    KRB_SECTION_EVENTS  = 0x07,  // Event bindings
} KRBSectionType;

// File flags
typedef enum {
    KRB_FLAG_DEBUG      = 0x0001,  // Contains debug symbols
    KRB_FLAG_COMPRESSED = 0x0002,  // Sections are compressed
    KRB_FLAG_SIGNED     = 0x0004,  // Has digital signature
} KRBFlags;

// ============================================================================
// FILE HEADER
// ============================================================================

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;             // KRB_MAGIC ("KRBY")
    uint8_t  version_major;     // Format major version
    uint8_t  version_minor;     // Format minor version
    uint16_t flags;             // KRBFlags
    uint32_t section_count;     // Number of sections
    uint32_t entry_function;    // Index of entry function (usually 0)
    uint32_t checksum;          // CRC32 of all sections
    uint8_t  reserved[12];      // Reserved for future use
} KRBHeader;

typedef struct {
    uint8_t  type;              // KRBSectionType
    uint8_t  flags;             // Section-specific flags
    uint16_t reserved;
    uint32_t offset;            // Offset from start of file
    uint32_t size;              // Size in bytes
    uint32_t uncompressed_size; // Original size (if compressed)
} KRBSectionHeader;

#pragma pack(pop)

// ============================================================================
// BYTECODE OPCODES
// ============================================================================

typedef enum {
    // ========== Stack Operations ==========
    OP_NOP          = 0x00,  // No operation
    OP_PUSH_NULL    = 0x01,  // Push null
    OP_PUSH_TRUE    = 0x02,  // Push true (1)
    OP_PUSH_FALSE   = 0x03,  // Push false (0)
    OP_PUSH_INT8    = 0x04,  // Push 8-bit signed int (1 byte operand)
    OP_PUSH_INT16   = 0x05,  // Push 16-bit signed int (2 byte operand)
    OP_PUSH_INT32   = 0x06,  // Push 32-bit signed int (4 byte operand)
    OP_PUSH_INT64   = 0x07,  // Push 64-bit signed int (8 byte operand)
    OP_PUSH_FLOAT   = 0x08,  // Push 32-bit float (4 byte operand)
    OP_PUSH_DOUBLE  = 0x09,  // Push 64-bit double (8 byte operand)
    OP_PUSH_STR     = 0x0A,  // Push string from string table (2 byte index)
    OP_POP          = 0x0B,  // Discard top of stack
    OP_DUP          = 0x0C,  // Duplicate top of stack
    OP_SWAP         = 0x0D,  // Swap top two stack values

    // ========== Variables ==========
    OP_LOAD_LOCAL   = 0x10,  // Load local variable (1 byte index)
    OP_STORE_LOCAL  = 0x11,  // Store to local variable (1 byte index)
    OP_LOAD_GLOBAL  = 0x12,  // Load global/state variable (2 byte index)
    OP_STORE_GLOBAL = 0x13,  // Store to global/state variable (2 byte index)

    // ========== Arithmetic ==========
    OP_ADD          = 0x20,  // a + b
    OP_SUB          = 0x21,  // a - b
    OP_MUL          = 0x22,  // a * b
    OP_DIV          = 0x23,  // a / b
    OP_MOD          = 0x24,  // a % b
    OP_NEG          = 0x25,  // -a
    OP_INC          = 0x26,  // a + 1
    OP_DEC          = 0x27,  // a - 1

    // ========== Comparison ==========
    OP_EQ           = 0x30,  // a == b
    OP_NE           = 0x31,  // a != b
    OP_LT           = 0x32,  // a < b
    OP_LE           = 0x33,  // a <= b
    OP_GT           = 0x34,  // a > b
    OP_GE           = 0x35,  // a >= b

    // ========== Logic ==========
    OP_AND          = 0x40,  // a && b
    OP_OR           = 0x41,  // a || b
    OP_NOT          = 0x42,  // !a

    // ========== Bitwise ==========
    OP_BAND         = 0x48,  // a & b
    OP_BOR          = 0x49,  // a | b
    OP_BXOR         = 0x4A,  // a ^ b
    OP_BNOT         = 0x4B,  // ~a
    OP_SHL          = 0x4C,  // a << b
    OP_SHR          = 0x4D,  // a >> b

    // ========== Control Flow ==========
    OP_JMP          = 0x50,  // Unconditional jump (2 byte offset)
    OP_JMP_IF       = 0x51,  // Jump if true (2 byte offset)
    OP_JMP_IF_NOT   = 0x52,  // Jump if false (2 byte offset)
    OP_CALL         = 0x53,  // Call function (2 byte function index)
    OP_CALL_NATIVE  = 0x54,  // Call native/builtin (2 byte index)
    OP_RET          = 0x55,  // Return from function
    OP_RET_VAL      = 0x56,  // Return value from function

    // ========== UI Operations ==========
    OP_GET_COMP     = 0x60,  // Get component by ID (4 byte ID)
    OP_SET_PROP     = 0x61,  // Set property (2 byte prop ID)
    OP_GET_PROP     = 0x62,  // Get property (2 byte prop ID)
    OP_SET_TEXT     = 0x63,  // Set text content
    OP_SET_VISIBLE  = 0x64,  // Set visibility (bool on stack)
    OP_ADD_CHILD    = 0x65,  // Add child component
    OP_REMOVE_CHILD = 0x66,  // Remove child component
    OP_REDRAW       = 0x67,  // Trigger UI redraw

    // ========== String Operations ==========
    OP_STR_CONCAT   = 0x70,  // Concatenate strings
    OP_STR_LEN      = 0x71,  // Get string length
    OP_STR_SUBSTR   = 0x72,  // Get substring
    OP_STR_FORMAT   = 0x73,  // Format string with values

    // ========== Array Operations ==========
    OP_ARR_NEW      = 0x80,  // Create new array (1 byte initial size)
    OP_ARR_GET      = 0x81,  // Get array element (index on stack)
    OP_ARR_SET      = 0x82,  // Set array element
    OP_ARR_PUSH     = 0x83,  // Push to array
    OP_ARR_POP      = 0x84,  // Pop from array
    OP_ARR_LEN      = 0x85,  // Get array length

    // ========== Debug ==========
    OP_DEBUG_PRINT  = 0xF0,  // Print top of stack
    OP_DEBUG_BREAK  = 0xF1,  // Breakpoint

    OP_HALT         = 0xFF,  // End execution
} KRBOpcode;

// ============================================================================
// PROPERTY IDs (for OP_SET_PROP/OP_GET_PROP)
// ============================================================================

typedef enum {
    PROP_TEXT           = 0x0001,
    PROP_VISIBLE        = 0x0002,
    PROP_ENABLED        = 0x0003,
    PROP_WIDTH          = 0x0010,
    PROP_HEIGHT         = 0x0011,
    PROP_X              = 0x0012,
    PROP_Y              = 0x0013,
    PROP_BG_COLOR       = 0x0020,
    PROP_FG_COLOR       = 0x0021,
    PROP_BORDER_COLOR   = 0x0022,
    PROP_BORDER_WIDTH   = 0x0023,
    PROP_BORDER_RADIUS  = 0x0024,
    PROP_FONT_SIZE      = 0x0030,
    PROP_FONT_WEIGHT    = 0x0031,
    PROP_OPACITY        = 0x0040,
    PROP_PADDING        = 0x0050,
    PROP_MARGIN         = 0x0051,
    PROP_GAP            = 0x0052,
} KRBPropertyID;

// ============================================================================
// RUNTIME STRUCTURES
// ============================================================================

// Forward declarations
typedef struct KRBModule KRBModule;
typedef struct KRBRuntime KRBRuntime;
typedef struct KRBFunction KRBFunction;

// Function definition
typedef struct KRBFunction {
    char* name;                 // Function name
    uint32_t code_offset;       // Offset in code section
    uint32_t code_size;         // Size of bytecode
    uint8_t param_count;        // Number of parameters
    uint8_t local_count;        // Number of local variables
    uint16_t flags;             // Function flags
} KRBFunction;

// Event binding (component ID + event type → function)
typedef struct {
    uint32_t component_id;
    uint16_t event_type;        // 0=click, 1=change, 2=submit, etc.
    uint16_t function_index;    // Index into function table
} KRBEventBinding;

// Loaded module
typedef struct KRBModule {
    KRBHeader header;

    // Sections (raw data)
    uint8_t* ui_data;
    uint32_t ui_size;
    uint8_t* code_data;
    uint32_t code_size;
    uint8_t* data_data;
    uint32_t data_size;

    // Parsed tables
    char** string_table;
    uint32_t string_count;
    KRBFunction* functions;
    uint32_t function_count;
    KRBEventBinding* event_bindings;
    uint32_t event_binding_count;

    // Global state
    int64_t* globals;
    uint32_t global_count;
} KRBModule;

// VM stack value
typedef struct {
    enum { VAL_NULL, VAL_BOOL, VAL_INT, VAL_FLOAT, VAL_STRING, VAL_ARRAY } type;
    union {
        bool b;
        int64_t i;
        double f;
        char* s;
        struct { void* data; int len; } arr;
    };
} KRBValue;

// Call frame
typedef struct {
    uint32_t function_index;
    uint32_t return_address;
    uint32_t stack_base;
    KRBValue* locals;
    uint8_t local_count;
} KRBCallFrame;

// Runtime state
typedef struct KRBRuntime {
    KRBModule* module;

    // Execution state
    uint32_t ip;                // Instruction pointer
    KRBValue* stack;            // Value stack
    uint32_t stack_top;
    uint32_t stack_capacity;

    // Call stack
    KRBCallFrame* call_stack;
    uint32_t call_depth;
    uint32_t call_capacity;

    // UI state
    IRComponent* root;          // Component tree
    bool needs_redraw;

    // Renderer interface
    struct IRRenderer* renderer;

    // Error handling
    char* error;
    bool has_error;
} KRBRuntime;

// ============================================================================
// COMPILATION API
// ============================================================================

// Compile .kir file to .krb module
KRBModule* krb_compile_from_kir(const char* kir_path);

// Compile IR tree + logic block to .krb module
KRBModule* krb_compile_from_ir(IRComponent* root, IRLogicBlock* logic, IRReactiveManifest* manifest);

// Write module to .krb file
bool krb_write_file(KRBModule* module, const char* path);

// Read module from .krb file
KRBModule* krb_read_file(const char* path);

// Free module
void krb_module_free(KRBModule* module);

// ============================================================================
// RUNTIME API
// ============================================================================

// Create runtime for module
KRBRuntime* krb_runtime_create(KRBModule* module);

// Set renderer (must be called before run)
void krb_runtime_set_renderer(KRBRuntime* rt, struct IRRenderer* renderer);

// Run main loop (blocking)
int krb_runtime_run(KRBRuntime* rt);

// Execute one bytecode step (for debugging)
bool krb_runtime_step(KRBRuntime* rt);

// Dispatch event to handlers
void krb_runtime_dispatch_event(KRBRuntime* rt, uint32_t component_id, uint16_t event_type);

// Get/set global variable
KRBValue krb_runtime_get_global(KRBRuntime* rt, uint32_t index);
void krb_runtime_set_global(KRBRuntime* rt, uint32_t index, KRBValue value);

// Destroy runtime
void krb_runtime_destroy(KRBRuntime* rt);

// ============================================================================
// DISASSEMBLY / DEBUG
// ============================================================================

// Disassemble bytecode to text
char* krb_disassemble(KRBModule* module);

// Disassemble single function
char* krb_disassemble_function(KRBModule* module, uint32_t function_index);

// Print module info
void krb_print_info(KRBModule* module);

// ============================================================================
// UTILITY
// ============================================================================

// Calculate CRC32 checksum
uint32_t krb_crc32(const void* data, size_t size);

// Get opcode name
const char* krb_opcode_name(KRBOpcode op);

// Get property name
const char* krb_property_name(KRBPropertyID prop);

#endif // IR_KRB_H
