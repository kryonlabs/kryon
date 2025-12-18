# Kryon Memory Debugging Skill

## Purpose

This skill provides a systematic workflow for debugging memory-related issues in Kryon using AddressSanitizer (ASAN) and UndefinedBehaviorSanitizer (UBSan). It auto-activates when users report crashes, segfaults, memory corruption, or related issues.

## Auto-Activation Criteria

This skill activates when the user's message contains any of these keywords:

**Crash Keywords:**
- SIGSEGV, segfault, segmentation fault, crash, core dump, illegal storage access

**Memory Errors:**
- use-after-free, double free, heap corruption, buffer overflow, stack overflow
- memory corruption, corrupted memory, invalid memory

**Pointer Issues:**
- NULL pointer, null dereference, invalid pointer, dangling pointer, wild pointer

**Memory Leaks:**
- memory leak, leaking memory, memory not freed

**General:**
- ASAN, AddressSanitizer, sanitizer, valgrind

## Six-Step ASAN Debugging Workflow

When this skill activates, guide the user through these steps:

### Step 1: Build with ASAN

Build the entire Kryon stack with AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
make asan-full
```

**What this does:**
- Cleans and rebuilds IR library with ASAN+UBSan
- Cleans and rebuilds Desktop backend with ASAN+UBSan
- Rebuilds Nim CLI linking against ASAN-instrumented libraries
- Preserves pkg-config CFLAGS (HarfBuzz, FreeType, etc.)

**Expected output:**
```
🔍 Building Kryon with ASAN+UBSan (full stack)...
Step 1/3: Building IR library with ASAN+UBSan...
Step 2/3: Building Desktop backend with ASAN+UBSan...
Step 3/3: Building Nim CLI with ASAN-instrumented libraries...
✅ ASAN build complete!
```

### Step 2: Run with ASAN

Run the failing example or reproduce the crash:

```bash
# Basic run
LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH ./run_example.sh <example_name>

# With leak detection
ASAN_OPTIONS=detect_leaks=1 \
  LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
  ./run_example.sh <example_name>

# Continue after first error (find all issues)
ASAN_OPTIONS=halt_on_error=0 \
  LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
  ./run_example.sh <example_name>

# Save to log file
ASAN_OPTIONS=log_path=/tmp/asan.log \
  LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
  ./run_example.sh <example_name>

# Or use automated test
make asan-test EXAMPLE=<example_name>
```

**Key ASAN Options:**
- `detect_leaks=1` - Enable leak detection (default: enabled on Linux)
- `halt_on_error=0` - Continue after first error to find all issues
- `log_path=/tmp/asan.log` - Save output to file instead of stderr
- `print_stats=1` - Print statistics at exit

### Step 3: Parse ASAN Output

ASAN will report errors in this format:

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x603000000010
    #0 0x4567890 in function_name file.c:123
    #1 0x4567abc in caller_function file.c:456
    ...
```

