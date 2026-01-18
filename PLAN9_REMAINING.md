# Plan 9 - Remaining Work

## ✅ Completed

**Backend (1076 lines):**
- Full backend implementation
- Event handling (mouse, keyboard, resize)
- Rendering (rect, text, line, rounded rect)
- Font management + caching
- Visual effects (gradients, shadows)
- Backend registration
- Test files

---

**Infrastructure:**
- ✅ `ir/ir_platform.h` - Platform compatibility wrapper (complete type mappings)
- ✅ `core/include/kryon.h` - Updated to use ir_platform.h
- ✅ `ir/ir_log.c` - Updated to use ir_platform.h
- ✅ `third_party/cJSON/cJSON_plan9.h` - Plan 9 compatibility layer for cJSON
- ✅ `third_party/cJSON/cJSON.c` - Updated with Plan 9 support
- ✅ mkfiles - Build system with correct include paths
- ✅ Test KIR files
- ✅ `scripts/update_ir_platform.sh` - Automation script
- ✅ Type compatibility - All stdint.h types mapped (int8_t, int16_t, int32_t, etc.)
- ✅ Bool support - Added bool/true/false definitions for Plan 9

---

## 🔨 Remaining Work

### 1. Update IR Library (Optional)
Run automation script to update remaining IR files:
```bash
./scripts/update_ir_platform.sh
```
This updates ~100 IR C files to use `ir_platform.h`. **Optional** - only needed if specific files fail to compile.

### 2. Test Build (NEXT STEP)

```bash
nix-shell
export PLAN9=/nix/store/.../plan9/
cd backends/plan9
mk                                    # Build backend
cd ../../
mk                                    # Build full stack
kryon run examples/plan9/hello.kir   # Test
```

---

## Files

**Backend:** `backends/plan9/` (7 files, 1076 lines)
```
plan9_renderer.c    396 lines  - Core, lifecycle, command execution
plan9_fonts.c       186 lines  - Font loading, caching, measurement
plan9_input.c       173 lines  - Event handling
plan9_effects.c     143 lines  - Gradients, shadows
plan9_layout.c       59 lines  - Text measurement callback
plan9_internal.h     86 lines  - Data structures
plan9_renderer.h     33 lines  - Public API
```

**Build:** mkfiles (4 files)
```
mkfile              - Root build
ir/mkfile           - IR library
cli/mkfile          - CLI tool
backends/plan9/mkfile - Backend
```

**Tests:** `examples/plan9/` (2 KIR files)
```
hello.kir           - Hello world
test_primitives.kir - Rendering test
```

---

## Known Non-Issues

**Clang errors:** IDE shows errors for `<u.h>`, `<draw.h>` because we're on Linux. Compiles fine with Plan 9's `9c`.

**Unused function:** `find_component_at_point()` in `plan9_input.c` - exists for future use, not called yet.

---

## Next Steps

1. ✅ ~~Create `ir/ir_platform.h`~~ **DONE**
2. ✅ ~~Port cJSON~~ **DONE**
3. ✅ ~~Fix mkfile linking~~ **DONE**
4. ✅ ~~Fix type compatibility (stdint.h types)~~ **DONE**
5. **Test build: `mk`** ← YOU ARE HERE
6. Fix any remaining compilation errors
7. Run `kryon run examples/plan9/hello.kir`

**Estimated remaining:** 1-3 hours (mostly debugging)
