#define _POSIX_C_SOURCE 200809L
#include "ir_vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// VM lifecycle

IRVM* ir_vm_create(void) {
    IRVM* vm = (IRVM*)calloc(1, sizeof(IRVM));
    if (!vm) return NULL;

    vm->stack.sp = -1;  // Empty stack
    vm->pc = 0;
    vm->halted = false;
    vm->state.count = 0;
    vm->locals.count = 0;
    vm->host_funcs.count = 0;
    vm->error[0] = '\0';

    return vm;
}

void ir_vm_destroy(IRVM* vm) {
    if (!vm) return;

    // Free any string values on stack
    for (int i = 0; i <= vm->stack.sp; i++) {
        if (vm->stack.stack[i].type == VAL_STRING && vm->stack.stack[i].as.s) {
            free(vm->stack.stack[i].as.s);
        }
    }

    // Free any string values in state
    for (int i = 0; i < vm->state.count; i++) {
        if (vm->state.entries[i].value.type == VAL_STRING && vm->state.entries[i].value.as.s) {
            free(vm->state.entries[i].value.as.s);
        }
    }

    free(vm);
}

// Stack operations

void ir_vm_push_int(IRVM* vm, int64_t value) {
    IRValue v = {.type = VAL_INT, .as = {.i = value}};
    ir_vm_push(vm, v);
}

void ir_vm_push_float(IRVM* vm, double value) {
    IRValue v = {.type = VAL_FLOAT, .as = {.f = value}};
    ir_vm_push(vm, v);
}

void ir_vm_push_string(IRVM* vm, const char* value) {
    IRValue v = {.type = VAL_STRING, .as = {.s = value ? strdup(value) : NULL}};
    ir_vm_push(vm, v);
}

void ir_vm_push_bool(IRVM* vm, bool value) {
    IRValue v = {.type = VAL_BOOL, .as = {.b = value}};
    ir_vm_push(vm, v);
}

void ir_vm_push(IRVM* vm, IRValue value) {
    if (vm->stack.sp >= VM_STACK_MAX - 1) {
        snprintf(vm->error, sizeof(vm->error), "Stack overflow");
        vm->halted = true;
        return;
    }

    vm->stack.sp++;
    vm->stack.stack[vm->stack.sp] = value;
}

bool ir_vm_pop(IRVM* vm, IRValue* out) {
    if (vm->stack.sp < 0) {
        snprintf(vm->error, sizeof(vm->error), "Stack underflow");
        vm->halted = true;
        return false;
    }

    if (out) {
        *out = vm->stack.stack[vm->stack.sp];
    }
    vm->stack.sp--;
    return true;
}

IRValue* ir_vm_peek(IRVM* vm) {
    if (vm->stack.sp < 0) return NULL;
    return &vm->stack.stack[vm->stack.sp];
}

// State operations

bool ir_vm_get_state(IRVM* vm, uint32_t state_id, IRValue* out) {
    for (int i = 0; i < vm->state.count; i++) {
        if (vm->state.entries[i].id == state_id) {
            if (out) {
                *out = vm->state.entries[i].value;
            }
            return true;
        }
    }

    // State not found - return default (0)
    if (out) {
        out->type = VAL_INT;
        out->as.i = 0;
    }
    return false;
}

bool ir_vm_set_state(IRVM* vm, uint32_t state_id, IRValue value) {
    // Find existing state entry
    for (int i = 0; i < vm->state.count; i++) {
        if (vm->state.entries[i].id == state_id) {
            // Free old string if needed
            if (vm->state.entries[i].value.type == VAL_STRING && vm->state.entries[i].value.as.s) {
                free(vm->state.entries[i].value.as.s);
            }
            vm->state.entries[i].value = value;
            return true;
        }
    }

    // Add new state entry
    if (vm->state.count >= VM_STATE_MAX) {
        snprintf(vm->error, sizeof(vm->error), "State table full");
        vm->halted = true;
        return false;
    }

    vm->state.entries[vm->state.count].id = state_id;
    vm->state.entries[vm->state.count].value = value;
    vm->state.count++;
    return true;
}

