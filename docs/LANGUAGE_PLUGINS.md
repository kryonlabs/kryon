# Kryon Language Plugin System

Complete guide to Kryon's extensible language plugin architecture.

## Table of Contents

- [Overview](#overview)
- [Using Language Plugins](#using-language-plugins)
- [Available Languages](#available-languages)
- [Creating Custom Language Plugins](#creating-custom-language-plugins)
- [Plugin API Reference](#plugin-api-reference)
- [Build Integration](#build-integration)
- [Troubleshooting](#troubleshooting)

---

## Overview

Kryon supports an extensible language plugin system that allows event handlers and functions to be written in different scripting languages. This enables:

- **Polyglot development** - Use the best language for each task
- **Integration** - Leverage existing ecosystems (shell scripts, Lua, Python, etc.)
- **Zero overhead** - Plugins only loaded when actually used
- **Clean architecture** - VTable-based plugin interface

### Design Principles

1. **Auto-registration** - Plugins register themselves via GCC `__attribute__((constructor))`
2. **Conditional compilation** - Language plugins only compiled when dependencies available
3. **Runtime availability checks** - Clear errors if language unavailable at runtime
4. **State synchronization** - Bidirectional communication between Kryon and scripting languages

---

## Using Language Plugins

### Syntax

Functions can specify a language using the `function "language"` syntax:

```kry
// Native Kryon function (default)
function myFunction() {
    count = count + 1
}

// Inferno shell function
function "sh" myShellFunction() {
    count=`{kryonget count}
    count=`{expr $count + 1}
    kryonset count $count
}
```

**Language Identifier:**
- Empty string `""` or omitted → Native Kryon (always available)
- `"sh"` → Inferno shell (requires `KRYON_PLUGIN_INFERNO`)

### State Access from Scripting Languages

Language plugins provide helper commands to access Kryon state:

**kryonget** - Read Kryon variable:
```sh
value=`{kryonget variableName}
echo $value
```

**kryonset** - Write Kryon variable:
```sh
kryonset variableName newValue
```

### Example: Counter with sh Language

```kry
var count = 0

function "sh" increment() {
    count=`{kryonget count}
    count=`{expr $count + 1}
    kryonset count $count
}

function "sh" reset() {
    kryonset count 0
}

App {
    Container {
        Text {
            text = "Count: {count}"
        }

        Button {
            text = "Increment"
            onClick = "increment"
        }

        Button {
            text = "Reset"
            onClick = "reset"
        }
    }
}
```

---

## Available Languages

### Native Kryon

**Identifier:** `""` (empty string or omitted)
**Availability:** Always available
**Execution:** Built-in interpreter

Native Kryon functions use the Kryon scripting syntax with support for:
- Variable assignment
- Arithmetic operations
- Conditional logic
- UI updates

**Example:**
```kry
var x = 10

function update() {
    x = x + 5
    if (x > 100) {
        x = 0
    }
}
```

### Inferno Shell (sh)

**Identifier:** `"sh"`
**Availability:** Requires `KRYON_PLUGIN_INFERNO` build flag and Inferno emu
**Execution:** `emu -r. dis/sh.dis <script>`

Inferno shell (formerly rc) provides full shell scripting capabilities:
- Process execution
- Pipelines and redirection
- Shell variables and functions
- Access to Inferno/TaijiOS utilities

**Example:**
```kry
function "sh" processFile() {
    filename=`{kryonget selectedFile}
    lines=`{cat $filename | wc -l}
    kryonset lineCount $lines
}
```

**Injected Helpers:**

The sh language plugin automatically provides:
```sh
# Read Kryon variable
kryonget() {
    # Implementation injected by plugin
}

# Write Kryon variable
kryonset() {
    # Implementation injected by plugin
}
```

---

## Creating Custom Language Plugins

### Plugin Structure

Custom language plugins implement the `KryonLanguagePlugin` interface defined in `include/language_plugins.h`.

**Minimal Example:**

```c
#include "runtime.h"
#include "language_plugins.h"

static bool my_lang_init(struct KryonRuntime *runtime) {
    // Initialize language runtime
    return true;
}

static void my_lang_shutdown(struct KryonRuntime *runtime) {
    // Clean up language runtime
}

static bool my_lang_is_available(struct KryonRuntime *runtime) {
    // Check if language is available (e.g., interpreter installed)
    return system("which my_interpreter > /dev/null 2>&1") == 0;
}

static KryonLanguageExecutionResult my_lang_execute(
    struct KryonRuntime *runtime,
    struct KryonElement *element,
    struct KryonScriptFunction *function,
    char *error_buffer,
    size_t error_buffer_size
) {
    // Execute the function
    const char *code = function->code;

    // TODO: Execute code in your language runtime
    // TODO: Handle kryonget/kryonset for state access
    // TODO: Update Kryon state if needed

    return KRYON_LANG_RESULT_SUCCESS;
}

static KryonLanguagePlugin my_language_plugin = {
    .identifier = "mylang",
    .name = "My Custom Language",
    .version = "1.0.0",
    .init = my_lang_init,
    .shutdown = my_lang_shutdown,
    .is_available = my_lang_is_available,
    .execute = my_lang_execute
};

// Auto-register on load
__attribute__((constructor))
static void register_my_language(void) {
    kryon_language_register(&my_language_plugin);
}
```

### Key Implementation Points

1. **State Synchronization**
   - Provide `kryonget`/`kryonset` equivalents for your language
   - Parse output to detect variable changes
   - Update Kryon runtime state after execution

2. **Error Handling**
   - Populate `error_buffer` with clear error messages
   - Return appropriate `KryonLanguageExecutionResult`
   - Capture stderr from language execution

3. **Availability Check**
   - Verify interpreter/runtime is installed
   - Check version compatibility if needed
   - Fail gracefully with helpful error messages

4. **Temporary Files** (if needed)
   - Create temp script files in `/tmp/kryon_XXXXXX`
   - Clean up temp files after execution
   - Use `mkstemp()` for secure file creation

---

## Plugin API Reference

### KryonLanguagePlugin Structure

```c
typedef struct KryonLanguagePlugin {
    const char *identifier;  // Unique identifier (e.g., "sh", "lua", "python")
    const char *name;        // Display name (e.g., "Inferno Shell")
    const char *version;     // Plugin version

    bool (*init)(struct KryonRuntime *runtime);
    void (*shutdown)(struct KryonRuntime *runtime);
    bool (*is_available)(struct KryonRuntime *runtime);

    KryonLanguageExecutionResult (*execute)(
        struct KryonRuntime *runtime,
        struct KryonElement *element,
        struct KryonScriptFunction *function,
        char *error_buffer,
        size_t error_buffer_size
    );
} KryonLanguagePlugin;
```

### Execution Result

```c
typedef enum {
    KRYON_LANG_RESULT_SUCCESS = 0,
    KRYON_LANG_RESULT_ERROR,
    KRYON_LANG_RESULT_NOT_AVAILABLE
} KryonLanguageExecutionResult;
```

### Registration API

```c
// Register a language plugin
bool kryon_language_register(KryonLanguagePlugin *plugin);

// Get plugin by identifier
KryonLanguagePlugin* kryon_language_get(const char *identifier);

// Check if language is available
bool kryon_language_available(struct KryonRuntime *runtime, const char *identifier);

// Execute function in specified language
KryonLanguageExecutionResult kryon_language_execute_function(
    struct KryonRuntime *runtime,
    struct KryonElement *element,
    struct KryonScriptFunction *function,
    char *error_buffer,
    size_t error_buffer_size
);
```

### Accessing Kryon State

From within plugin `execute()` implementation:

```c
// Get variable value
KryonValue *value = kryon_runtime_get_var(runtime, "variableName");

// Set variable value
KryonValue new_value;
new_value.type = KRYON_TYPE_INT;
new_value.as.int_value = 42;
kryon_runtime_set_var(runtime, "variableName", &new_value);
```

---

## Build Integration

### Adding Plugin to Build System

**1. Create plugin source file:**
```
/src/runtime/languages/my_language.c
```

**2. Update Makefile:**
```makefile
# Add to LANGUAGE_SRC conditionally
ifneq ($(filter mylang,$(PLUGINS)),)
    LANGUAGE_SRC += src/runtime/languages/my_language.c
    CFLAGS += -DKRYON_PLUGIN_MYLANG=1
endif
```

**3. Conditional compilation in plugin:**
```c
#ifdef KRYON_PLUGIN_MYLANG
// Plugin implementation
#else
// Empty stub if needed
#endif
```

**4. Build with plugin:**
```bash
make PLUGINS=mylang
# Or for multiple plugins:
make PLUGINS="inferno mylang"
```

### Example: Inferno Plugin Build

```makefile
# Automatic Inferno detection
ifneq ($(INFERNO),)
    LANGUAGE_SRC += src/runtime/languages/sh_language.c
    CFLAGS += -DKRYON_PLUGIN_INFERNO=1
    CFLAGS += -I$(INFERNO)/include
    LDFLAGS += -L$(INFERNO)/Linux/amd64/lib -l9
endif
```

---

## Troubleshooting

### Plugin Not Available at Runtime

**Error:**
```
Error: Language 'sh' is not available.
Function 'myFunction' cannot be executed.
```

**Causes:**
1. Plugin not compiled into build (missing build flag)
2. Language runtime not installed (e.g., emu not in PATH)
3. Plugin failed availability check

**Solutions:**
```bash
# Verify build includes plugin
./build/bin/kryon --version  # Should list available plugins

# Check runtime availability
which emu  # For sh language

# Rebuild with plugin enabled
make -f Makefile.inferno  # For sh language
```

### Plugin Registered but Execution Fails

**Error:**
```
Error executing function 'myFunction': <error message>
```

**Debugging:**
```bash
# Run with debug output
./build/bin/kryon run app.kry --debug

# Check for:
# - "Language plugin registered: <name>"
# - Execution error messages
# - stderr output from language runtime
```

**Common issues:**
- Syntax errors in function code
- Missing kryonget/kryonset helpers
- Runtime not in PATH
- Permissions issues with temp files

### State Not Updating

**Problem:** `kryonset` called but Kryon UI doesn't update.

**Checklist:**
1. Verify kryonset implementation parses output correctly
2. Check that kryon_runtime_set_var() is called
3. Ensure runtime->needs_update = true after state change
4. Verify variable name matches exactly (case-sensitive)

**Debug output:**
```c
fprintf(stderr, "Setting variable '%s' to '%s'\n", var_name, value);
```

### Performance Issues

**Problem:** Language plugin execution is slow.

**Optimizations:**
1. **Cache interpreter instances** instead of spawning per execution
2. **Use persistent REPL** if language supports it
3. **Batch state updates** instead of individual kryonset calls
4. **Profile execution** to find bottlenecks

---

## Future Language Plugins

Potential language plugins for future development:

- **Lua** (`"lua"`) - Lightweight embedded scripting
- **Python** (`"py"`) - Data processing and ML integration
- **JavaScript** (`"js"`) - Web ecosystem integration
- **Wren** (`"wren"`) - Fast embedded scripting
- **Scheme** (`"scm"`) - Lisp-family functional programming

See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines on submitting new language plugins.

---

## References

- [Plugin Implementation Examples](../src/runtime/languages/)
- [Plugin API Header](../include/language_plugins.h)
- [SH Language Guide](./SH_LANGUAGE_GUIDE.md)
- [BUILD.md](./BUILD.md) - Build system configuration