**Key information to extract:**
1. **Error type** (first line): heap-use-after-free, buffer-overflow, etc.
2. **Memory address** (first line): Where the error occurred
3. **Stack trace** (#0, #1, ...): Call stack leading to error
4. **Allocation/deallocation traces**: Where memory was allocated/freed

If the helper script exists, use it:

```bash
./scripts/parse_asan.sh /tmp/asan_output.txt
```

### Step 4: Identify Root Cause

Match the error type to common patterns:

| Error Type | Typical Cause | Where to Look |
|------------|---------------|---------------|
| **heap-use-after-free** | Accessing freed memory | Component lifecycle, cleanup order |
| **heap-buffer-overflow** | Array index out of bounds | Array accesses, string operations |
| **stack-buffer-overflow** | Local array too small | Local buffers, sprintf/strcpy |
| **double-free** | Freeing same pointer twice | Cleanup functions, error paths |
| **memory leak** | Missing free() call | Component destructors, error returns |
| **global-buffer-overflow** | Static array overflow | Global buffers, constants |
| **stack-use-after-scope** | Using local after function returns | Returning pointers to locals |
| **initialization-order-fiasco** | Wrong global init order | Static initialization |

**Example Analysis:**

```
ERROR: AddressSanitizer: heap-use-after-free on address 0x603000000010
    #0 in ir_get_style backends/desktop/desktop_rendering.c:442
    #1 in render_component backends/desktop/desktop_rendering.c:1205
```

**Diagnosis:**
- Component was freed but still being accessed
- Look for: Tab switching, hot reload, component removal
- Check: Cleanup order, NULL checks before access

### Step 5: Apply Fix

Based on error type, apply targeted fixes:

#### Use-After-Free
```c
// BEFORE (BROKEN)
IRComponent* comp = get_component();
free_component(comp);
use_component(comp);  // ❌ Use after free!

// AFTER (FIXED)
IRComponent* comp = get_component();
if (comp) {
    use_component(comp);
    free_component(comp);
    comp = NULL;  // Prevent future access
}
```

#### Buffer Overflow
```c
// BEFORE (BROKEN)
char buffer[10];
sprintf(buffer, "Very long string here");  // ❌ Overflow!

// AFTER (FIXED)
char buffer[256];
snprintf(buffer, sizeof(buffer), "Very long string here");
```

#### Double Free
```c
// BEFORE (BROKEN)
free(ptr);
if (error) {
    free(ptr);  // ❌ Double free!
}

// AFTER (FIXED)
free(ptr);
ptr = NULL;
if (error) {
    if (ptr) free(ptr);  // Won't execute
}
```

#### Memory Leak
```c
// BEFORE (BROKEN)
char* data = malloc(100);
if (error) return;  // ❌ Leak on error path!

// AFTER (FIXED)
char* data = malloc(100);
if (error) {
    free(data);  // Cleanup before return
    return;
}
```

### Step 6: Verify Fix

After applying the fix:

1. **Rebuild with ASAN:**
   ```bash
   make asan-full
   ```

2. **Run the test again:**
   ```bash
   make asan-test EXAMPLE=<example_name>
   ```

3. **Confirm no errors:**
   ```
   ✅ No ASAN errors detected
   ```

4. **Test edge cases:**
   - Run with `detect_leaks=1` to check for leaks
   - Test multiple iterations to catch intermittent issues
   - Try different examples to ensure fix doesn't break others

5. **Clean up ASAN build:**
   ```bash
   make clean-asan
   make build  # Regular build for production
   ```

## Real Examples from Kryon Development

### Example 1: Tab Switching Use-After-Free

**Symptom:** Crash when switching between tabs

**ASAN Output:**
```
ERROR: AddressSanitizer: heap-use-after-free
    #0 in ir_tabgroup_set_active_tab ir/ir_builder.c:2145
    #1 in handle_tab_click runtime.nim:456
```

**Root Cause:** Tab panels were being freed but TabGroup still referenced them

**Fix:** Added NULL checks before accessing tab panels
```nim
# BEFORE
proc setActiveTab*(groupId: int, index: int) =
  let state = tabGroupStates[groupId]
  state.activeIndex = index

# AFTER
proc setActiveTab*(groupId: int, index: int) =
  if groupId notin tabGroupStates: return  # NULL check
  let state = tabGroupStates[groupId]
  if state.isNil: return  # Additional safety
  state.activeIndex = index
```

**Verification:** Ran `make asan-test EXAMPLE=tabs_reorderable` - no errors

### Example 2: Markdown Parser Buffer Overflow

**Symptom:** Crash when parsing large markdown files

**ASAN Output:**
```
ERROR: AddressSanitizer: heap-buffer-overflow
    #0 in append_to_buffer ir/ir_markdown_parser.c:145
    #1 in parse_text_node ir/ir_markdown_parser.c:320
```

**Root Cause:** Text buffer reallocated, pointers invalidated

**Fix:** Replaced realloc with fixed-size chunk chain
```c
// BEFORE (BROKEN)
char* buffer = malloc(capacity);
// ... later ...
buffer = realloc(buffer, new_capacity);  // ❌ Invalidates pointers!

// AFTER (FIXED)
typedef struct TextBufferChunk {
    char data[8192];
    struct TextBufferChunk* next;
} TextBufferChunk;

// Chunks never move, pointers stay valid
```

**Verification:** Ran `make asan-test EXAMPLE=markdown_demo` - no errors

### Example 3: Hot Reload Memory Leak

**Symptom:** Memory usage grows on each reload

**ASAN Output:**
```
Direct leak of 4096 bytes:
    #0 in malloc
    #1 in ir_create_component ir/ir_builder.c:234
    #2 in rebuild_ui runtime.nim:789
```

**Root Cause:** Old components not freed before creating new ones

**Fix:** Added cleanup before rebuild
```nim
# BEFORE
proc hotReload*() =
  buildUI()  # ❌ Creates new components without freeing old

# AFTER
proc hotReload*() =
  if rootComponent != nil:
    ir_destroy_component(rootComponent)  # Free old tree
    rootComponent = nil
  buildUI()  # Now create new components
```

**Verification:** Ran with `ASAN_OPTIONS=detect_leaks=1` - no leaks

## Common ASAN Error Patterns

### Pattern 1: Component Lifecycle Issues

**Error:** `heap-use-after-free` in component access

**Typical Locations:**
- Tab switching
- Hot reload
- Window close
- Navigation

**Fix Strategy:**
1. Add NULL checks before accessing components
2. Ensure cleanup happens in correct order
3. Set pointers to NULL after freeing
4. Use reference counting if needed

### Pattern 2: String/Buffer Operations

**Error:** `heap-buffer-overflow` in string operations

**Typical Locations:**
- sprintf/strcpy calls
- String concatenation
- Text rendering
- Path manipulation

**Fix Strategy:**
1. Replace sprintf with snprintf
2. Use strncpy instead of strcpy
3. Always allocate buffer size + 1 for null terminator
4. Validate input lengths

### Pattern 3: Reactive State Management

**Error:** `heap-use-after-free` in reactive callbacks

**Typical Locations:**
- Button onClick handlers
- State update callbacks
- Conditional rendering
- Loop-generated components

**Fix Strategy:**
1. Copy loop variables, don't capture by reference
2. Check if component still exists before callback
3. Suspend conditionals instead of destroying
4. Clear handler tables on cleanup

### Pattern 4: Memory Leaks in Error Paths

**Error:** `Direct leak` or `Indirect leak`

**Typical Locations:**
- Early returns
- Exception handling
- Resource initialization failures
- Nested allocations

**Fix Strategy:**
1. Use defer/finally for cleanup
2. Free resources before early returns
3. Track allocations in reverse order
4. Use RAII patterns when possible

## Environment Variables Reference

### ASAN Options

Set via `ASAN_OPTIONS=option1=value:option2=value`:

| Option | Default | Description |
|--------|---------|-------------|
| `detect_leaks` | 1 (Linux) | Enable leak detection |
| `halt_on_error` | 1 | Stop after first error |
| `log_path` | stderr | Write output to file |
| `print_stats` | 0 | Print statistics at exit |
| `verbosity` | 0 | ASAN verbosity level (0-2) |
| `print_summary` | 1 | Print error summary at exit |
| `fast_unwind_on_malloc` | 1 | Faster stack traces |

**Examples:**
```bash
# Find all errors, don't stop at first
ASAN_OPTIONS=halt_on_error=0

# Save to file for analysis
ASAN_OPTIONS=log_path=/tmp/asan.log

# Maximum debugging information
ASAN_OPTIONS=verbosity=2:print_stats=1:print_summary=1
```

### UBSan Options

Set via `UBSAN_OPTIONS=option1=value:option2=value`:

| Option | Default | Description |
|--------|---------|-------------|
| `print_stacktrace` | 0 | Show stack traces for UBSan errors |
| `halt_on_error` | 0 | Stop after first UBSan error |
| `log_path` | stderr | Write output to file |

**Example:**
```bash
# Show stack traces for undefined behavior
UBSAN_OPTIONS=print_stacktrace=1
```

### Combined Usage

```bash
# Maximum debugging (ASAN + UBSan with full info)
ASAN_OPTIONS=detect_leaks=1:halt_on_error=0:verbosity=2 \
UBSAN_OPTIONS=print_stacktrace=1 \
LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
./run_example.sh example_name
```

## Troubleshooting ASAN Build Issues

### Issue: Missing Include Paths

**Symptom:**
```
fatal error: harfbuzz/hb.h: No such file or directory
```

**Cause:** ASAN targets override CFLAGS, losing pkg-config includes

**Fix:** Use `CFLAGS="$(CFLAGS) $(ASAN_FLAGS)"` in Makefiles (already fixed)

### Issue: Linking Errors

**Symptom:**
```
undefined reference to `__asan_init`
```

**Cause:** Some libraries built with ASAN, others without

**Solution:** Always use `make asan-full` to rebuild entire stack

### Issue: False Positives

**Symptom:** ASAN reports errors in third-party libraries

**Solution:** Use suppressions file (create `.asan-suppress`):
```
# Suppress leaks in SDL3
leak:libSDL3.so

# Suppress leaks in HarfBuzz
leak:libharfbuzz.so
```

Then run:
```bash
ASAN_OPTIONS=suppressions=.asan-suppress \
LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
./run_example.sh example_name
```

## Quick Reference

### Build Commands
```bash
make asan-full              # Build entire stack with ASAN+UBSan
make asan-test EXAMPLE=foo  # Run ASAN test on example
make clean-asan             # Clean ASAN build artifacts
```

### Run Commands
```bash
# Basic ASAN run
LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH ./run_example.sh foo

# With leak detection
ASAN_OPTIONS=detect_leaks=1 \
  LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH ./run_example.sh foo

# Find all errors
ASAN_OPTIONS=halt_on_error=0 \
  LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH ./run_example.sh foo

# Save to log
ASAN_OPTIONS=log_path=/tmp/asan.log \
  LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH ./run_example.sh foo
```

### Analysis Commands
```bash
# Parse ASAN output
./scripts/parse_asan.sh /tmp/asan_output.txt

# Check for specific error type
grep "heap-use-after-free" /tmp/asan_output.txt

# Extract stack traces
grep "^    #" /tmp/asan_output.txt

# Count errors
grep -c "ERROR: AddressSanitizer" /tmp/asan_output.txt
```

## Integration with Other Skills

This skill works alongside:

- **kryon-visual-debug**: For UI rendering issues (use visual debugging BEFORE ASAN for layout issues)
- **kryon-layout-inspector**: For layout calculation problems
- **kryon-screenshot-diff**: For verifying fixes don't break UI

**Decision Tree:**
1. **Visual issue (layout, rendering)?** → Use kryon-visual-debug first
2. **Crash, SIGSEGV, memory error?** → Use kryon-memory-debug (this skill)
3. **Performance, freezing, slow?** → Use timeout + trace (not this skill)

## Success Metrics

After using this skill, the user should have:

1. ✅ Successfully built Kryon with ASAN+UBSan
2. ✅ Reproduced the crash/error with ASAN
3. ✅ Identified the error type and location
4. ✅ Applied a targeted fix based on error pattern
5. ✅ Verified the fix with ASAN (no errors)
6. ✅ Understood how to use ASAN for future debugging

## When NOT to Use This Skill

- **Performance issues** - Use timeout + trace instead
- **UI layout problems** - Use kryon-visual-debug first
- **Compilation errors** - Not a memory issue
- **Logic bugs** - Use regular debugging (print statements, gdb)

ASAN is specifically for **memory safety issues**. If there's no crash/corruption, this skill is not the right tool.