// Local variable operations

bool ir_vm_get_local(IRVM* vm, uint32_t local_id, IRValue* out) {
    if (local_id >= (uint32_t)vm->locals.count) {
        // Local not found - return default (0)
        if (out) {
            out->type = VAL_INT;
            out->as.i = 0;
        }
        return false;
    }

    if (out) {
        *out = vm->locals.locals[local_id];
    }
    return true;
}

bool ir_vm_set_local(IRVM* vm, uint32_t local_id, IRValue value) {
    // Expand local table if needed
    while ((uint32_t)vm->locals.count <= local_id) {
        if (vm->locals.count >= VM_LOCAL_MAX) {
            snprintf(vm->error, sizeof(vm->error), "Local variable table full");
            vm->halted = true;
            return false;
        }
        vm->locals.locals[vm->locals.count].type = VAL_INT;
        vm->locals.locals[vm->locals.count].as.i = 0;
        vm->locals.count++;
    }

    // Free old string if needed
    if (vm->locals.locals[local_id].type == VAL_STRING && vm->locals.locals[local_id].as.s) {
        free(vm->locals.locals[local_id].as.s);
    }

    vm->locals.locals[local_id] = value;
    return true;
}

// Host function operations

bool ir_vm_register_host_function(IRVM* vm, uint32_t id, const char* name,
                                   IRHostFunctionCallback callback, void* user_data) {
    if (vm->host_funcs.count >= VM_HOST_FUNC_MAX) {
        snprintf(vm->error, sizeof(vm->error), "Host function registry full");
        return false;
    }

    vm->host_funcs.functions[vm->host_funcs.count].id = id;
    vm->host_funcs.functions[vm->host_funcs.count].name = name ? strdup(name) : NULL;
    vm->host_funcs.functions[vm->host_funcs.count].callback = callback;
    vm->host_funcs.functions[vm->host_funcs.count].user_data = user_data;
    vm->host_funcs.count++;
    return true;
}

bool ir_vm_call_host_function(IRVM* vm, uint32_t func_id) {
    for (int i = 0; i < vm->host_funcs.count; i++) {
        if (vm->host_funcs.functions[i].id == func_id) {
            if (vm->host_funcs.functions[i].callback) {
                vm->host_funcs.functions[i].callback(vm, vm->host_funcs.functions[i].user_data);
                return true;
            }
            snprintf(vm->error, sizeof(vm->error), "Host function %u has no callback", func_id);
            return false;
        }
    }

    // Host function not found - graceful degradation
    fprintf(stderr, "[VM] Host function %u not available\n", func_id);
    return false;
}

// Bytecode execution

bool ir_vm_execute(IRVM* vm, const uint8_t* bytecode, int size) {
    vm->code = bytecode;
    vm->code_size = size;
    vm->pc = 0;
    vm->halted = false;
    vm->error[0] = '\0';

    while (!vm->halted && vm->pc < vm->code_size) {
        if (!ir_vm_step(vm)) {
            return false;
        }
    }

    return !vm->halted || vm->error[0] == '\0';
}

