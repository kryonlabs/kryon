## Kryon Bytecode Module
## Provides Nim bindings for bytecode compilation and manipulation

import ir_core

# ============================================================================
# C Bindings - IR VM
# ============================================================================

type
  IROpcode* {.importc, header: "ir_vm.h".} = enum
    # Stack operations
    OP_PUSH_INT = 0x01
    OP_PUSH_FLOAT = 0x02
    OP_PUSH_STRING = 0x03
    OP_PUSH_BOOL = 0x04
    OP_POP = 0x05
    OP_DUP = 0x06

    # Arithmetic
    OP_ADD = 0x10
    OP_SUB = 0x11
    OP_MUL = 0x12
    OP_DIV = 0x13
    OP_MOD = 0x14
    OP_NEG = 0x15

    # Comparison
    OP_EQ = 0x20
    OP_NE = 0x21
    OP_LT = 0x22
    OP_GT = 0x23
    OP_LE = 0x24
    OP_GE = 0x25

    # Logical
    OP_AND = 0x30
    OP_OR = 0x31
    OP_NOT = 0x32

    # String
    OP_CONCAT = 0x40

    # State management
    OP_GET_STATE = 0x50
    OP_SET_STATE = 0x51
    OP_GET_LOCAL = 0x52
    OP_SET_LOCAL = 0x53

    # Control flow
    OP_JUMP = 0x60
    OP_JUMP_IF_FALSE = 0x61
    OP_CALL = 0x62
    OP_RETURN = 0x63

    # Host interaction
    OP_CALL_HOST = 0x70
    OP_GET_PROP = 0x71
    OP_SET_PROP = 0x72

    # Special
    OP_HALT = 0xFF

# ============================================================================
# C Bindings - IR Metadata
# ============================================================================

type
  IRStateType* {.importc, header: "ir_metadata.h".} = enum
    IR_STATE_TYPE_INT
    IR_STATE_TYPE_FLOAT
    IR_STATE_TYPE_STRING
    IR_STATE_TYPE_BOOL

  IRBytecodeInstructionArg* {.importc, header: "ir_metadata.h", union.} = object
    int_arg*: int64
    float_arg*: cdouble
    string_arg*: cstring
    bool_arg*: bool
    id_arg*: uint32
    offset_arg*: int32

  IRBytecodeInstruction* {.importc, header: "ir_metadata.h".} = object
    opcode*: IROpcode
    arg*: IRBytecodeInstructionArg
    has_arg*: bool

  IRBytecodeFunction* {.importc, header: "ir_metadata.h".} = object
    id*: uint32
    name*: cstring
    instructions*: ptr IRBytecodeInstruction
    instruction_count*: cint
    instruction_capacity*: cint

  IRStateDefinition* {.importc, header: "ir_metadata.h".} = object
    id*: uint32
    name*: cstring
    # Note: type and initial_value fields exist but require union handling

  IRHostFunctionDeclaration* {.importc, header: "ir_metadata.h".} = object
    id*: uint32
    name*: cstring
    signature*: cstring
    required*: bool

  IRMetadata* {.importc: "struct IRMetadata", header: "ir_metadata.h", incompletestruct.} = object

# C function bindings
{.push importc, header: "ir_metadata.h".}

proc ir_metadata_create*(): ptr IRMetadata
proc ir_metadata_destroy*(metadata: ptr IRMetadata)

proc ir_metadata_add_function*(metadata: ptr IRMetadata, id: uint32, name: cstring): ptr IRBytecodeFunction
proc ir_function_add_instruction*(function: ptr IRBytecodeFunction, instr: IRBytecodeInstruction): bool

proc ir_instr_simple*(opcode: IROpcode): IRBytecodeInstruction
proc ir_instr_int*(opcode: IROpcode, arg: int64): IRBytecodeInstruction
proc ir_instr_float*(opcode: IROpcode, arg: cdouble): IRBytecodeInstruction
proc ir_instr_string*(opcode: IROpcode, arg: cstring): IRBytecodeInstruction
proc ir_instr_bool*(opcode: IROpcode, arg: bool): IRBytecodeInstruction
proc ir_instr_id*(opcode: IROpcode, arg: uint32): IRBytecodeInstruction
proc ir_instr_offset*(opcode: IROpcode, arg: int32): IRBytecodeInstruction

proc ir_metadata_add_state_int*(metadata: ptr IRMetadata, id: uint32, name: cstring, initial_value: int64): ptr IRStateDefinition
proc ir_metadata_add_state_float*(metadata: ptr IRMetadata, id: uint32, name: cstring, initial_value: cdouble): ptr IRStateDefinition
proc ir_metadata_add_state_string*(metadata: ptr IRMetadata, id: uint32, name: cstring, initial_value: cstring): ptr IRStateDefinition
proc ir_metadata_add_state_bool*(metadata: ptr IRMetadata, id: uint32, name: cstring, initial_value: bool): ptr IRStateDefinition

