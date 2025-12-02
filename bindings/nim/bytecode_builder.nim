0BSD## Bytecode Builder for Kryon Framework
##
## Provides a DSL for building bytecode functions manually.
## Phase 4: Simple manual bytecode construction
## Phase 5: Automatic compilation from Nim expressions (future)

import ir_core
import ir_vm
import ir_metadata

type
  BytecodeBuilder* = ref object
    ## Builder for constructing bytecode functions
    metadata*: ptr IRMetadata
    currentFunction*: ptr IRBytecodeFunction
    functionId*: uint32
    nextFunctionId*: uint32  # For auto-compilation
    stateMap*: seq[tuple[name: string, id: uint32]]  # Map state names to IDs

proc newBytecodeBuilder*(metadata: ptr IRMetadata): BytecodeBuilder =
  ## Create a new bytecode builder
  result = BytecodeBuilder(
    metadata: metadata,
    currentFunction: nil,
    functionId: 0,
    nextFunctionId: 1000,  # Start auto-generated IDs at 1000
    stateMap: @[]
  )

proc getNextFunctionId*(builder: BytecodeBuilder): uint32 =
  ## Get the next available function ID for auto-compilation
  result = builder.nextFunctionId
  builder.nextFunctionId += 1

proc registerState*(builder: BytecodeBuilder, name: string, id: uint32, initialValue: int) =
  ## Register a reactive state variable
  builder.stateMap.add((name, id))
  discard ir_metadata_add_state_int(builder.metadata, id, cstring(name), initialValue.int64)

proc findStateId(builder: BytecodeBuilder, name: string): uint32 =
  ## Find state ID by name
  for (stateName, stateId) in builder.stateMap:
    if stateName == name:
      return stateId
  raise newException(ValueError, "State not found: " & name)

proc beginFunction*(builder: BytecodeBuilder, id: uint32, name: string) =
  ## Start building a new bytecode function
  builder.functionId = id
  builder.currentFunction = ir_metadata_add_function(builder.metadata, id, cstring(name))

proc endFunction*(builder: BytecodeBuilder) =
  ## Finish building the current function
  # Add HALT instruction if not already present
  if builder.currentFunction != nil:
    discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
      opcode: OP_HALT,
      has_arg: false
    ))
  builder.currentFunction = nil

# Bytecode instruction builders

proc getState*(builder: BytecodeBuilder, stateName: string) =
  ## Add OP_GET_STATE instruction
  let stateId = builder.findStateId(stateName)
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_GET_STATE,
    has_arg: true,
    arg: IRBytecodeArg(id_arg: stateId)
  ))

proc setState*(builder: BytecodeBuilder, stateName: string) =
  ## Add OP_SET_STATE instruction
  let stateId = builder.findStateId(stateName)
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_SET_STATE,
    has_arg: true,
    arg: IRBytecodeArg(id_arg: stateId)
  ))

proc pushInt*(builder: BytecodeBuilder, value: int) =
  ## Add OP_PUSH_INT instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_PUSH_INT,
    has_arg: true,
    arg: IRBytecodeArg(int_arg: value.int64)
  ))

proc pushFloat*(builder: BytecodeBuilder, value: float) =
  ## Add OP_PUSH_FLOAT instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_PUSH_FLOAT,
    has_arg: true,
    arg: IRBytecodeArg(float_arg: value.cdouble)
  ))

proc pushBool*(builder: BytecodeBuilder, value: bool) =
  ## Add OP_PUSH_BOOL instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_PUSH_BOOL,
    has_arg: true,
    arg: IRBytecodeArg(bool_arg: value)
  ))

proc pushString*(builder: BytecodeBuilder, value: string) =
  ## Add OP_PUSH_STRING instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_PUSH_STRING,
    has_arg: true,
    arg: IRBytecodeArg(string_arg: cstring(value))
  ))

# Arithmetic operations

proc add*(builder: BytecodeBuilder) =
  ## Add OP_ADD instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_ADD,
    has_arg: false
  ))

proc sub*(builder: BytecodeBuilder) =
  ## Add OP_SUB instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_SUB,
    has_arg: false
  ))

proc mul*(builder: BytecodeBuilder) =
  ## Add OP_MUL instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_MUL,
    has_arg: false
  ))

proc divide*(builder: BytecodeBuilder) =
  ## Add OP_DIV instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_DIV,
    has_arg: false
  ))

proc negate*(builder: BytecodeBuilder) =
  ## Add OP_NEG instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_NEG,
    has_arg: false
  ))

# Comparison operations

proc equal*(builder: BytecodeBuilder) =
  ## Add OP_EQ instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_EQ,
    has_arg: false
  ))

proc notEqual*(builder: BytecodeBuilder) =
  ## Add OP_NE instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_NE,
    has_arg: false
  ))

proc lessThan*(builder: BytecodeBuilder) =
  ## Add OP_LT instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_LT,
    has_arg: false
  ))

proc greaterThan*(builder: BytecodeBuilder) =
  ## Add OP_GT instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_GT,
    has_arg: false
  ))

proc lessOrEqual*(builder: BytecodeBuilder) =
  ## Add OP_LE instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_LE,
    has_arg: false
  ))

proc greaterOrEqual*(builder: BytecodeBuilder) =
  ## Add OP_GE instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_GE,
    has_arg: false
  ))

# Logical operations

proc logicalAnd*(builder: BytecodeBuilder) =
  ## Add OP_AND instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_AND,
    has_arg: false
  ))

proc logicalOr*(builder: BytecodeBuilder) =
  ## Add OP_OR instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_OR,
    has_arg: false
  ))

proc logicalNot*(builder: BytecodeBuilder) =
  ## Add OP_NOT instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_NOT,
    has_arg: false
  ))

# Host function calls

proc callHost*(builder: BytecodeBuilder, functionId: uint32) =
  ## Add OP_CALL_HOST instruction
  discard ir_function_add_instruction(builder.currentFunction, IRBytecodeInstruction(
    opcode: OP_CALL_HOST,
    has_arg: true,
    arg: IRBytecodeArg(id_arg: functionId)
  ))

# Helper: Build common patterns

proc increment*(builder: BytecodeBuilder, stateName: string) =
  ## Helper: state += 1
  ## Generates: GET_STATE, PUSH_INT 1, ADD, SET_STATE
  builder.getState(stateName)
  builder.pushInt(1)
  builder.add()
  builder.setState(stateName)

proc decrement*(builder: BytecodeBuilder, stateName: string) =
  ## Helper: state -= 1
  ## Generates: GET_STATE, PUSH_INT 1, SUB, SET_STATE
  builder.getState(stateName)
  builder.pushInt(1)
  builder.sub()
  builder.setState(stateName)

proc addValue*(builder: BytecodeBuilder, stateName: string, value: int) =
  ## Helper: state += value
  ## Generates: GET_STATE, PUSH_INT value, ADD, SET_STATE
  builder.getState(stateName)
  builder.pushInt(value)
  builder.add()
  builder.setState(stateName)

proc toggle*(builder: BytecodeBuilder, stateName: string) =
  ## Helper: state = !state
  ## Generates: GET_STATE, NOT, SET_STATE
  builder.getState(stateName)
  builder.logicalNot()
  builder.setState(stateName)