bool ir_vm_step(IRVM* vm) {
    if (vm->pc >= vm->code_size) {
        vm->halted = true;
        return true;
    }

    IROpcode op = (IROpcode)vm->code[vm->pc++];

    switch (op) {
        // Stack operations
        case OP_PUSH_INT: {
            if (vm->pc + 8 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            int64_t value = 0;
            memcpy(&value, &vm->code[vm->pc], 8);
            vm->pc += 8;
            ir_vm_push_int(vm, value);
            break;
        }

        case OP_PUSH_FLOAT: {
            if (vm->pc + 8 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            double value = 0;
            memcpy(&value, &vm->code[vm->pc], 8);
            vm->pc += 8;
            ir_vm_push_float(vm, value);
            break;
        }

        case OP_PUSH_STRING: {
            // String length (4 bytes) + string data
            if (vm->pc + 4 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            uint32_t len = 0;
            memcpy(&len, &vm->code[vm->pc], 4);
            vm->pc += 4;

            if (vm->pc + len > (uint32_t)vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            char* str = (char*)malloc(len + 1);
            memcpy(str, &vm->code[vm->pc], len);
            str[len] = '\0';
            vm->pc += len;
            ir_vm_push_string(vm, str);
            free(str);  // ir_vm_push_string makes a copy
            break;
        }

        case OP_PUSH_BOOL: {
            if (vm->pc + 1 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            bool value = vm->code[vm->pc++] != 0;
            ir_vm_push_bool(vm, value);
            break;
        }

        case OP_POP: {
            IRValue value;
            if (!ir_vm_pop(vm, &value)) {
                return false;
            }
            // Free string if needed
            if (value.type == VAL_STRING && value.as.s) {
                free(value.as.s);
            }
            break;
        }

        case OP_DUP: {
            IRValue* top = ir_vm_peek(vm);
            if (!top) {
                snprintf(vm->error, sizeof(vm->error), "Stack underflow");
                vm->halted = true;
                return false;
            }
            // Make a copy for strings
            IRValue dup = *top;
            if (dup.type == VAL_STRING && dup.as.s) {
                dup.as.s = strdup(dup.as.s);
            }
            ir_vm_push(vm, dup);
            break;
        }

        // Arithmetic operations
        case OP_ADD: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) {
                return false;
            }
            if (a.type == VAL_INT && b.type == VAL_INT) {
                ir_vm_push_int(vm, a.as.i + b.as.i);
            } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                double av = (a.type == VAL_FLOAT) ? a.as.f : (double)a.as.i;
                double bv = (b.type == VAL_FLOAT) ? b.as.f : (double)b.as.i;
                ir_vm_push_float(vm, av + bv);
            } else {
                snprintf(vm->error, sizeof(vm->error), "OP_ADD requires numeric operands");
                vm->halted = true;
                return false;
            }
            break;
        }

        case OP_SUB: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type == VAL_INT && b.type == VAL_INT) {
                ir_vm_push_int(vm, a.as.i - b.as.i);
            } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                double av = (a.type == VAL_FLOAT) ? a.as.f : (double)a.as.i;
                double bv = (b.type == VAL_FLOAT) ? b.as.f : (double)b.as.i;
                ir_vm_push_float(vm, av - bv);
            } else {
                snprintf(vm->error, sizeof(vm->error), "OP_SUB requires numeric operands");
                vm->halted = true;
                return false;
            }
            break;
        }

        case OP_MUL: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type == VAL_INT && b.type == VAL_INT) {
                ir_vm_push_int(vm, a.as.i * b.as.i);
            } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                double av = (a.type == VAL_FLOAT) ? a.as.f : (double)a.as.i;
                double bv = (b.type == VAL_FLOAT) ? b.as.f : (double)b.as.i;
                ir_vm_push_float(vm, av * bv);
            } else {
                snprintf(vm->error, sizeof(vm->error), "OP_MUL requires numeric operands");
                vm->halted = true;
                return false;
            }
            break;
        }

        case OP_DIV: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if ((b.type == VAL_INT && b.as.i == 0) || (b.type == VAL_FLOAT && b.as.f == 0.0)) {
                snprintf(vm->error, sizeof(vm->error), "Division by zero");
                vm->halted = true;
                return false;
            }
            if (a.type == VAL_INT && b.type == VAL_INT) {
                ir_vm_push_int(vm, a.as.i / b.as.i);
            } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                double av = (a.type == VAL_FLOAT) ? a.as.f : (double)a.as.i;
                double bv = (b.type == VAL_FLOAT) ? b.as.f : (double)b.as.i;
                ir_vm_push_float(vm, av / bv);
            } else {
                snprintf(vm->error, sizeof(vm->error), "OP_DIV requires numeric operands");
                vm->halted = true;
                return false;
            }
            break;
        }

        case OP_MOD: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_INT || b.type != VAL_INT) {
                snprintf(vm->error, sizeof(vm->error), "OP_MOD requires integer operands");
                vm->halted = true;
                return false;
            }
            if (b.as.i == 0) {
                snprintf(vm->error, sizeof(vm->error), "Modulo by zero");
                vm->halted = true;
                return false;
            }
            ir_vm_push_int(vm, a.as.i % b.as.i);
            break;
        }

        case OP_NEG: {
            IRValue a;
            if (!ir_vm_pop(vm, &a)) return false;
            if (a.type == VAL_INT) {
                ir_vm_push_int(vm, -a.as.i);
            } else if (a.type == VAL_FLOAT) {
                ir_vm_push_float(vm, -a.as.f);
            } else {
                snprintf(vm->error, sizeof(vm->error), "OP_NEG requires numeric operand");
                vm->halted = true;
                return false;
            }
            break;
        }

        // Comparison operations
        case OP_EQ: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            bool result = false;
            if (a.type == VAL_INT && b.type == VAL_INT) result = (a.as.i == b.as.i);
            else if (a.type == VAL_BOOL && b.type == VAL_BOOL) result = (a.as.b == b.as.b);
            else if (a.type == VAL_STRING && b.type == VAL_STRING)
                result = (strcmp(a.as.s, b.as.s) == 0);
            ir_vm_push_bool(vm, result);
            break;
        }

        case OP_NE: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            bool result = true;
            if (a.type == VAL_INT && b.type == VAL_INT) result = (a.as.i != b.as.i);
            else if (a.type == VAL_BOOL && b.type == VAL_BOOL) result = (a.as.b != b.as.b);
            else if (a.type == VAL_STRING && b.type == VAL_STRING)
                result = (strcmp(a.as.s, b.as.s) != 0);
            ir_vm_push_bool(vm, result);
            break;
        }

        case OP_LT: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_INT || b.type != VAL_INT) {
                snprintf(vm->error, sizeof(vm->error), "OP_LT requires integer operands");
                vm->halted = true;
                return false;
            }
            ir_vm_push_bool(vm, a.as.i < b.as.i);
            break;
        }

        case OP_GT: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_INT || b.type != VAL_INT) {
                snprintf(vm->error, sizeof(vm->error), "OP_GT requires integer operands");
                vm->halted = true;
                return false;
            }
            ir_vm_push_bool(vm, a.as.i > b.as.i);
            break;
        }

        case OP_LE: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_INT || b.type != VAL_INT) {
                snprintf(vm->error, sizeof(vm->error), "OP_LE requires integer operands");
                vm->halted = true;
                return false;
            }
            ir_vm_push_bool(vm, a.as.i <= b.as.i);
            break;
        }

        case OP_GE: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_INT || b.type != VAL_INT) {
                snprintf(vm->error, sizeof(vm->error), "OP_GE requires integer operands");
                vm->halted = true;
                return false;
            }
            ir_vm_push_bool(vm, a.as.i >= b.as.i);
            break;
        }

        // Logical operations
        case OP_AND: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_BOOL || b.type != VAL_BOOL) {
                snprintf(vm->error, sizeof(vm->error), "OP_AND requires boolean operands");
                vm->halted = true;
                return false;
            }
            ir_vm_push_bool(vm, a.as.b && b.as.b);
            break;
        }

        case OP_OR: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_BOOL || b.type != VAL_BOOL) {
                snprintf(vm->error, sizeof(vm->error), "OP_OR requires boolean operands");
                vm->halted = true;
                return false;
            }
            ir_vm_push_bool(vm, a.as.b || b.as.b);
            break;
        }

        case OP_NOT: {
            IRValue a;
            if (!ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_BOOL) {
                snprintf(vm->error, sizeof(vm->error), "OP_NOT requires boolean operand");
                vm->halted = true;
                return false;
            }
            ir_vm_push_bool(vm, !a.as.b);
            break;
        }

        // String operations
        case OP_CONCAT: {
            IRValue b, a;
            if (!ir_vm_pop(vm, &b) || !ir_vm_pop(vm, &a)) return false;
            if (a.type != VAL_STRING || b.type != VAL_STRING) {
                snprintf(vm->error, sizeof(vm->error), "OP_CONCAT requires string operands");
                vm->halted = true;
                return false;
            }
            size_t len = strlen(a.as.s) + strlen(b.as.s) + 1;
            char* result = (char*)malloc(len);
            strcpy(result, a.as.s);
            strcat(result, b.as.s);
            ir_vm_push_string(vm, result);
            free(result);
            free(a.as.s);
            free(b.as.s);
            break;
        }

        // State management
        case OP_GET_STATE: {
            // Read 4-byte state ID (little-endian)
            if (vm->pc + 4 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }

            uint32_t state_id = 0;
            memcpy(&state_id, &vm->code[vm->pc], 4);
            vm->pc += 4;

            IRValue value;
            ir_vm_get_state(vm, state_id, &value);

            // Push value onto stack
            if (vm->stack.sp >= VM_STACK_MAX - 1) {
                snprintf(vm->error, sizeof(vm->error), "Stack overflow");
                vm->halted = true;
                return false;
            }
            vm->stack.sp++;
            vm->stack.stack[vm->stack.sp] = value;
            break;
        }

        case OP_SET_STATE: {
            if (vm->pc + 4 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            uint32_t state_id = 0;
            memcpy(&state_id, &vm->code[vm->pc], 4);
            vm->pc += 4;
            IRValue value;
            if (!ir_vm_pop(vm, &value)) return false;
            if (!ir_vm_set_state(vm, state_id, value)) return false;
            break;
        }

        // Local variables
        case OP_GET_LOCAL: {
            if (vm->pc + 4 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            uint32_t local_id = 0;
            memcpy(&local_id, &vm->code[vm->pc], 4);
            vm->pc += 4;
            IRValue value;
            ir_vm_get_local(vm, local_id, &value);
            ir_vm_push(vm, value);
            break;
        }

        case OP_SET_LOCAL: {
            if (vm->pc + 4 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            uint32_t local_id = 0;
            memcpy(&local_id, &vm->code[vm->pc], 4);
            vm->pc += 4;
            IRValue value;
            if (!ir_vm_pop(vm, &value)) return false;
            if (!ir_vm_set_local(vm, local_id, value)) return false;
            break;
        }

        // Control flow
        case OP_JUMP: {
            if (vm->pc + 4 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            int32_t offset = 0;
            memcpy(&offset, &vm->code[vm->pc], 4);
            vm->pc += offset;  // Jump is relative
            break;
        }

        case OP_JUMP_IF_FALSE: {
            if (vm->pc + 4 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            int32_t offset = 0;
            memcpy(&offset, &vm->code[vm->pc], 4);
            vm->pc += 4;
            IRValue cond;
            if (!ir_vm_pop(vm, &cond)) return false;
            if (cond.type != VAL_BOOL) {
                snprintf(vm->error, sizeof(vm->error), "OP_JUMP_IF_FALSE requires boolean condition");
                vm->halted = true;
                return false;
            }
            if (!cond.as.b) {
                vm->pc += offset - 4;  // Offset from after the instruction
            }
            break;
        }

        case OP_CALL: {
            // TODO: Implement function call stack
            snprintf(vm->error, sizeof(vm->error), "OP_CALL not yet implemented");
            vm->halted = true;
            return false;
        }

        case OP_RETURN: {
            // TODO: Implement function call stack
            snprintf(vm->error, sizeof(vm->error), "OP_RETURN not yet implemented");
            vm->halted = true;
            return false;
        }

        // Host interaction
        case OP_CALL_HOST: {
            if (vm->pc + 4 > vm->code_size) {
                snprintf(vm->error, sizeof(vm->error), "Unexpected end of bytecode");
                vm->halted = true;
                return false;
            }
            uint32_t func_id = 0;
            memcpy(&func_id, &vm->code[vm->pc], 4);
            vm->pc += 4;
            if (!ir_vm_call_host_function(vm, func_id)) {
                // Graceful degradation - continue execution
                fprintf(stderr, "[VM] Host function %u not available, continuing\n", func_id);
            }
            break;
        }

        case OP_GET_PROP: {
            // TODO: Implement component property access
            snprintf(vm->error, sizeof(vm->error), "OP_GET_PROP not yet implemented");
            vm->halted = true;
            return false;
        }

        case OP_SET_PROP: {
            // TODO: Implement component property access
            snprintf(vm->error, sizeof(vm->error), "OP_SET_PROP not yet implemented");
            vm->halted = true;
            return false;
        }

        case OP_HALT: {
            vm->halted = true;
            break;
        }

        default: {
            snprintf(vm->error, sizeof(vm->error), "Unknown opcode: 0x%02X at pc=%d", op, vm->pc - 1);
            vm->halted = true;
            return false;
        }
    }

    return true;
}

// Debugging

void ir_vm_print_stack(IRVM* vm) {
    printf("Stack (sp=%d):\n", vm->stack.sp);
    for (int i = 0; i <= vm->stack.sp; i++) {
        IRValue* v = &vm->stack.stack[i];
        printf("  [%d] ", i);
        switch (v->type) {
            case VAL_INT:
                printf("INT: %lld\n", (long long)v->as.i);
                break;
            case VAL_FLOAT:
                printf("FLOAT: %f\n", v->as.f);
                break;
            case VAL_STRING:
                printf("STRING: \"%s\"\n", v->as.s ? v->as.s : "(null)");
                break;
            case VAL_BOOL:
                printf("BOOL: %s\n", v->as.b ? "true" : "false");
                break;
        }
    }
}

void ir_vm_print_state(IRVM* vm) {
    printf("State (%d entries):\n", vm->state.count);
    for (int i = 0; i < vm->state.count; i++) {
        IRStateEntry* e = &vm->state.entries[i];
        printf("  [%u] ", e->id);
        switch (e->value.type) {
            case VAL_INT:
                printf("INT: %lld\n", (long long)e->value.as.i);
                break;
            case VAL_FLOAT:
                printf("FLOAT: %f\n", e->value.as.f);
                break;
            case VAL_STRING:
                printf("STRING: \"%s\"\n", e->value.as.s ? e->value.as.s : "(null)");
                break;
            case VAL_BOOL:
                printf("BOOL: %s\n", e->value.as.b ? "true" : "false");
                break;
        }
    }
}

const char* ir_vm_opcode_name(IROpcode op) {
    switch (op) {
        // Stack operations
        case OP_PUSH_INT: return "PUSH_INT";
        case OP_PUSH_FLOAT: return "PUSH_FLOAT";
        case OP_PUSH_STRING: return "PUSH_STRING";
        case OP_PUSH_BOOL: return "PUSH_BOOL";
        case OP_POP: return "POP";
        case OP_DUP: return "DUP";
        // Arithmetic
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_MOD: return "MOD";
        case OP_NEG: return "NEG";
        // Comparison
        case OP_EQ: return "EQ";
        case OP_NE: return "NE";
        case OP_LT: return "LT";
        case OP_GT: return "GT";
        case OP_LE: return "LE";
        case OP_GE: return "GE";
        // Logical
        case OP_AND: return "AND";
        case OP_OR: return "OR";
        case OP_NOT: return "NOT";
        // String
        case OP_CONCAT: return "CONCAT";
        // State
        case OP_GET_STATE: return "GET_STATE";
        case OP_SET_STATE: return "SET_STATE";
        case OP_GET_LOCAL: return "GET_LOCAL";
        case OP_SET_LOCAL: return "SET_LOCAL";
        // Control flow
        case OP_JUMP: return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_CALL: return "CALL";
        case OP_RETURN: return "RETURN";
        // Host
        case OP_CALL_HOST: return "CALL_HOST";
        case OP_GET_PROP: return "GET_PROP";
        case OP_SET_PROP: return "SET_PROP";
        // System
        case OP_HALT: return "HALT";
        default: return "UNKNOWN";
    }
}

const char* ir_vm_value_type_name(IRValueType type) {
    switch (type) {
        case VAL_INT: return "INT";
        case VAL_FLOAT: return "FLOAT";
        case VAL_STRING: return "STRING";
        case VAL_BOOL: return "BOOL";
        default: return "UNKNOWN";
    }
}