proc ir_metadata_add_host_function*(metadata: ptr IRMetadata, id: uint32, name: cstring, signature: cstring, required: bool): ptr IRHostFunctionDeclaration

proc ir_metadata_find_function*(metadata: ptr IRMetadata, id: uint32): ptr IRBytecodeFunction
proc ir_metadata_find_state*(metadata: ptr IRMetadata, id: uint32): ptr IRStateDefinition
proc ir_metadata_find_host_function*(metadata: ptr IRMetadata, id: uint32): ptr IRHostFunctionDeclaration

{.pop.}

# ============================================================================
# High-level Nim API
# ============================================================================

type
  BytecodeBuilder* = ref object
    ## High-level builder for creating bytecode functions
    metadata: ptr IRMetadata
    function: ptr IRBytecodeFunction
    nextStateId: uint32
    nextFunctionId: uint32
    nextHostFunctionId: uint32

proc newBytecodeBuilder*(metadata: ptr IRMetadata = nil): BytecodeBuilder =
  ## Create a new bytecode builder
  result = BytecodeBuilder(
    metadata: if metadata.isNil: ir_metadata_create() else: metadata,
    nextStateId: 1,
    nextFunctionId: 1,
    nextHostFunctionId: 1
  )

proc getMetadata*(builder: BytecodeBuilder): ptr IRMetadata =
  ## Get the underlying metadata object
  builder.metadata

proc destroy*(builder: BytecodeBuilder) =
  ## Destroy the builder and free metadata
  if not builder.metadata.isNil:
    ir_metadata_destroy(builder.metadata)
    builder.metadata = nil

proc beginFunction*(builder: BytecodeBuilder, name: string): BytecodeBuilder =
  ## Start building a new function
  builder.function = ir_metadata_add_function(
    builder.metadata,
    builder.nextFunctionId,
    name.cstring
  )
  inc builder.nextFunctionId
  result = builder

proc endFunction*(builder: BytecodeBuilder): BytecodeBuilder =
  ## Finish building current function
  builder.function = nil
  result = builder

# ============================================================================
# Instruction Builders (Fluent API)
# ============================================================================

template addInstr(builder: BytecodeBuilder, instr: IRBytecodeInstruction) =
  if not builder.function.isNil:
    discard ir_function_add_instruction(builder.function, instr)

proc pushInt*(builder: BytecodeBuilder, value: int64): BytecodeBuilder =
  builder.addInstr(ir_instr_int(OP_PUSH_INT, value))
  result = builder

proc pushFloat*(builder: BytecodeBuilder, value: float): BytecodeBuilder =
  builder.addInstr(ir_instr_float(OP_PUSH_FLOAT, value.cdouble))
  result = builder

proc pushString*(builder: BytecodeBuilder, value: string): BytecodeBuilder =
  builder.addInstr(ir_instr_string(OP_PUSH_STRING, value.cstring))
  result = builder

proc pushBool*(builder: BytecodeBuilder, value: bool): BytecodeBuilder =
  builder.addInstr(ir_instr_bool(OP_PUSH_BOOL, value))
  result = builder

proc pop*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_POP))
  result = builder

proc dup*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_DUP))
  result = builder

# Arithmetic
proc add*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_ADD))
  result = builder

proc sub*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_SUB))
  result = builder

proc mul*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_MUL))
  result = builder

proc `div`*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_DIV))
  result = builder

proc `mod`*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_MOD))
  result = builder

proc neg*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_NEG))
  result = builder

# Comparison
proc eq*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_EQ))
  result = builder

proc ne*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_NE))
  result = builder

proc lt*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_LT))
  result = builder

proc gt*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_GT))
  result = builder

proc le*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_LE))
  result = builder

proc ge*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_GE))
  result = builder

# Logical
proc `and`*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_AND))
  result = builder

proc `or`*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_OR))
  result = builder

proc `not`*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_NOT))
  result = builder

# String
proc concat*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_CONCAT))
  result = builder

# State management
proc getState*(builder: BytecodeBuilder, stateId: uint32): BytecodeBuilder =
  builder.addInstr(ir_instr_id(OP_GET_STATE, stateId))
  result = builder

proc setState*(builder: BytecodeBuilder, stateId: uint32): BytecodeBuilder =
  builder.addInstr(ir_instr_id(OP_SET_STATE, stateId))
  result = builder

