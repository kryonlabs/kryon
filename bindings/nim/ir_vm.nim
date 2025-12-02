## Nim bindings for IR Bytecode VM
##
## Provides access to the bytecode virtual machine

{.passC: "-I../../ir".}
{.passL: "-L../../build".}

# VM types
type
  IRVM* {.importc: "struct IRVM", header: "ir_vm.h", incompleteStruct.} = object

  IROpcode* {.importc: "IROpcode", header: "ir_vm.h".} = enum
    # Stack operations (0x01-0x06)
    OP_PUSH_INT = 0x01
    OP_PUSH_FLOAT = 0x02
    OP_PUSH_STRING = 0x03
    OP_PUSH_BOOL = 0x04
    OP_POP = 0x05
    OP_DUP = 0x06

    # Arithmetic operations (0x10-0x15)
    OP_ADD = 0x10
    OP_SUB = 0x11
    OP_MUL = 0x12
    OP_DIV = 0x13
    OP_MOD = 0x14
    OP_NEG = 0x15

    # Comparison operations (0x20-0x25)
    OP_EQ = 0x20
    OP_NE = 0x21
    OP_LT = 0x22
    OP_GT = 0x23
    OP_LE = 0x24
    OP_GE = 0x25

    # Logical operations (0x30-0x32)
    OP_AND = 0x30
    OP_OR = 0x31
    OP_NOT = 0x32

    # String operations (0x40)
    OP_CONCAT = 0x40

    # State operations (0x50-0x53)
    OP_GET_STATE = 0x50
    OP_SET_STATE = 0x51
    OP_GET_LOCAL = 0x52
    OP_SET_LOCAL = 0x53

    # Control flow (0x60-0x63)
    OP_JUMP = 0x60
    OP_JUMP_IF_FALSE = 0x61
    OP_CALL = 0x62
    OP_RETURN = 0x63

    # Host operations (0x70-0x72)
    OP_CALL_HOST = 0x70
    OP_GET_PROP = 0x71
    OP_SET_PROP = 0x72

    # Special (0xFF)
    OP_HALT = 0xFF

  IRValueType* {.importc: "IRValueType", header: "ir_vm.h".} = enum
    VAL_INT = 0
    VAL_FLOAT = 1
    VAL_STRING = 2
    VAL_BOOL = 3

  # Note: These are anonymous unions in C, we create named types for Nim
  IRBytecodeArg* {.union, bycopy.} = object
    int_arg*: int64
    float_arg*: cdouble
    string_arg*: cstring
    bool_arg*: bool
    id_arg*: uint32
    offset_arg*: int32

  IRBytecodeInstruction* {.importc: "IRBytecodeInstruction", header: "ir_vm.h", bycopy.} = object
    opcode*: IROpcode
    has_arg*: bool
    arg*: IRBytecodeArg

  IRValueData* {.union, bycopy.} = object
    i*: int64
    f*: cdouble
    s*: cstring
    b*: bool

  IRValue* {.importc: "IRValue", header: "ir_vm.h", bycopy.} = object
    `type`*: IRValueType
    `as`*: IRValueData

  IRHostFunctionCallback* = proc(vm: ptr IRVM; user_data: pointer) {.cdecl.}
  IRStateChangeCallback* = proc(vm: ptr IRVM; state_id: uint32; new_value: IRValue; user_data: pointer) {.cdecl.}

# VM lifecycle
proc ir_vm_create*(): ptr IRVM {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_destroy*(vm: ptr IRVM) {.importc, cdecl, header: "ir_vm.h".}

# Execution
proc ir_vm_execute*(vm: ptr IRVM; bytecode: ptr uint8; size: cint): bool {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_step*(vm: ptr IRVM): bool {.importc, cdecl, header: "ir_vm.h".}

# Stack operations
proc ir_vm_push_int*(vm: ptr IRVM; value: int64) {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_push_float*(vm: ptr IRVM; value: cdouble) {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_push_string*(vm: ptr IRVM; value: cstring) {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_push_bool*(vm: ptr IRVM; value: bool) {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_push*(vm: ptr IRVM; value: IRValue) {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_pop*(vm: ptr IRVM; `out`: ptr IRValue): bool {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_peek*(vm: ptr IRVM): ptr IRValue {.importc, cdecl, header: "ir_vm.h".}

# State operations
proc ir_vm_get_state*(vm: ptr IRVM; state_id: uint32; `out`: ptr IRValue): bool {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_set_state*(vm: ptr IRVM; state_id: uint32; value: IRValue): bool {.importc, cdecl, header: "ir_vm.h".}

# Local variable operations
proc ir_vm_get_local*(vm: ptr IRVM; local_id: uint32; `out`: ptr IRValue): bool {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_set_local*(vm: ptr IRVM; local_id: uint32; value: IRValue): bool {.importc, cdecl, header: "ir_vm.h".}

# Host function registry
proc ir_vm_register_host_function*(vm: ptr IRVM; id: uint32; name: cstring; callback: IRHostFunctionCallback; user_data: pointer): bool {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_call_host_function*(vm: ptr IRVM; func_id: uint32): bool {.importc, cdecl, header: "ir_vm.h".}

# State change callback (Phase 3: Backend Integration)
proc ir_vm_register_state_callback*(vm: ptr IRVM; callback: IRStateChangeCallback; user_data: pointer) {.importc, cdecl, header: "ir_vm.h".}

# Debugging
proc ir_vm_print_stack*(vm: ptr IRVM) {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_print_state*(vm: ptr IRVM) {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_opcode_name*(op: IROpcode): cstring {.importc, cdecl, header: "ir_vm.h".}
proc ir_vm_value_type_name*(`type`: IRValueType): cstring {.importc, cdecl, header: "ir_vm.h".}
