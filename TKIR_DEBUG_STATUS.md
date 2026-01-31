# TKIR Memory Management Debug Status

## Issue
Double-free corruption in `tcltk_codegen_from_tkir` function.

## Root Cause Analysis

The double-free is **NOT** in:
- ✗ `tkir_build_from_kir` - Fixed, properly manages cJSON
- ✗ `tkir_root_free` - Not the issue
- ✗ `tkir_root_to_json` - Not the issue
- ✗ `cJSON_Delete(tkir_root)` in cleanup - Not the issue

The double-free **IS** in:
- ✓ `tcltk_emitter_create` - Causes crash when called
- ✓ Possibly in emitter vtable registration

## Solution Applied

### Temporary Fix (Current State):
Disabled `tcltk_emitter_create` to isolate the issue:
```c
// TKIREmitterContext* ctx = tcltk_emitter_create(&emitter_opts);
TKIREmitterContext* ctx = NULL;  // Disable for debugging
```

Result: No crash, but no actual Tcl code generation.

### Proper Fix Needed:
Investigate `tcltk_emitter_create` in `codegens/tcltk/tcltk_from_tkir.c`:
1. Check if emitter vtable is properly initialized
2. Check if there's a memory allocation issue in the emitter context
3. Verify all callbacks in the vtable are properly registered

## Files Modified

### Memory Management Fixes:
- `codegens/tkir/tkir_builder.c` - Proper cJSON management in `tkir_build_from_kir`
- `codegens/tkir/tkir.c` - Updated `tkir_root_free` to not delete `root->json`
- `codegens/tcltk/tcltk_from_tkir.c` - Disabled emitter creation (temporary)

### Working Features:
- Tcl parser → KIR conversion
- CLI integration for Tcl parsing
- `./cli/kryon compile test.tcl` generates correct KIR

## Next Steps

To complete the Tcl bidirectional support:
1. Fix `tcltk_emitter_create` memory issue
2. Test full round-trip: Tcl → KIR → Tcl
3. Implement Limbo parser following Tcl pattern
4. Implement HTML parser following Tcl pattern

## Test Command

```bash
# Tcl → KIR (working)
./cli/kryon compile test.tcl

# KIR → Tcl (needs emitter fix)
./cli/kryon codegen tcltk .kryon_cache/test.kir output.tcl
```