proc getLocal*(builder: BytecodeBuilder, localId: uint32): BytecodeBuilder =
  builder.addInstr(ir_instr_id(OP_GET_LOCAL, localId))
  result = builder

proc setLocal*(builder: BytecodeBuilder, localId: uint32): BytecodeBuilder =
  builder.addInstr(ir_instr_id(OP_SET_LOCAL, localId))
  result = builder

# Control flow
proc jump*(builder: BytecodeBuilder, offset: int32): BytecodeBuilder =
  builder.addInstr(ir_instr_offset(OP_JUMP, offset))
  result = builder

proc jumpIfFalse*(builder: BytecodeBuilder, offset: int32): BytecodeBuilder =
  builder.addInstr(ir_instr_offset(OP_JUMP_IF_FALSE, offset))
  result = builder

proc call*(builder: BytecodeBuilder, functionId: uint32): BytecodeBuilder =
  builder.addInstr(ir_instr_id(OP_CALL, functionId))
  result = builder

proc `return`*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_RETURN))
  result = builder

# Host interaction
proc callHost*(builder: BytecodeBuilder, hostFunctionId: uint32): BytecodeBuilder =
  builder.addInstr(ir_instr_id(OP_CALL_HOST, hostFunctionId))
  result = builder

proc getProp*(builder: BytecodeBuilder, propId: uint32): BytecodeBuilder =
  builder.addInstr(ir_instr_id(OP_GET_PROP, propId))
  result = builder

proc setProp*(builder: BytecodeBuilder, propId: uint32): BytecodeBuilder =
  builder.addInstr(ir_instr_id(OP_SET_PROP, propId))
  result = builder

# Special
proc halt*(builder: BytecodeBuilder): BytecodeBuilder =
  builder.addInstr(ir_instr_simple(OP_HALT))
  result = builder

# ============================================================================
# State Management
# ============================================================================

proc addStateInt*(builder: BytecodeBuilder, name: string, initialValue: int64): uint32 =
  ## Add an integer state and return its ID
  result = builder.nextStateId
  discard ir_metadata_add_state_int(builder.metadata, result, name.cstring, initialValue)
  inc builder.nextStateId

proc addStateFloat*(builder: BytecodeBuilder, name: string, initialValue: float): uint32 =
  ## Add a float state and return its ID
  result = builder.nextStateId
  discard ir_metadata_add_state_float(builder.metadata, result, name.cstring, initialValue.cdouble)
  inc builder.nextStateId

proc addStateString*(builder: BytecodeBuilder, name: string, initialValue: string): uint32 =
  ## Add a string state and return its ID
  result = builder.nextStateId
  discard ir_metadata_add_state_string(builder.metadata, result, name.cstring, initialValue.cstring)
  inc builder.nextStateId

proc addStateBool*(builder: BytecodeBuilder, name: string, initialValue: bool): uint32 =
  ## Add a boolean state and return its ID
  result = builder.nextStateId
  discard ir_metadata_add_state_bool(builder.metadata, result, name.cstring, initialValue)
  inc builder.nextStateId

# ============================================================================
# Host Function Management
# ============================================================================

proc addHostFunction*(builder: BytecodeBuilder, name: string, signature: string, required: bool = false): uint32 =
  ## Add a host function declaration and return its ID
  result = builder.nextHostFunctionId
  discard ir_metadata_add_host_function(builder.metadata, result, name.cstring, signature.cstring, required)
  inc builder.nextHostFunctionId

# ============================================================================
# Example / Documentation
# ============================================================================

when isMainModule:
  echo "Kryon Bytecode Builder Example"
  echo "================================"
  echo ""

  # Create a builder
  let builder = newBytecodeBuilder()

  # Add a counter state
  let counterId = builder.addStateInt("counter", 0)
  echo "Created state 'counter' with ID: ", counterId

  # Build an increment function
  discard builder
    .beginFunction("increment")
    .getState(counterId)
    .pushInt(1)
    .add()
    .setState(counterId)
    .halt()
    .endFunction()

  echo "Created function 'increment'"

  # Build a conditional function
  discard builder
    .beginFunction("check_positive")
    .getState(counterId)
    .pushInt(0)
    .gt()
    .jumpIfFalse(3)
    .pushBool(true)
    .`return`()
    .pushBool(false)
    .halt()
    .endFunction()

  echo "Created function 'check_positive'"

  # Add a host function
  let logId = builder.addHostFunction("console.log", "(string) -> void")
  echo "Created host function 'console.log' with ID: ", logId

  echo ""
  echo "Metadata created successfully!"
  echo "Use ir_serialize_json_with_metadata() to serialize to .kir file"

  # Cleanup
  builder.destroy()
