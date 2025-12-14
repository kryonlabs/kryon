## Kryon Bytecode (.krb) Format Bindings
## Provides Nim bindings for .krb compilation and disassembly

import ir_core

const
  kryonLib = "libkryon_ir.so"
  kryonHeader = "ir_krb.h"

# ============================================================================
# Types
# ============================================================================

type
  KRBHeader* {.importc, header: kryonHeader.} = object
    magic*: uint32
    version_major*: uint8
    version_minor*: uint8
    flags*: uint16
    section_count*: uint32
    entry_function*: uint32
    checksum*: uint32
    reserved*: array[12, uint8]

  KRBFunction* {.importc, header: kryonHeader.} = object
    name*: cstring
    code_offset*: uint32
    code_size*: uint32
    param_count*: uint8
    local_count*: uint8
    flags*: uint16

  KRBEventBinding* {.importc, header: kryonHeader.} = object
    component_id*: uint32
    event_type*: uint16
    function_index*: uint16

  KRBModule* {.importc, header: kryonHeader.} = object
    header*: KRBHeader
    ui_data*: ptr uint8
    ui_size*: uint32
    code_data*: ptr uint8
    code_size*: uint32
    data_data*: ptr uint8
    data_size*: uint32
    string_table*: ptr cstring
    string_count*: uint32
    functions*: ptr KRBFunction
    function_count*: uint32
    event_bindings*: ptr KRBEventBinding
    event_binding_count*: uint32
    globals*: ptr int64
    global_count*: uint32

  KRBRuntime* {.importc, header: kryonHeader.} = object
    module*: ptr KRBModule
    ip*: uint32
    # ... other fields

# ============================================================================
# C Function Bindings
# ============================================================================

# Compilation
proc krb_compile_from_kir*(kir_path: cstring): ptr KRBModule
  {.importc, dynlib: kryonLib, header: kryonHeader.}

proc krb_compile_from_ir*(root: ptr IRComponent, logic: pointer, manifest: pointer): ptr KRBModule
  {.importc, dynlib: kryonLib, header: kryonHeader.}

proc krb_write_file*(module: ptr KRBModule, path: cstring): bool
  {.importc, dynlib: kryonLib, header: kryonHeader.}

proc krb_read_file*(path: cstring): ptr KRBModule
  {.importc, dynlib: kryonLib, header: kryonHeader.}

proc krb_module_free*(module: ptr KRBModule)
  {.importc, dynlib: kryonLib, header: kryonHeader.}

# Disassembly
proc krb_disassemble*(module: ptr KRBModule): cstring
  {.importc, dynlib: kryonLib, header: kryonHeader.}

proc krb_disassemble_function*(module: ptr KRBModule, function_index: uint32): cstring
  {.importc, dynlib: kryonLib, header: kryonHeader.}

proc krb_print_info*(module: ptr KRBModule)
  {.importc, dynlib: kryonLib, header: kryonHeader.}

# Utility
proc krb_opcode_name*(op: uint8): cstring
  {.importc, dynlib: kryonLib, header: kryonHeader.}

proc krb_crc32*(data: pointer, size: csize_t): uint32
  {.importc, dynlib: kryonLib, header: kryonHeader.}

# ============================================================================
# High-Level Nim API
# ============================================================================

const
  KRB_MAGIC* = 0x5942524B'u32  # "KRBY"
  KRB_VERSION_MAJOR* = 1
  KRB_VERSION_MINOR* = 0

proc compileKirToKrb*(kirPath: string, krbPath: string): bool =
  ## Compile a .kir file to .krb bytecode
  let module = krb_compile_from_kir(kirPath.cstring)
  if module.isNil:
    return false
  let success = krb_write_file(module, krbPath.cstring)
  krb_module_free(module)
  return success

proc disassembleKrb*(krbPath: string): string =
  ## Disassemble a .krb file to text
  let module = krb_read_file(krbPath.cstring)
  if module.isNil:
    return ""
  let disasm = krb_disassemble(module)
  if disasm.isNil:
    krb_module_free(module)
    return ""
  result = $disasm
  # Note: disasm is malloc'd, would need to free it
  krb_module_free(module)

proc inspectKrb*(krbPath: string) =
  ## Print .krb file info to stdout
  let module = krb_read_file(krbPath.cstring)
  if module.isNil:
    echo "Failed to read: ", krbPath
    return
  krb_print_info(module)
  krb_module_free(module)

proc isValidKrb*(krbPath: string): bool =
  ## Check if a file is a valid .krb file
  let module = krb_read_file(krbPath.cstring)
  if module.isNil:
    return false
  let valid = module.header.magic == KRB_MAGIC
  krb_module_free(module)
  return valid
