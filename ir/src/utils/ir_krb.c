/**
 * @file ir_krb.c
 * @brief Kryon Bytecode (.krb) format implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/ir_krb.h"
#include "../include/ir_serialization.h"
#include "../include/ir_logic.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table(void) {
    if (crc32_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

uint32_t krb_crc32(const void* data, size_t size) {
    init_crc32_table();
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ bytes[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// OPCODE NAMES (for disassembly)
// ============================================================================

const char* krb_opcode_name(KRBOpcode op) {
    switch (op) {
        case OP_NOP: return "NOP";
        case OP_PUSH_NULL: return "PUSH_NULL";
        case OP_PUSH_TRUE: return "PUSH_TRUE";
        case OP_PUSH_FALSE: return "PUSH_FALSE";
        case OP_PUSH_INT8: return "PUSH_INT8";
        case OP_PUSH_INT16: return "PUSH_INT16";
        case OP_PUSH_INT32: return "PUSH_INT32";
        case OP_PUSH_INT64: return "PUSH_INT64";
        case OP_PUSH_FLOAT: return "PUSH_FLOAT";
        case OP_PUSH_DOUBLE: return "PUSH_DOUBLE";
        case OP_PUSH_STR: return "PUSH_STR";
        case OP_POP: return "POP";
        case OP_DUP: return "DUP";
        case OP_SWAP: return "SWAP";
        case OP_LOAD_LOCAL: return "LOAD_LOCAL";
        case OP_STORE_LOCAL: return "STORE_LOCAL";
        case OP_LOAD_GLOBAL: return "LOAD_GLOBAL";
        case OP_STORE_GLOBAL: return "STORE_GLOBAL";
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_MOD: return "MOD";
        case OP_NEG: return "NEG";
        case OP_INC: return "INC";
        case OP_DEC: return "DEC";
        case OP_EQ: return "EQ";
        case OP_NE: return "NE";
        case OP_LT: return "LT";
        case OP_LE: return "LE";
        case OP_GT: return "GT";
        case OP_GE: return "GE";
        case OP_AND: return "AND";
        case OP_OR: return "OR";
        case OP_NOT: return "NOT";
        case OP_BAND: return "BAND";
        case OP_BOR: return "BOR";
        case OP_BXOR: return "BXOR";
        case OP_BNOT: return "BNOT";
        case OP_SHL: return "SHL";
        case OP_SHR: return "SHR";
        case OP_JMP: return "JMP";
        case OP_JMP_IF: return "JMP_IF";
        case OP_JMP_IF_NOT: return "JMP_IF_NOT";
        case OP_CALL: return "CALL";
        case OP_CALL_NATIVE: return "CALL_NATIVE";
        case OP_RET: return "RET";
        case OP_RET_VAL: return "RET_VAL";
        case OP_GET_COMP: return "GET_COMP";
        case OP_SET_PROP: return "SET_PROP";
        case OP_GET_PROP: return "GET_PROP";
        case OP_SET_TEXT: return "SET_TEXT";
        case OP_SET_VISIBLE: return "SET_VISIBLE";
        case OP_ADD_CHILD: return "ADD_CHILD";
        case OP_REMOVE_CHILD: return "REMOVE_CHILD";
        case OP_REDRAW: return "REDRAW";
        case OP_STR_CONCAT: return "STR_CONCAT";
        case OP_STR_LEN: return "STR_LEN";
        case OP_STR_SUBSTR: return "STR_SUBSTR";
        case OP_STR_FORMAT: return "STR_FORMAT";
        case OP_ARR_NEW: return "ARR_NEW";
        case OP_ARR_GET: return "ARR_GET";
        case OP_ARR_SET: return "ARR_SET";
        case OP_ARR_PUSH: return "ARR_PUSH";
        case OP_ARR_POP: return "ARR_POP";
        case OP_ARR_LEN: return "ARR_LEN";
        case OP_DEBUG_PRINT: return "DEBUG_PRINT";
        case OP_DEBUG_BREAK: return "DEBUG_BREAK";
        case OP_HALT: return "HALT";
        default: return "UNKNOWN";
    }
}

const char* krb_property_name(KRBPropertyID prop) {
    switch (prop) {
        case PROP_TEXT: return "text";
        case PROP_VISIBLE: return "visible";
        case PROP_ENABLED: return "enabled";
        case PROP_WIDTH: return "width";
        case PROP_HEIGHT: return "height";
        case PROP_X: return "x";
        case PROP_Y: return "y";
        case PROP_BG_COLOR: return "bg_color";
        case PROP_FG_COLOR: return "fg_color";
        case PROP_BORDER_COLOR: return "border_color";
        case PROP_BORDER_WIDTH: return "border_width";
        case PROP_BORDER_RADIUS: return "border_radius";
        case PROP_FONT_SIZE: return "font_size";
        case PROP_FONT_WEIGHT: return "font_weight";
        case PROP_OPACITY: return "opacity";
        case PROP_PADDING: return "padding";
        case PROP_MARGIN: return "margin";
        case PROP_GAP: return "gap";
        default: return "unknown";
    }
}

// ============================================================================
// STRING TABLE
// ============================================================================

typedef struct {
    char** strings;
    uint32_t count;
    uint32_t capacity;
} StringTable;

static void string_table_init(StringTable* st) {
    st->count = 0;
    st->capacity = 64;
    st->strings = (char**)malloc(sizeof(char*) * st->capacity);
}

static void string_table_free(StringTable* st) {
    for (uint32_t i = 0; i < st->count; i++) {
        free(st->strings[i]);
    }
    free(st->strings);
}

static uint16_t string_table_add(StringTable* st, const char* str) {
    if (!str) str = "";

    // Check if string already exists
    for (uint32_t i = 0; i < st->count; i++) {
        if (strcmp(st->strings[i], str) == 0) {
            return (uint16_t)i;
        }
    }

    // Add new string
    if (st->count >= st->capacity) {
        st->capacity *= 2;
        st->strings = (char**)realloc(st->strings, sizeof(char*) * st->capacity);
    }

    size_t len = strlen(str);
    st->strings[st->count] = (char*)malloc(len + 1);
    strcpy(st->strings[st->count], str);
    return (uint16_t)st->count++;
}

// ============================================================================
// BYTECODE BUILDER
// ============================================================================

typedef struct {
    uint8_t* code;
    uint32_t size;
    uint32_t capacity;
} BytecodeBuilder;

static void bytecode_init(BytecodeBuilder* bb) {
    bb->size = 0;
    bb->capacity = 1024;
    bb->code = (uint8_t*)malloc(bb->capacity);
}

static void bytecode_free(BytecodeBuilder* bb) {
    free(bb->code);
}

static void bytecode_ensure_capacity(BytecodeBuilder* bb, uint32_t needed) {
    while (bb->size + needed > bb->capacity) {
        bb->capacity *= 2;
        bb->code = (uint8_t*)realloc(bb->code, bb->capacity);
    }
}

static void bytecode_emit_u8(BytecodeBuilder* bb, uint8_t value) {
    bytecode_ensure_capacity(bb, 1);
    bb->code[bb->size++] = value;
}

static void bytecode_emit_u16(BytecodeBuilder* bb, uint16_t value) {
    bytecode_ensure_capacity(bb, 2);
    bb->code[bb->size++] = value & 0xFF;
    bb->code[bb->size++] = (value >> 8) & 0xFF;
}

static void bytecode_emit_u32(BytecodeBuilder* bb, uint32_t value) {
    bytecode_ensure_capacity(bb, 4);
    bb->code[bb->size++] = value & 0xFF;
    bb->code[bb->size++] = (value >> 8) & 0xFF;
    bb->code[bb->size++] = (value >> 16) & 0xFF;
    bb->code[bb->size++] = (value >> 24) & 0xFF;
}

static void bytecode_emit_i64(BytecodeBuilder* bb, int64_t value) {
    bytecode_ensure_capacity(bb, 8);
    for (int i = 0; i < 8; i++) {
        bb->code[bb->size++] = (value >> (i * 8)) & 0xFF;
    }
}

static void bytecode_emit_f32(BytecodeBuilder* bb, float value) {
    union { float f; uint32_t u; } conv = { .f = value };
    bytecode_emit_u32(bb, conv.u);
}

// ============================================================================
// EXPRESSION COMPILATION
// ============================================================================

static void compile_expression(BytecodeBuilder* bb, StringTable* st, IRExpression* expr);

static void compile_expression(BytecodeBuilder* bb, StringTable* st, IRExpression* expr) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            if (expr->int_value >= -128 && expr->int_value <= 127) {
                bytecode_emit_u8(bb, OP_PUSH_INT8);
                bytecode_emit_u8(bb, (int8_t)expr->int_value);
            } else if (expr->int_value >= -32768 && expr->int_value <= 32767) {
                bytecode_emit_u8(bb, OP_PUSH_INT16);
                bytecode_emit_u16(bb, (int16_t)expr->int_value);
            } else if (expr->int_value >= INT32_MIN && expr->int_value <= INT32_MAX) {
                bytecode_emit_u8(bb, OP_PUSH_INT32);
                bytecode_emit_u32(bb, (int32_t)expr->int_value);
            } else {
                bytecode_emit_u8(bb, OP_PUSH_INT64);
                bytecode_emit_i64(bb, expr->int_value);
            }
            break;

        case EXPR_LITERAL_FLOAT:
            bytecode_emit_u8(bb, OP_PUSH_FLOAT);
            bytecode_emit_f32(bb, (float)expr->float_value);
            break;

        case EXPR_LITERAL_STRING:
            bytecode_emit_u8(bb, OP_PUSH_STR);
            bytecode_emit_u16(bb, string_table_add(st, expr->string_value));
            break;

        case EXPR_LITERAL_BOOL:
            bytecode_emit_u8(bb, expr->bool_value ? OP_PUSH_TRUE : OP_PUSH_FALSE);
            break;

        case EXPR_LITERAL_NULL:
            bytecode_emit_u8(bb, OP_PUSH_NULL);
            break;

        case EXPR_VAR_REF:
            // Load variable (use global for now, locals would need scope analysis)
            bytecode_emit_u8(bb, OP_LOAD_GLOBAL);
            bytecode_emit_u16(bb, string_table_add(st, expr->var_ref.name) % 256);
            break;

        case EXPR_BINARY:
            // Compile left and right operands
            compile_expression(bb, st, expr->binary.left);
            compile_expression(bb, st, expr->binary.right);

            // Emit operator
            switch (expr->binary.op) {
                case BINARY_OP_ADD: bytecode_emit_u8(bb, OP_ADD); break;
                case BINARY_OP_SUB: bytecode_emit_u8(bb, OP_SUB); break;
                case BINARY_OP_MUL: bytecode_emit_u8(bb, OP_MUL); break;
                case BINARY_OP_DIV: bytecode_emit_u8(bb, OP_DIV); break;
                case BINARY_OP_MOD: bytecode_emit_u8(bb, OP_MOD); break;
                case BINARY_OP_EQ: bytecode_emit_u8(bb, OP_EQ); break;
                case BINARY_OP_NEQ: bytecode_emit_u8(bb, OP_NE); break;
                case BINARY_OP_LT: bytecode_emit_u8(bb, OP_LT); break;
                case BINARY_OP_LTE: bytecode_emit_u8(bb, OP_LE); break;
                case BINARY_OP_GT: bytecode_emit_u8(bb, OP_GT); break;
                case BINARY_OP_GTE: bytecode_emit_u8(bb, OP_GE); break;
                case BINARY_OP_AND: bytecode_emit_u8(bb, OP_AND); break;
                case BINARY_OP_OR: bytecode_emit_u8(bb, OP_OR); break;
                case BINARY_OP_CONCAT: bytecode_emit_u8(bb, OP_STR_CONCAT); break;
            }
            break;

        case EXPR_UNARY:
            compile_expression(bb, st, expr->unary.operand);
            switch (expr->unary.op) {
                case UNARY_OP_NEG: bytecode_emit_u8(bb, OP_NEG); break;
                case UNARY_OP_NOT: bytecode_emit_u8(bb, OP_NOT); break;
            }
            break;

        case EXPR_CALL:
            // Push arguments
            for (int i = 0; i < expr->call.arg_count; i++) {
                compile_expression(bb, st, expr->call.args[i]);
            }
            // Call function
            bytecode_emit_u8(bb, OP_CALL_NATIVE);
            bytecode_emit_u16(bb, string_table_add(st, expr->call.function));
            break;

        default:
            // Not implemented yet
            break;
    }
}

// ============================================================================
// STATEMENT COMPILATION
// ============================================================================

static void compile_statement(BytecodeBuilder* bb, StringTable* st, IRStatement* stmt);

static void compile_statement(BytecodeBuilder* bb, StringTable* st, IRStatement* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case STMT_ASSIGN:
            // Compile expression
            compile_expression(bb, st, stmt->assign.expr);
            // Store to global (variable index by name for now)
            bytecode_emit_u8(bb, OP_STORE_GLOBAL);
            bytecode_emit_u16(bb, string_table_add(st, stmt->assign.target) % 256);
            break;

        case STMT_ASSIGN_OP:
            // Load current value
            bytecode_emit_u8(bb, OP_LOAD_GLOBAL);
            bytecode_emit_u16(bb, string_table_add(st, stmt->assign_op.target) % 256);
            // Compile expression
            compile_expression(bb, st, stmt->assign_op.expr);
            // Apply operator
            switch (stmt->assign_op.op) {
                case ASSIGN_OP_ADD: bytecode_emit_u8(bb, OP_ADD); break;
                case ASSIGN_OP_SUB: bytecode_emit_u8(bb, OP_SUB); break;
                case ASSIGN_OP_MUL: bytecode_emit_u8(bb, OP_MUL); break;
                case ASSIGN_OP_DIV: bytecode_emit_u8(bb, OP_DIV); break;
            }
            // Store result
            bytecode_emit_u8(bb, OP_STORE_GLOBAL);
            bytecode_emit_u16(bb, string_table_add(st, stmt->assign_op.target) % 256);
            break;

        case STMT_IF:
            {
                // Compile condition
                compile_expression(bb, st, stmt->if_stmt.condition);

                // Jump if false
                bytecode_emit_u8(bb, OP_JMP_IF_NOT);
                uint32_t else_jump_pos = bb->size;
                bytecode_emit_u16(bb, 0);  // Placeholder

                // Compile then body
                for (int i = 0; i < stmt->if_stmt.then_count; i++) {
                    compile_statement(bb, st, stmt->if_stmt.then_body[i]);
                }

                if (stmt->if_stmt.else_count > 0) {
                    // Jump over else
                    bytecode_emit_u8(bb, OP_JMP);
                    uint32_t end_jump_pos = bb->size;
                    bytecode_emit_u16(bb, 0);  // Placeholder

                    // Patch else jump
                    uint16_t else_target = (uint16_t)bb->size;
                    bb->code[else_jump_pos] = else_target & 0xFF;
                    bb->code[else_jump_pos + 1] = (else_target >> 8) & 0xFF;

                    // Compile else body
                    for (int i = 0; i < stmt->if_stmt.else_count; i++) {
                        compile_statement(bb, st, stmt->if_stmt.else_body[i]);
                    }

                    // Patch end jump
                    uint16_t end_target = (uint16_t)bb->size;
                    bb->code[end_jump_pos] = end_target & 0xFF;
                    bb->code[end_jump_pos + 1] = (end_target >> 8) & 0xFF;
                } else {
                    // Patch else jump to end
                    uint16_t else_target = (uint16_t)bb->size;
                    bb->code[else_jump_pos] = else_target & 0xFF;
                    bb->code[else_jump_pos + 1] = (else_target >> 8) & 0xFF;
                }
            }
            break;

        case STMT_WHILE:
            {
                uint32_t loop_start = bb->size;

                // Compile condition
                compile_expression(bb, st, stmt->while_stmt.condition);

                // Jump if false
                bytecode_emit_u8(bb, OP_JMP_IF_NOT);
                uint32_t end_jump_pos = bb->size;
                bytecode_emit_u16(bb, 0);  // Placeholder

                // Compile body
                for (int i = 0; i < stmt->while_stmt.body_count; i++) {
                    compile_statement(bb, st, stmt->while_stmt.body[i]);
                }

                // Jump back to start
                bytecode_emit_u8(bb, OP_JMP);
                bytecode_emit_u16(bb, (uint16_t)loop_start);

                // Patch end jump
                uint16_t end_target = (uint16_t)bb->size;
                bb->code[end_jump_pos] = end_target & 0xFF;
                bb->code[end_jump_pos + 1] = (end_target >> 8) & 0xFF;
            }
            break;

        case STMT_CALL:
            // Push arguments
            for (int i = 0; i < stmt->call.arg_count; i++) {
                compile_expression(bb, st, stmt->call.args[i]);
            }
            // Call function
            bytecode_emit_u8(bb, OP_CALL_NATIVE);
            bytecode_emit_u16(bb, string_table_add(st, stmt->call.function));
            // Pop result
            bytecode_emit_u8(bb, OP_POP);
            break;

        case STMT_RETURN:
            if (stmt->return_stmt.value) {
                compile_expression(bb, st, stmt->return_stmt.value);
                bytecode_emit_u8(bb, OP_RET_VAL);
            } else {
                bytecode_emit_u8(bb, OP_RET);
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// FUNCTION COMPILATION
// ============================================================================

static bool compile_function(KRBModule* module, BytecodeBuilder* bb, StringTable* st,
                             IRLogicFunction* func, KRBFunction* out_func) {
    (void)module;

    out_func->code_offset = bb->size;
    out_func->param_count = func->param_count;
    out_func->local_count = 0;  // Would need analysis
    out_func->flags = 0;

    // Set function name
    size_t name_len = strlen(func->name);
    out_func->name = (char*)malloc(name_len + 1);
    strcpy(out_func->name, func->name);

    // Compile statements
    for (int i = 0; i < func->statement_count; i++) {
        compile_statement(bb, st, func->statements[i]);
    }

    // Add return if not present
    bytecode_emit_u8(bb, OP_RET);

    out_func->code_size = bb->size - out_func->code_offset;
    return true;
}

// ============================================================================
// MODULE COMPILATION
// ============================================================================

KRBModule* krb_compile_from_ir(IRComponent* root, IRLogicBlock* logic, IRReactiveManifest* manifest) {
    KRBModule* module = (KRBModule*)calloc(1, sizeof(KRBModule));

    // Initialize header
    module->header.magic = KRB_MAGIC;
    module->header.version_major = KRB_VERSION_MAJOR;
    module->header.version_minor = KRB_VERSION_MINOR;
    module->header.flags = 0;

    // Serialize UI tree to binary format
    IRBuffer* ui_buffer = ir_serialize_binary(root);
    if (ui_buffer) {
        module->ui_data = (uint8_t*)malloc(ui_buffer->size);
        memcpy(module->ui_data, ui_buffer->data, ui_buffer->size);
        module->ui_size = (uint32_t)ui_buffer->size;
        ir_buffer_destroy(ui_buffer);
    }

    // Initialize bytecode builder and string table
    BytecodeBuilder bb;
    StringTable st;
    bytecode_init(&bb);
    string_table_init(&st);

    // Compile logic functions
    if (logic && logic->function_count > 0) {
        module->function_count = logic->function_count;
        module->functions = (KRBFunction*)calloc(module->function_count, sizeof(KRBFunction));

        for (int i = 0; i < logic->function_count; i++) {
            compile_function(module, &bb, &st, logic->functions[i], &module->functions[i]);
        }

        // Copy event bindings
        module->event_binding_count = logic->event_binding_count;
        module->event_bindings = (KRBEventBinding*)calloc(module->event_binding_count, sizeof(KRBEventBinding));

        for (int i = 0; i < logic->event_binding_count; i++) {
            IREventBinding* src = logic->event_bindings[i];
            module->event_bindings[i].component_id = src->component_id;

            // Map event type string to numeric
            if (strcmp(src->event_type, "click") == 0) {
                module->event_bindings[i].event_type = 0;
            } else if (strcmp(src->event_type, "change") == 0) {
                module->event_bindings[i].event_type = 1;
            } else {
                module->event_bindings[i].event_type = 0;
            }

            // Find function index by name
            for (uint32_t j = 0; j < module->function_count; j++) {
                if (strcmp(module->functions[j].name, src->handler_name) == 0) {
                    module->event_bindings[i].function_index = (uint16_t)j;
                    break;
                }
            }
        }
    }

    // Copy bytecode
    module->code_data = (uint8_t*)malloc(bb.size);
    memcpy(module->code_data, bb.code, bb.size);
    module->code_size = bb.size;

    // Copy string table
    module->string_count = st.count;
    module->string_table = (char**)malloc(sizeof(char*) * st.count);
    for (uint32_t i = 0; i < st.count; i++) {
        size_t len = strlen(st.strings[i]);
        module->string_table[i] = (char*)malloc(len + 1);
        strcpy(module->string_table[i], st.strings[i]);
    }

    // Initialize globals from manifest
    if (manifest && manifest->variable_count > 0) {
        module->global_count = manifest->variable_count;
        module->globals = (int64_t*)calloc(module->global_count, sizeof(int64_t));
        // Note: Would need to parse initial values from manifest
    }

    // Calculate section count
    module->header.section_count = 3;  // UI, code, strings (at minimum)
    if (module->function_count > 0) module->header.section_count++;  // functions
    if (module->event_binding_count > 0) module->header.section_count++;  // events

    bytecode_free(&bb);
    string_table_free(&st);

    return module;
}

KRBModule* krb_compile_from_kir(const char* kir_path) {
    // Load component tree using existing function
    IRComponent* root = ir_read_json_file(kir_path);
    if (!root) {
        fprintf(stderr, "[KRB] Failed to load KIR file: %s\n", kir_path);
        return NULL;
    }

    // Also parse logic block from the same file
    IRLogicBlock* logic = NULL;

    FILE* file = fopen(kir_path, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char* json_str = malloc(file_size + 1);
        if (json_str) {
            size_t read_size = fread(json_str, 1, file_size, file);
            json_str[read_size] = '\0';

            cJSON* json = cJSON_Parse(json_str);
            if (json) {
                cJSON* logic_json = cJSON_GetObjectItem(json, "logic");
                if (logic_json) {
                    logic = ir_logic_block_from_json(logic_json);
                    if (logic) {
                        fprintf(stderr, "[KRB] Loaded logic block: %d functions, %d event bindings\n",
                                logic->function_count, logic->event_binding_count);
                    }
                }
                cJSON_Delete(json);
            }
            free(json_str);
        }
        fclose(file);
    }

    // Compile to KRB
    KRBModule* module = krb_compile_from_ir(root, logic, NULL);

    // Cleanup
    ir_destroy_component(root);
    if (logic) ir_logic_block_free(logic);

    return module;
}

// ============================================================================
// FILE I/O
// ============================================================================

bool krb_write_file(KRBModule* module, const char* path) {
    FILE* file = fopen(path, "wb");
    if (!file) return false;

    // First pass: count sections
    int temp_section_count = 3;  // UI, CODE, STRINGS always present
    if (module->function_count > 0) temp_section_count++;
    if (module->event_binding_count > 0) temp_section_count++;

    // Calculate offsets
    uint32_t current_offset = sizeof(KRBHeader) + sizeof(KRBSectionHeader) * temp_section_count;

    // Prepare section headers
    KRBSectionHeader sections[8];
    int section_idx = 0;

    // UI section
    sections[section_idx].type = KRB_SECTION_UI;
    sections[section_idx].flags = 0;
    sections[section_idx].reserved = 0;
    sections[section_idx].offset = current_offset;
    sections[section_idx].size = module->ui_size;
    sections[section_idx].uncompressed_size = module->ui_size;
    current_offset += module->ui_size;
    section_idx++;

    // Code section
    sections[section_idx].type = KRB_SECTION_CODE;
    sections[section_idx].flags = 0;
    sections[section_idx].reserved = 0;
    sections[section_idx].offset = current_offset;
    sections[section_idx].size = module->code_size;
    sections[section_idx].uncompressed_size = module->code_size;
    current_offset += module->code_size;
    section_idx++;

    // String table section
    uint32_t string_section_size = 4;  // count
    for (uint32_t i = 0; i < module->string_count; i++) {
        string_section_size += 4 + strlen(module->string_table[i]);  // length + string
    }

    sections[section_idx].type = KRB_SECTION_STRINGS;
    sections[section_idx].flags = 0;
    sections[section_idx].reserved = 0;
    sections[section_idx].offset = current_offset;
    sections[section_idx].size = string_section_size;
    sections[section_idx].uncompressed_size = string_section_size;
    current_offset += string_section_size;
    section_idx++;

    // Functions section (if any)
    uint32_t funcs_section_size = 0;
    if (module->function_count > 0) {
        funcs_section_size = 4;  // count
        for (uint32_t i = 0; i < module->function_count; i++) {
            funcs_section_size += 4;  // name length
            if (module->functions[i].name)
                funcs_section_size += strlen(module->functions[i].name);
            funcs_section_size += 4 + 4 + 1 + 1 + 2;  // offset, size, params, locals, flags
        }

        sections[section_idx].type = KRB_SECTION_FUNCS;
        sections[section_idx].flags = 0;
        sections[section_idx].reserved = 0;
        sections[section_idx].offset = current_offset;
        sections[section_idx].size = funcs_section_size;
        sections[section_idx].uncompressed_size = funcs_section_size;
        current_offset += funcs_section_size;
        section_idx++;
    }

    // Events section (if any)
    uint32_t events_section_size = 0;
    if (module->event_binding_count > 0) {
        events_section_size = 4 + module->event_binding_count * 8;  // count + bindings

        sections[section_idx].type = KRB_SECTION_EVENTS;
        sections[section_idx].flags = 0;
        sections[section_idx].reserved = 0;
        sections[section_idx].offset = current_offset;
        sections[section_idx].size = events_section_size;
        sections[section_idx].uncompressed_size = events_section_size;
        current_offset += events_section_size;
        section_idx++;
    }

    // Update header section count
    module->header.section_count = section_idx;

    // Write header
    fwrite(&module->header, sizeof(KRBHeader), 1, file);

    // Write section headers
    for (int i = 0; i < section_idx; i++) {
        fwrite(&sections[i], sizeof(KRBSectionHeader), 1, file);
    }

    // Write UI data
    fwrite(module->ui_data, 1, module->ui_size, file);

    // Write code data
    fwrite(module->code_data, 1, module->code_size, file);

    // Write string table
    fwrite(&module->string_count, 4, 1, file);
    for (uint32_t i = 0; i < module->string_count; i++) {
        uint32_t len = (uint32_t)strlen(module->string_table[i]);
        fwrite(&len, 4, 1, file);
        fwrite(module->string_table[i], 1, len, file);
    }

    // Write functions section
    if (module->function_count > 0) {
        fwrite(&module->function_count, 4, 1, file);
        for (uint32_t i = 0; i < module->function_count; i++) {
            KRBFunction* f = &module->functions[i];
            uint32_t name_len = f->name ? strlen(f->name) : 0;
            fwrite(&name_len, 4, 1, file);
            if (name_len > 0) fwrite(f->name, 1, name_len, file);
            fwrite(&f->code_offset, 4, 1, file);
            fwrite(&f->code_size, 4, 1, file);
            fwrite(&f->param_count, 1, 1, file);
            fwrite(&f->local_count, 1, 1, file);
            fwrite(&f->flags, 2, 1, file);
        }
    }

    // Write events section
    if (module->event_binding_count > 0) {
        fwrite(&module->event_binding_count, 4, 1, file);
        for (uint32_t i = 0; i < module->event_binding_count; i++) {
            fwrite(&module->event_bindings[i].component_id, 4, 1, file);
            fwrite(&module->event_bindings[i].event_type, 2, 1, file);
            fwrite(&module->event_bindings[i].function_index, 2, 1, file);
        }
    }

    fclose(file);
    return true;
}

KRBModule* krb_read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    KRBModule* module = (KRBModule*)calloc(1, sizeof(KRBModule));

    // Read header
    if (fread(&module->header, sizeof(KRBHeader), 1, file) != 1) {
        free(module);
        fclose(file);
        return NULL;
    }

    // Verify magic
    if (module->header.magic != KRB_MAGIC) {
        fprintf(stderr, "[KRB] Invalid magic number\n");
        free(module);
        fclose(file);
        return NULL;
    }

    // Read section headers
    for (uint32_t i = 0; i < module->header.section_count; i++) {
        KRBSectionHeader sh;
        if (fread(&sh, sizeof(KRBSectionHeader), 1, file) != 1) {
            krb_module_free(module);
            fclose(file);
            return NULL;
        }

        long header_pos = ftell(file);  // Save position for next header
        fseek(file, sh.offset, SEEK_SET);

        switch (sh.type) {
            case KRB_SECTION_UI:
                module->ui_data = (uint8_t*)malloc(sh.size);
                module->ui_size = sh.size;
                if (fread(module->ui_data, 1, sh.size, file) != sh.size) {
                    // Handle error
                }
                break;

            case KRB_SECTION_CODE:
                module->code_data = (uint8_t*)malloc(sh.size);
                module->code_size = sh.size;
                if (fread(module->code_data, 1, sh.size, file) != sh.size) {
                    // Handle error
                }
                break;

            case KRB_SECTION_STRINGS:
                if (fread(&module->string_count, 4, 1, file) != 1) {
                    // Handle error
                }
                module->string_table = (char**)malloc(sizeof(char*) * module->string_count);
                for (uint32_t j = 0; j < module->string_count; j++) {
                    uint32_t len;
                    if (fread(&len, 4, 1, file) != 1) break;
                    module->string_table[j] = (char*)malloc(len + 1);
                    if (fread(module->string_table[j], 1, len, file) != len) break;
                    module->string_table[j][len] = '\0';
                }
                break;

            case KRB_SECTION_FUNCS:
                if (fread(&module->function_count, 4, 1, file) != 1) break;
                module->functions = (KRBFunction*)calloc(module->function_count, sizeof(KRBFunction));
                for (uint32_t j = 0; j < module->function_count; j++) {
                    KRBFunction* f = &module->functions[j];
                    uint32_t name_len;
                    if (fread(&name_len, 4, 1, file) != 1) break;
                    if (name_len > 0) {
                        f->name = (char*)malloc(name_len + 1);
                        if (fread(f->name, 1, name_len, file) != name_len) break;
                        f->name[name_len] = '\0';
                    }
                    if (fread(&f->code_offset, 4, 1, file) != 1) break;
                    if (fread(&f->code_size, 4, 1, file) != 1) break;
                    if (fread(&f->param_count, 1, 1, file) != 1) break;
                    if (fread(&f->local_count, 1, 1, file) != 1) break;
                    if (fread(&f->flags, 2, 1, file) != 1) break;
                }
                break;

            case KRB_SECTION_EVENTS:
                if (fread(&module->event_binding_count, 4, 1, file) != 1) break;
                module->event_bindings = (KRBEventBinding*)calloc(module->event_binding_count, sizeof(KRBEventBinding));
                for (uint32_t j = 0; j < module->event_binding_count; j++) {
                    if (fread(&module->event_bindings[j].component_id, 4, 1, file) != 1) break;
                    if (fread(&module->event_bindings[j].event_type, 2, 1, file) != 1) break;
                    if (fread(&module->event_bindings[j].function_index, 2, 1, file) != 1) break;
                }
                break;

            default:
                break;
        }

        fseek(file, header_pos, SEEK_SET);  // Restore position for next header
    }

    fclose(file);
    return module;
}

void krb_module_free(KRBModule* module) {
    if (!module) return;

    free(module->ui_data);
    free(module->code_data);
    free(module->data_data);

    for (uint32_t i = 0; i < module->string_count; i++) {
        free(module->string_table[i]);
    }
    free(module->string_table);

    for (uint32_t i = 0; i < module->function_count; i++) {
        free(module->functions[i].name);
    }
    free(module->functions);
    free(module->event_bindings);
    free(module->globals);

    free(module);
}

// ============================================================================
// DISASSEMBLY
// ============================================================================

char* krb_disassemble(KRBModule* module) {
    if (!module || !module->code_data) return NULL;

    // Allocate buffer for disassembly
    size_t buf_size = module->code_size * 64;  // Estimate
    char* result = (char*)malloc(buf_size);
    size_t offset = 0;

    offset += snprintf(result + offset, buf_size - offset,
                       "; KRB Disassembly\n"
                       "; Version: %d.%d\n"
                       "; Code size: %u bytes\n"
                       "; Strings: %u\n"
                       "; Functions: %u\n\n",
                       module->header.version_major, module->header.version_minor,
                       module->code_size, module->string_count, module->function_count);

    uint32_t ip = 0;
    while (ip < module->code_size) {
        uint8_t op = module->code_data[ip];
        const char* name = krb_opcode_name(op);

        offset += snprintf(result + offset, buf_size - offset, "%04X: %s", ip, name);
        ip++;

        // Handle operands based on opcode
        switch (op) {
            case OP_PUSH_INT8:
                offset += snprintf(result + offset, buf_size - offset, " %d",
                                   (int8_t)module->code_data[ip]);
                ip++;
                break;

            case OP_PUSH_INT16:
                {
                    int16_t val = module->code_data[ip] | (module->code_data[ip+1] << 8);
                    offset += snprintf(result + offset, buf_size - offset, " %d", val);
                    ip += 2;
                }
                break;

            case OP_PUSH_INT32:
                {
                    int32_t val = module->code_data[ip] | (module->code_data[ip+1] << 8) |
                                  (module->code_data[ip+2] << 16) | (module->code_data[ip+3] << 24);
                    offset += snprintf(result + offset, buf_size - offset, " %d", val);
                    ip += 4;
                }
                break;

            case OP_PUSH_STR:
            case OP_JMP:
            case OP_JMP_IF:
            case OP_JMP_IF_NOT:
            case OP_CALL:
            case OP_CALL_NATIVE:
            case OP_LOAD_GLOBAL:
            case OP_STORE_GLOBAL:
            case OP_SET_PROP:
            case OP_GET_PROP:
                {
                    uint16_t val = module->code_data[ip] | (module->code_data[ip+1] << 8);
                    offset += snprintf(result + offset, buf_size - offset, " %u", val);
                    if (op == OP_PUSH_STR && val < module->string_count) {
                        offset += snprintf(result + offset, buf_size - offset,
                                           " ; \"%s\"", module->string_table[val]);
                    }
                    ip += 2;
                }
                break;

            case OP_LOAD_LOCAL:
            case OP_STORE_LOCAL:
                offset += snprintf(result + offset, buf_size - offset, " %u",
                                   module->code_data[ip]);
                ip++;
                break;

            case OP_GET_COMP:
                {
                    uint32_t id = module->code_data[ip] | (module->code_data[ip+1] << 8) |
                                  (module->code_data[ip+2] << 16) | (module->code_data[ip+3] << 24);
                    offset += snprintf(result + offset, buf_size - offset, " #%u", id);
                    ip += 4;
                }
                break;

            default:
                break;
        }

        offset += snprintf(result + offset, buf_size - offset, "\n");
    }

    return result;
}

char* krb_disassemble_function(KRBModule* module, uint32_t function_index) {
    if (!module || function_index >= module->function_count) return NULL;

    KRBFunction* func = &module->functions[function_index];
    size_t buf_size = func->code_size * 64;
    char* result = (char*)malloc(buf_size);
    size_t offset = 0;

    offset += snprintf(result + offset, buf_size - offset,
                       "; Function: %s\n"
                       "; Params: %u, Locals: %u\n\n",
                       func->name ? func->name : "<anonymous>",
                       func->param_count, func->local_count);

    uint32_t ip = func->code_offset;
    uint32_t end = func->code_offset + func->code_size;

    while (ip < end && ip < module->code_size) {
        uint8_t op = module->code_data[ip];
        offset += snprintf(result + offset, buf_size - offset,
                           "%04X: %s\n", ip - func->code_offset, krb_opcode_name(op));
        ip++;

        // Skip operands (simplified)
        switch (op) {
            case OP_PUSH_INT8:
            case OP_LOAD_LOCAL:
            case OP_STORE_LOCAL:
                ip++; break;
            case OP_PUSH_INT16:
            case OP_PUSH_STR:
            case OP_JMP:
            case OP_JMP_IF:
            case OP_JMP_IF_NOT:
            case OP_CALL:
            case OP_CALL_NATIVE:
            case OP_LOAD_GLOBAL:
            case OP_STORE_GLOBAL:
            case OP_SET_PROP:
            case OP_GET_PROP:
                ip += 2; break;
            case OP_PUSH_INT32:
            case OP_PUSH_FLOAT:
            case OP_GET_COMP:
                ip += 4; break;
            case OP_PUSH_INT64:
            case OP_PUSH_DOUBLE:
                ip += 8; break;
            default: break;
        }
    }

    return result;
}

void krb_print_info(KRBModule* module) {
    if (!module) {
        printf("NULL module\n");
        return;
    }

    printf("=== KRB Module Info ===\n");
    printf("Magic: 0x%08X (%s)\n", module->header.magic,
           module->header.magic == KRB_MAGIC ? "valid" : "INVALID");
    printf("Version: %d.%d\n", module->header.version_major, module->header.version_minor);
    printf("Flags: 0x%04X\n", module->header.flags);
    printf("Sections: %u\n", module->header.section_count);
    printf("UI Size: %u bytes\n", module->ui_size);
    printf("Code Size: %u bytes\n", module->code_size);
    printf("Strings: %u\n", module->string_count);
    printf("Functions: %u\n", module->function_count);
    printf("Event Bindings: %u\n", module->event_binding_count);
    printf("Globals: %u\n", module->global_count);

    if (module->string_count > 0) {
        printf("\n=== String Table ===\n");
        for (uint32_t i = 0; i < module->string_count && i < 20; i++) {
            printf("  [%u] \"%s\"\n", i, module->string_table[i]);
        }
        if (module->string_count > 20) {
            printf("  ... (%u more)\n", module->string_count - 20);
        }
    }

    if (module->function_count > 0) {
        printf("\n=== Functions ===\n");
        for (uint32_t i = 0; i < module->function_count; i++) {
            KRBFunction* f = &module->functions[i];
            printf("  [%u] %s (offset=%u, size=%u, params=%u, locals=%u)\n",
                   i, f->name ? f->name : "<anonymous>",
                   f->code_offset, f->code_size, f->param_count, f->local_count);
        }
    }

    if (module->event_binding_count > 0) {
        printf("\n=== Event Bindings ===\n");
        for (uint32_t i = 0; i < module->event_binding_count; i++) {
            KRBEventBinding* eb = &module->event_bindings[i];
            printf("  Component #%u, Event %u → Function %u\n",
                   eb->component_id, eb->event_type, eb->function_index);
        }
    }
}

// ============================================================================
// VM EXECUTION ENGINE
// ============================================================================

// Stack operations
static inline void vm_push(KRBRuntime* rt, KRBValue val) {
    if (rt->stack_top >= rt->stack_capacity) {
        rt->stack_capacity *= 2;
        rt->stack = (KRBValue*)realloc(rt->stack, rt->stack_capacity * sizeof(KRBValue));
    }
    rt->stack[rt->stack_top++] = val;
}

static inline KRBValue vm_pop(KRBRuntime* rt) {
    if (rt->stack_top == 0) {
        KRBValue null_val = { .type = VAL_NULL };
        return null_val;
    }
    return rt->stack[--rt->stack_top];
}

static inline KRBValue vm_peek(KRBRuntime* rt) {
    if (rt->stack_top == 0) {
        KRBValue null_val = { .type = VAL_NULL };
        return null_val;
    }
    return rt->stack[rt->stack_top - 1];
}

// Read bytecode operands
static inline uint8_t vm_read_u8(KRBRuntime* rt) {
    return rt->module->code_data[rt->ip++];
}

static inline int8_t vm_read_i8(KRBRuntime* rt) {
    return (int8_t)rt->module->code_data[rt->ip++];
}

static inline uint16_t vm_read_u16(KRBRuntime* rt) {
    uint16_t val = rt->module->code_data[rt->ip] |
                   (rt->module->code_data[rt->ip + 1] << 8);
    rt->ip += 2;
    return val;
}

static inline int16_t vm_read_i16(KRBRuntime* rt) {
    return (int16_t)vm_read_u16(rt);
}

static inline uint32_t vm_read_u32(KRBRuntime* rt) {
    uint32_t val = rt->module->code_data[rt->ip] |
                   (rt->module->code_data[rt->ip + 1] << 8) |
                   (rt->module->code_data[rt->ip + 2] << 16) |
                   (rt->module->code_data[rt->ip + 3] << 24);
    rt->ip += 4;
    return val;
}

static inline int64_t vm_read_i64(KRBRuntime* rt) {
    int64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= ((int64_t)rt->module->code_data[rt->ip + i]) << (i * 8);
    }
    rt->ip += 8;
    return val;
}

static inline float vm_read_f32(KRBRuntime* rt) {
    union { uint32_t u; float f; } conv;
    conv.u = vm_read_u32(rt);
    return conv.f;
}

KRBRuntime* krb_runtime_create(KRBModule* module) {
    KRBRuntime* rt = (KRBRuntime*)calloc(1, sizeof(KRBRuntime));
    rt->module = module;
    rt->stack_capacity = 256;
    rt->stack = (KRBValue*)calloc(rt->stack_capacity, sizeof(KRBValue));
    rt->call_capacity = 64;
    rt->call_stack = (KRBCallFrame*)calloc(rt->call_capacity, sizeof(KRBCallFrame));

    // Allocate globals based on string table (each unique variable gets a slot)
    // For now, just allocate a fixed number
    if (module->global_count == 0) {
        module->global_count = module->string_count > 0 ? module->string_count : 16;
        module->globals = (int64_t*)calloc(module->global_count, sizeof(int64_t));
    }

    return rt;
}

void krb_runtime_set_renderer(KRBRuntime* rt, struct IRRenderer* renderer) {
    if (rt) rt->renderer = renderer;
}

// Execute a single bytecode instruction
bool krb_runtime_step(KRBRuntime* rt) {
    if (!rt || !rt->module || !rt->module->code_data) return false;
    if (rt->ip >= rt->module->code_size) return false;

    uint8_t op = vm_read_u8(rt);
    KRBValue a, b, result;

    switch (op) {
        case OP_NOP:
            break;

        case OP_PUSH_NULL:
            result.type = VAL_NULL;
            vm_push(rt, result);
            break;

        case OP_PUSH_TRUE:
            result.type = VAL_BOOL;
            result.b = true;
            vm_push(rt, result);
            break;

        case OP_PUSH_FALSE:
            result.type = VAL_BOOL;
            result.b = false;
            vm_push(rt, result);
            break;

        case OP_PUSH_INT8:
            result.type = VAL_INT;
            result.i = vm_read_i8(rt);
            vm_push(rt, result);
            break;

        case OP_PUSH_INT16:
            result.type = VAL_INT;
            result.i = vm_read_i16(rt);
            vm_push(rt, result);
            break;

        case OP_PUSH_INT32:
            result.type = VAL_INT;
            result.i = (int32_t)vm_read_u32(rt);
            vm_push(rt, result);
            break;

        case OP_PUSH_INT64:
            result.type = VAL_INT;
            result.i = vm_read_i64(rt);
            vm_push(rt, result);
            break;

        case OP_PUSH_FLOAT:
            result.type = VAL_FLOAT;
            result.f = vm_read_f32(rt);
            vm_push(rt, result);
            break;

        case OP_PUSH_STR: {
            uint16_t str_idx = vm_read_u16(rt);
            result.type = VAL_STRING;
            if (str_idx < rt->module->string_count) {
                result.s = rt->module->string_table[str_idx];
            } else {
                result.s = "";
            }
            vm_push(rt, result);
            break;
        }

        case OP_POP:
            vm_pop(rt);
            break;

        case OP_DUP:
            vm_push(rt, vm_peek(rt));
            break;

        // Arithmetic
        case OP_ADD:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_INT;
            result.i = a.i + b.i;
            vm_push(rt, result);
            break;

        case OP_SUB:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_INT;
            result.i = a.i - b.i;
            vm_push(rt, result);
            break;

        case OP_MUL:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_INT;
            result.i = a.i * b.i;
            vm_push(rt, result);
            break;

        case OP_DIV:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_INT;
            result.i = (b.i != 0) ? a.i / b.i : 0;
            vm_push(rt, result);
            break;

        case OP_MOD:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_INT;
            result.i = (b.i != 0) ? a.i % b.i : 0;
            vm_push(rt, result);
            break;

        case OP_NEG:
            a = vm_pop(rt);
            result.type = VAL_INT;
            result.i = -a.i;
            vm_push(rt, result);
            break;

        // Comparison
        case OP_EQ:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = (a.i == b.i);
            vm_push(rt, result);
            break;

        case OP_NE:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = (a.i != b.i);
            vm_push(rt, result);
            break;

        case OP_LT:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = (a.i < b.i);
            vm_push(rt, result);
            break;

        case OP_LE:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = (a.i <= b.i);
            vm_push(rt, result);
            break;

        case OP_GT:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = (a.i > b.i);
            vm_push(rt, result);
            break;

        case OP_GE:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = (a.i >= b.i);
            vm_push(rt, result);
            break;

        // Logic
        case OP_AND:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = (a.b && b.b);
            vm_push(rt, result);
            break;

        case OP_OR:
            b = vm_pop(rt);
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = (a.b || b.b);
            vm_push(rt, result);
            break;

        case OP_NOT:
            a = vm_pop(rt);
            result.type = VAL_BOOL;
            result.b = !a.b;
            vm_push(rt, result);
            break;

        // Control flow
        case OP_JMP: {
            uint16_t target = vm_read_u16(rt);
            rt->ip = target;
            break;
        }

        case OP_JMP_IF: {
            uint16_t target = vm_read_u16(rt);
            a = vm_pop(rt);
            if (a.b || a.i != 0) {
                rt->ip = target;
            }
            break;
        }

        case OP_JMP_IF_NOT: {
            uint16_t target = vm_read_u16(rt);
            a = vm_pop(rt);
            if (!a.b && a.i == 0) {
                rt->ip = target;
            }
            break;
        }

        // Global variables
        case OP_LOAD_GLOBAL: {
            uint16_t idx = vm_read_u16(rt);
            result.type = VAL_INT;
            if (idx < rt->module->global_count) {
                result.i = rt->module->globals[idx];
            } else {
                result.i = 0;
            }
            vm_push(rt, result);
            break;
        }

        case OP_STORE_GLOBAL: {
            uint16_t idx = vm_read_u16(rt);
            a = vm_pop(rt);
            if (idx < rt->module->global_count) {
                rt->module->globals[idx] = a.i;
            }
            break;
        }

        case OP_RET:
            // Return from function - clear stack frame and return
            if (rt->call_depth > 0) {
                rt->call_depth--;
                rt->ip = rt->call_stack[rt->call_depth].return_address;
                rt->stack_top = rt->call_stack[rt->call_depth].stack_base;
            }
            return false;  // Signal function completed

        case OP_HALT:
            return false;

        default:
            // Unknown opcode - skip
            fprintf(stderr, "[VM] Unknown opcode 0x%02X at IP=%u\n", op, rt->ip - 1);
            return false;
    }

    return true;  // Continue execution
}

// Execute bytecode until completion or halt
static void vm_execute(KRBRuntime* rt, uint32_t start_ip) {
    rt->ip = start_ip;
    int max_steps = 10000;  // Prevent infinite loops

    while (max_steps-- > 0) {
        if (!krb_runtime_step(rt)) {
            break;
        }
    }

    if (max_steps <= 0) {
        fprintf(stderr, "[VM] Execution limit reached\n");
    }
}

// Find and execute handler for event
void krb_runtime_dispatch_event(KRBRuntime* rt, uint32_t component_id, uint16_t event_type) {
    if (!rt || !rt->module) return;

    // Find matching event binding
    for (uint32_t i = 0; i < rt->module->event_binding_count; i++) {
        KRBEventBinding* eb = &rt->module->event_bindings[i];
        if (eb->component_id == component_id && eb->event_type == event_type) {
            // Found handler - execute it
            if (eb->function_index < rt->module->function_count) {
                KRBFunction* func = &rt->module->functions[eb->function_index];
                vm_execute(rt, func->code_offset);
                rt->needs_redraw = true;
            }
            return;
        }
    }
}

int krb_runtime_run(KRBRuntime* rt) {
    if (!rt || !rt->module) return -1;

    // For now, just print info about the module
    // Full integration with renderer would happen here
    fprintf(stderr, "[VM] Module loaded: %u functions, %u event bindings\n",
            rt->module->function_count, rt->module->event_binding_count);

    // Print initial global state
    fprintf(stderr, "[VM] Initial globals:\n");
    for (uint32_t i = 0; i < rt->module->global_count && i < 10; i++) {
        if (i < rt->module->string_count) {
            fprintf(stderr, "  [%u] %s = %lld\n", i,
                    rt->module->string_table[i],
                    (long long)rt->module->globals[i]);
        }
    }

    return 0;
}

KRBValue krb_runtime_get_global(KRBRuntime* rt, uint32_t index) {
    KRBValue val = { .type = VAL_NULL };
    if (rt && rt->module && index < rt->module->global_count) {
        val.type = VAL_INT;
        val.i = rt->module->globals[index];
    }
    return val;
}

void krb_runtime_set_global(KRBRuntime* rt, uint32_t index, KRBValue value) {
    if (rt && rt->module && index < rt->module->global_count) {
        if (value.type == VAL_INT) {
            rt->module->globals[index] = value.i;
        }
    }
}

void krb_runtime_destroy(KRBRuntime* rt) {
    if (rt) {
        free(rt->stack);
        free(rt->call_stack);
        free(rt->error);
        free(rt);
    }
}
