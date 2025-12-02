0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD0BSD## Bytecode Compiler for Kryon DSL
##
## This module provides AST-based auto-compilation of onClick handlers to bytecode.
## Simple patterns like `counter.value += 1` are detected at compile time and
## automatically compiled to bytecode functions instead of Nim procs.

import macros, tables, strutils
import ../bytecode_builder
import ../ir_metadata
import ../ir_vm

type
  CompilationResult* = object
    success*: bool
    functionId*: uint32
    errorMsg*: string

type
  CodeGenResult* = object
    success*: bool
    code*: NimNode  # Generated Nim code that builds bytecode at runtime
    errorMsg*: string

proc tryCompileOnClick*(stateRegistry: Table[string, uint32],
                        handlerBody: NimNode,
                        metadataVar: NimNode): CodeGenResult =
  ## Attempt to generate bytecode builder code for onClick handler
  ## Returns generated code that will build bytecode at runtime
  ##
  ## Supported patterns:
  ## - `counter.value += 1` (increment by literal)
  ## - `counter.value -= 1` (decrement by literal)
  ## - `counter.value = 0` (set to literal)

  # For Phase 4, we'll start with a simple pattern matcher
  # Pattern: counter.value += 1 (or similar state updates)

  if handlerBody.kind != nnkStmtList:
    return CodeGenResult(success: false, errorMsg: "Not a statement list")

  if handlerBody.len != 1:
    return CodeGenResult(success: false, errorMsg: "Multiple statements not yet supported")

  let stmt = handlerBody[0]

  # Check for: stateVar.value += literal
  if stmt.kind == nnkInfix and stmt[0].strVal in ["+=", "-=", "*=", "/="]:
    let lhs = stmt[1]  # stateVar.value
    let rhs = stmt[2]  # literal value

    if lhs.kind == nnkDotExpr and lhs[1].strVal == "value" and rhs.kind == nnkIntLit:
      let stateName = $lhs[0]

      if stateName notin stateRegistry:
        return CodeGenResult(success: false,
          errorMsg: "State '" & stateName & "' not in registry")

      # Successfully recognized pattern! Generate bytecode building code
      let stateId = stateRegistry[stateName]  # Get state ID at compile-time
      let op = stmt[0].strVal
      let literal = rhs.intVal

      # Generate unique function ID based on state ID and operation
      # Format: stateId * 100 + operation offset
      let opOffset = case op
        of "+=": 1
        of "-=": 2
        of "*=": 3
        of "/=": 4
        else: 0
      let functionId = stateId * 100 + opOffset.uint32

      # Build the code with the operator inline
      let funcIdLit = newLit(functionId)
      let funcSym = bindSym("ir_metadata_add_function")
      let addInstrSym = bindSym("ir_function_add_instruction")
      let opGetStateSym = bindSym("OP_GET_STATE")
      let opSetStateSym = bindSym("OP_SET_STATE")
      let opPushIntSym = bindSym("OP_PUSH_INT")
      let stateIdLit = newLit(stateId)
      let literalLit = newIntLitNode(literal)
      let funcName = newStrLitNode("auto_" & stateName & "_" & op)

      # Generate operation opcode symbol
      let opcodeSym = case op
        of "+=": bindSym("OP_ADD")
        of "-=": bindSym("OP_SUB")
        of "*=": bindSym("OP_MUL")
        of "/=": bindSym("OP_DIV")
        else: newEmptyNode()

      let code = quote do:
        block:
          # Manually build bytecode function with deterministic ID
          let funcPtr = `funcSym`(`metadataVar`, `funcIdLit`, cstring(`funcName`))

          # GET_STATE
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: `opGetStateSym`,
            has_arg: true,
            arg: IRBytecodeArg(id_arg: `stateIdLit`)
          ))

          # PUSH_INT
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: `opPushIntSym`,
            has_arg: true,
            arg: IRBytecodeArg(int_arg: int64(`literalLit`))
          ))

          # Arithmetic operation
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: `opcodeSym`,
            has_arg: false
          ))

          # SET_STATE
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: `opSetStateSym`,
            has_arg: true,
            arg: IRBytecodeArg(id_arg: `stateIdLit`)
          ))

          # HALT
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: OP_HALT,
            has_arg: false
          ))

          `funcIdLit`

      return CodeGenResult(success: true, code: code)

  # Check for: stateVar.value = literal
  elif stmt.kind == nnkAsgn:
    let lhs = stmt[0]
    let rhs = stmt[1]

    if lhs.kind == nnkDotExpr and lhs[1].strVal == "value" and rhs.kind == nnkIntLit:
      let stateName = $lhs[0]

      if stateName notin stateRegistry:
        return CodeGenResult(success: false,
          errorMsg: "State '" & stateName & "' not in registry")

      let stateId = stateRegistry[stateName]  # Get state ID at compile-time
      let literal = rhs.intVal

      # Generate unique function ID: stateId * 100 + 5 (for assignment)
      let functionId = stateId * 100 + 5'u32

      let funcIdLit = newLit(functionId)
      let funcSym = bindSym("ir_metadata_add_function")
      let addInstrSym = bindSym("ir_function_add_instruction")
      let opSetStateSym = bindSym("OP_SET_STATE")
      let opPushIntSym = bindSym("OP_PUSH_INT")
      let stateIdLit = newLit(stateId)
      let literalLit = newIntLitNode(literal)
      let funcName = newStrLitNode("auto_set_" & stateName)

      let code = quote do:
        block:
          # Manually build bytecode function with deterministic ID
          let funcPtr = `funcSym`(`metadataVar`, `funcIdLit`, cstring(`funcName`))

          # PUSH_INT
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: `opPushIntSym`,
            has_arg: true,
            arg: IRBytecodeArg(int_arg: int64(`literalLit`))
          ))

          # SET_STATE
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: `opSetStateSym`,
            has_arg: true,
            arg: IRBytecodeArg(id_arg: `stateIdLit`)
          ))

          # HALT
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: OP_HALT,
            has_arg: false
          ))

          `funcIdLit`

      return CodeGenResult(success: true, code: code)

  # Check for: echo "message" (host function call)
  elif stmt.kind == nnkCommand and stmt[0].kind == nnkIdent and stmt[0].strVal == "echo":
    if stmt.len < 2:
      return CodeGenResult(success: false,
        errorMsg: "echo statement requires an argument")

    let arg = stmt[1]

    # Only support string literals for now (simplest case)
    if arg.kind == nnkStrLit:
      let message = arg.strVal

      # Use a deterministic function ID for echo
      # For now, use hash of message or a fixed ID like 10000
      let functionId = 10000'u32  # Fixed ID for echo function

      let funcIdLit = newLit(functionId)
      let funcSym = bindSym("ir_metadata_add_function")
      let addInstrSym = bindSym("ir_function_add_instruction")
      let opPushStringSym = bindSym("OP_PUSH_STRING")
      let opCallHostSym = bindSym("OP_CALL_HOST")
      let messageLit = newStrLitNode(message)
      let funcName = newStrLitNode("auto_echo")

      # We also need to register the host function in metadata
      let addHostFuncSym = bindSym("ir_metadata_add_host_function")
      let echoHostId = 1'u32  # Fixed ID for "echo" host function
      let echoHostIdLit = newLit(echoHostId)

      let code = quote do:
        block:
          # Register host function declaration (if not already registered)
          discard `addHostFuncSym`(`metadataVar`, `echoHostIdLit`, cstring("echo"), cstring("(string) -> void"), false)

          # Build bytecode function
          let funcPtr = `funcSym`(`metadataVar`, `funcIdLit`, cstring(`funcName`))

          # PUSH_STRING (the message)
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: `opPushStringSym`,
            has_arg: true,
            arg: IRBytecodeArg(string_arg: cstring(`messageLit`))
          ))

          # CALL_HOST (call echo with ID 1)
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: `opCallHostSym`,
            has_arg: true,
            arg: IRBytecodeArg(id_arg: `echoHostIdLit`)
          ))

          # HALT
          discard `addInstrSym`(funcPtr, IRBytecodeInstruction(
            opcode: OP_HALT,
            has_arg: false
          ))

          `funcIdLit`

      return CodeGenResult(success: true, code: code)
    else:
      return CodeGenResult(success: false,
        errorMsg: "echo only supports string literals in auto-compilation")

  # Pattern not recognized - fall back to Nim proc
  return CodeGenResult(success: false,
    errorMsg: "Pattern not recognized for auto-compilation")
