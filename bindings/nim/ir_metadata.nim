0BSD## Nim bindings for IR Metadata and VM
##
## Provides access to bytecode VM and metadata management

{.passC: "-I../../ir".}

import ir_core
import ir_vm

# IR Metadata type
type
  IRMetadata* {.importc: "struct IRMetadata", header: "ir_metadata.h", incompleteStruct.} = object

  IRStateType* {.importc: "IRStateType", header: "ir_metadata.h".} = enum
    IR_STATE_TYPE_INT = 0
    IR_STATE_TYPE_FLOAT = 1
    IR_STATE_TYPE_STRING = 2
    IR_STATE_TYPE_BOOL = 3

  IRStateInitialValue* {.union, bycopy.} = object
    int_value*: int64
    float_value*: cdouble
    string_value*: cstring
    bool_value*: bool

  IRStateDefinition* {.importc: "IRStateDefinition", header: "ir_metadata.h".} = object
    id*: uint32
    name*: cstring
    `type`*: IRStateType
    initial_value*: IRStateInitialValue

  IRBytecodeFunction* {.importc: "IRBytecodeFunction", header: "ir_metadata.h".} = object
    id*: uint32
    name*: cstring
    instructions*: ptr IRBytecodeInstruction
    instruction_count*: cint
    instruction_capacity*: cint

  IRHostFunctionDeclaration* {.importc: "IRHostFunctionDeclaration", header: "ir_metadata.h".} = object
    id*: uint32
    name*: cstring

# Metadata management
proc ir_metadata_create*(): ptr IRMetadata {.importc, cdecl, header: "ir_metadata.h".}
proc ir_metadata_destroy*(metadata: ptr IRMetadata) {.importc, cdecl, header: "ir_metadata.h".}

# Function management
proc ir_metadata_add_function*(metadata: ptr IRMetadata; id: uint32; name: cstring): ptr IRBytecodeFunction {.importc, cdecl, header: "ir_metadata.h".}
proc ir_function_add_instruction*(fun: ptr IRBytecodeFunction, instr: IRBytecodeInstruction): bool {.importc, cdecl, header: "ir_metadata.h".}

# State management
proc ir_metadata_add_state_int*(metadata: ptr IRMetadata; id: uint32; name: cstring; initial_value: int64): ptr IRStateDefinition {.importc, cdecl, header: "ir_metadata.h".}
proc ir_metadata_add_state_float*(metadata: ptr IRMetadata; id: uint32; name: cstring; initial_value: cdouble): ptr IRStateDefinition {.importc, cdecl, header: "ir_metadata.h".}
proc ir_metadata_add_state_string*(metadata: ptr IRMetadata; id: uint32; name: cstring; initial_value: cstring): ptr IRStateDefinition {.importc, cdecl, header: "ir_metadata.h".}
proc ir_metadata_add_state_bool*(metadata: ptr IRMetadata; id: uint32; name: cstring; initial_value: bool): ptr IRStateDefinition {.importc, cdecl, header: "ir_metadata.h".}

# Host function management
proc ir_metadata_add_host_function*(metadata: ptr IRMetadata; id: uint32; name: cstring; signature: cstring; required: bool): ptr IRHostFunctionDeclaration {.importc, cdecl, header: "ir_metadata.h".}

# Lookup
proc ir_metadata_find_function*(metadata: ptr IRMetadata; id: uint32): ptr IRBytecodeFunction {.importc, cdecl, header: "ir_metadata.h".}
proc ir_metadata_find_state*(metadata: ptr IRMetadata; id: uint32): ptr IRStateDefinition {.importc, cdecl, header: "ir_metadata.h".}
proc ir_metadata_find_host_function*(metadata: ptr IRMetadata; id: uint32): ptr IRHostFunctionDeclaration {.importc, cdecl, header: "ir_metadata.h".}

# VM integration
proc ir_vm_load_function*(vm: ptr IRVM, fun: ptr IRBytecodeFunction): bool {.importc, cdecl, header: "ir_metadata.h".}
proc ir_vm_init_state_from_def*(vm: ptr IRVM, state: ptr IRStateDefinition): bool {.importc, cdecl, header: "ir_metadata.h".}
proc ir_vm_call_function*(vm: ptr IRVM, function_id: uint32): bool {.importc, cdecl, header: "ir_metadata.h".}
