# Hare Codegen - Complete Implementation Roadmap

**Status:** Beta (Basic codegen working, integration incomplete)
**Last Updated:** 2025-01-24

## Overview

This roadmap details all remaining work to make Hare a first-class target in Kryon, bringing it to parity with existing codegens like Lua, TSX, and C.

---

## Phase 1: Build System Integration (HIGH PRIORITY)

**Goal:** Make Hare codegen fully integrated into the CLI build process.

**Estimated Effort:** 1-2 hours

### 1.1 Update CLI Makefile

**File:** `/kryon/cli/Makefile`

**Current State (lines 74-81):**
```makefile
CODEGEN_OBJS = $(wildcard ../build/codegen_common.o) \
               $(wildcard ../codegens/kry/kry_codegen.o) \
               $(wildcard ../codegens/tsx/tsx_codegen.o) \
               $(wildcard ../codegens/lua/lua_codegen.o) \
               $(wildcard ../codegens/markdown/markdown_codegen.o) \
               $(wildcard ../codegens/c/ir_c_codegen.o) \
               $(wildcard ../codegens/kotlin/kotlin_codegen.o) \
               $(wildcard ../codegens/react_common.o)
```

**Required Change:**
```makefile
CODEGEN_OBJS = $(wildcard ../build/codegen_common.o) \
               $(wildcard ../codegens/kry/kry_codegen.o) \
               $(wildcard ../codegens/tsx/tsx_codegen.o) \
               $(wildcard ../codegens/lua/lua_codegen.o) \
               $(wildcard ../codegens/markdown/markdown_codegen.o) \
               $(wildcard ../codegens/c/ir_c_codegen.o) \
               $(wildcard ../codegens/kotlin/kotlin_codegen.o) \
               $(wildcard ../codegens/hare/hare_codegen.o) \
               $(wildcard ../codegens/react_common.o)
```

### 1.2 Add Build Target for Hare Codegen

**File:** `/kryon/cli/Makefile`

**Location:** After line 216 (after kotlin build target)

**Add:**
```makefile
# Hare codegen library
../build/hare_codegen.o:
	$(MAKE) -C ../codegens/hare
```

### 1.3 Update Install Target

**File:** `/kryon/cli/Makefile`

**Add to installation section:**
```makefile
	# Copy hare codegen library
	$(MAKE) -C ../codegens/hare install
```

### 1.4 Update Clean Target

**File:** `/kryon/cli/Makefile`

**Add to clean target:**
```makefile
	$(MAKE) -C ../codegens/hare clean
```

### 1.5 Verify Build Integration

**Test Commands:**
```bash
cd /mnt/storage/Projects/KryonLabs/kryon/cli
make clean
make
./kryon codegen
# Should show hare in usage message
```

---

## Phase 2: Hare Expression Transpiler (MEDIUM PRIORITY)

**Goal:** Implement expression transpiler for KRY → Hare syntax conversion.

**Estimated Effort:** 3-4 hours

### 2.1 Expression Transpiler Architecture

**Reference File:** `/kryon/ir/parsers/kry/expression_transpiler.c`

**Current Supported Targets:**
- Lua (line 90-267): `cJSON_to_lua_syntax()`
- JavaScript: `cJSON_to_js_syntax()`
- C: `cJSON_to_c_syntax()`

### 2.2 Implement `cJSON_to_hare_syntax()`

**File:** `/kryon/ir/parsers/kry/expression_transpiler.c`

**Function Signature:**
```c
/**
 * Convert cJSON expression to Hare syntax
 * Handles:
 * - Literals (strings, numbers, booleans, null→void)
 * - Binary operators (+, -, *, /, %, ==, !=, <, >, <=, >=, &&, ||, ^)
 * - Logical operators (&&, ||, ^, !)
 * - Property access (obj.prop)
 * - Array access (arr[index])
 * - Function calls (func(arg1, arg2))
 * - Arrow functions (fn(arg) => expr)
 * - Array literals ([1, 2, 3])
 * - Object/struct literals ({key: value})
 *
 * @param item cJSON expression node
 * @return Hare syntax string (caller must free)
 */
char* cJSON_to_hare_syntax(const cJSON* item);
```

### 2.3 Hare-Specific Syntax Mapping

| JavaScript | Hare | Notes |
|------------|------|-------|
| `null` | `void` | Hare has no null |
| `undefined` | `void` | |
| `true/false` | `true/false` | Same |
| `&&` | `&&` | Same |
| `\|\|` | `\|\|` | Same |
| `!` | `!` | Same |
| `=>` (arrow) | `fn(...) =>` | `fn(args...) type = { ... }` |
| `obj.prop` | `obj.prop` | Same |
| `arr[index]` | `arr[index]` | Same |
| `[1,2,3]` | `[1, 2, 3]` | Same (spaces required) |
| `{key:val}` | `{key = val}` | = instead of : |

### 2.4 Implementation Details

**String Literals:**
```c
// Hare uses double quotes, escape sequences
// Input: "Hello\nWorld"
// Output: "Hello\\nWorld"
```

**Arrow Functions:**
```c
// Input: x => x * 2
// Output: fn(x: *void) size = { return x * 2; }
```

**Array Literals:**
```c
// Input: [1, 2, 3]
// Output: [1, 2, 3]
```

**Object Literals:**
```c
// Input: {name: "test", count: 42}
// Output: {name = "test", count = 42}
```

### 2.5 Add to Expression Transpiler Switch

**File:** `/kryon/ir/parsers/kry/expression_transpiler.c`

**Location:** Find the expression transpiler dispatch function

**Add Case:**
```c
else if (strcmp(target_language, "hare") == 0) {
    return cJSON_to_hare_syntax(expr);
}
```

### 2.6 Update Target Language Validation

**Files to Update:**
- `/kryon/ir/parsers/kry/expression_transpiler.h`
- Any validation functions

**Add "hare" to valid target list.**

### 2.7 Test Expression Transpilation

**Test Cases:**
```json
// Literals
{"type": "literal", "value": "hello"}       → "hello"
{"type": "literal", "value": 42}            → "42"
{"type": "literal", "value": true}          → "true"
{"type": "literal", "value": null}          → "void"

// Binary ops
{"type": "binary", "op": "+", "left": 1, "right": 2}  → "1 + 2"
{"type": "binary", "op": "==", "left": "a", "right": "b"}  → "a == b"

// Arrow functions
{"type": "arrow", "args": ["x"], "body": "x * 2"}  → "fn(x: *void) size = { return x * 2; }"

// Property access
{"type": "property", "object": "obj", "property": "prop"}  → "obj.prop"
```

---

## Phase 3: Hare Codegen Unit Tests (MEDIUM PRIORITY)

**Goal:** Ensure hare_codegen correctness with comprehensive test coverage.

**Estimated Effort:** 2-3 hours

### 3.1 Create Test Directory Structure

```
kryon/tests/codegen/hare/
├── test_simple_component.kir
├── test_simple_component.expected.ha
├── test_properties.kir
├── test_properties.expected.ha
├── test_children.kir
├── test_children.expected.ha
├── test_foreach.kir
├── test_foreach.expected.ha
├── test_event_handlers.kir
├── test_event_handlers.expected.ha
├── run_tests.sh
└── README.md
```

### 3.2 Test Files to Create

**Test 1: Simple Component**

**File:** `test_simple_component.kir`
```json
{
  "type": "Component",
  "name": "TestApp",
  "root": {
    "type": "Column",
    "gap": 20,
    "children": [
      {
        "type": "Text",
        "text": "Hello, Hare!",
        "color": "#ffffff"
      }
    ]
  }
}
```

**File:** `test_simple_component.expected.ha`
```hare
// Generated from .kir by Kryon Hare Code Generator
// Do not edit manually - regenerate from source

use kryon::*;

// UI Component Tree
export fn main() void = {
    column {
        gap = 20,
        text {
            text = "Hello, Hare!",
            color = "#ffffff",
        },
    };
};
```

**Test 2: Properties**

**File:** `test_properties.kir`
```json
{
  "root": {
    "type": "Container",
    "width": "400px",
    "height": "300px",
    "padding": 20,
    "backgroundColor": "#191970",
    "borderRadius": 8,
    "opacity": 0.95
  }
}
```

**Test 3: Nested Children**

**File:** `test_children.kir`
```json
{
  "root": {
    "type": "Row",
    "gap": 10,
    "children": [
      {"type": "Button", "text": "OK"},
      {"type": "Button", "text": "Cancel"}
    ]
  }
}
```

**Test 4: ForEach Component**

**File:** `test_foreach.kir`
```json
{
  "root": {
    "type": "Column",
    "children": [
      {
        "type": "ForEach",
        "custom_data": {
          "each_source": ["item1", "item2", "item3"],
          "each_item_name": "item"
        },
        "children": [
          {"type": "Text", "text": "${item}"}
        ]
      }
    ]
  }
}
```

**Test 5: Multi-File Module**

**File:** `test_multifile.kir`
```json
{
  "imports": ["components/button", "components/input"],
  "root": {
    "type": "Column",
    "children": [
      {"type": "ComponentRef", "name": "Button"},
      {"type": "ComponentRef", "name": "Input"}
    ]
  }
}
```

### 3.3 Test Runner Script

**File:** `run_tests.sh`
```bash
#!/bin/bash
set -e

TEST_DIR="$(dirname "$0")"
BUILD_DIR="../../../build"
CODEGEN_BIN="$BUILD_DIR/hare_codegen_test"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

passed=0
failed=0

run_test() {
    local test_file="$1"
    local expected_file="$2"
    local test_name=$(basename "$test_file" .kir)

    echo "Running: $test_name"

    # Run codegen
    output=$(../../../cli/kryon codegen hare "$test_file" - 2>&1)
    if [ $? -ne 0 ]; then
        echo -e "${RED}FAILED${NC}: $test_name (codegen error)"
        echo "$output"
        ((failed++))
        return
    fi

    # Compare with expected
    if ! diff -u "$expected_file" <(echo "$output") > /dev/null 2>&1; then
        echo -e "${RED}FAILED${NC}: $test_name (output mismatch)"
        echo "Expected:"
        cat "$expected_file"
        echo "Got:"
        echo "$output"
        ((failed++))
    else
        echo -e "${GREEN}PASSED${NC}: $test_name"
        ((passed++))
    fi
}

# Run all tests
for kir_file in "$TEST_DIR"/*.kir; do
    expected_file="${kir_file%.kir}.expected.ha"
    if [ -f "$expected_file" ]; then
        run_test "$kir_file" "$expected_file"
    fi
done

echo ""
echo "Results: $passed passed, $failed failed"

if [ $failed -gt 0 ]; then
    exit 1
fi
```

### 3.4 Update Root Test Makefile

**File:** `/kryon/tests/Makefile`

**Add hare test target:**
```makefile
test-hare:
	@echo "Testing Hare codegen..."
	@cd codegen/hare && ./run_tests.sh
```

---

## Phase 4: Hare Runtime Bindings (LOW-MEDIUM PRIORITY)

**Goal:** Enable Hare applications to use Kryon runtime via FFI.

**Estimated Effort:** 4-6 hours

### 4.1 Directory Structure

```
kryon/bindings/hare/
├── README.md
├── Makefile
├── kryon/
│   ├── types.ha
│   ├── component.ha
│   ├── container.ha
│   ├── column.ha
│   ├── row.ha
│   ├── text.ha
│   ├── button.ha
│   ├── input.ha
│   ├── checkbox.ha
│   └── for_each.ha
└── examples/
    └── hello.ha
```

### 4.2 Core Type Definitions

**File:** `/kryon/bindings/hare/kryon/types.ha`
```hare
// Kryon FFI bindings for Hare
// Defines low-level types and functions for interacting with libkryon

use fmt;

// Component type enumeration
export type component_type = enum uint {
    CONTAINER = 0,
    COLUMN,
    ROW,
    TEXT,
    BUTTON,
    INPUT,
    CHECKBOX,
    TAB_GROUP,
    TAB_BAR,
    TAB_CONTENT,
    TAB_PANEL,
    FOR_EACH,
};

// Color type (RGBA)
export type color = struct {
    r: u8,
    g: u8,
    b: u8,
    a: u8,
};

// Component opaque pointer (FFI)
export type component = void;

// FFI function declarations
@symbol("kryon_component_create") export fn create_component(ctype: component_type) *component;
@symbol("kryon_component_set_width") export fn set_width(comp: *component, width: str) void;
@symbol("kryon_component_set_height") export fn set_height(comp: *component, height: str) void;
@symbol("kryon_component_set_text") export fn set_text(comp: *component, text: str) void;
@symbol("kryon_component_set_color") export fn set_color(comp: *component, color: *color) void;
@symbol("kryon_component_add_child") export fn add_child(parent: *component, child: *component) void;
@symbol("kryon_run") export fn run(root: *component) int;
```

### 4.3 High-Level Component API

**File:** `/kryon/bindings/hare/kryon/container.ha`
```hare
use kryon::types;
use kryon::component;

// Container component builder
export type container = struct {
    comp: *types::component,
};

export fn container_new() container = {
    return container {
        comp = types::create_component(types::component_type::CONTAINER),
    };
};

export fn (c: container) width(w: str) container = {
    types::set_width(c.comp, w);
    return c;
};

export fn (c: container) height(h: str) container = {
    types::set_height(c.comp, h);
    return c;
};

export fn (c: container) padding(p: uint) container = {
    // Set padding via property setter
    return c;
};

export fn (c: container) background(color: str) container = {
    types::set_color(c.comp, parse_color(color));
    return c;
};

export fn (c: container) add(child: *types::component) void = {
    types::add_child(c.comp, child);
};

export fn (c: container) build() *types::component = {
    return c.comp;
};
```

### 4.4 Example Application

**File:** `/kryon/bindings/hare/examples/hello.ha`
```hare
use fmt;
use kryon::*;
use kryon::container;
use kryon::column;
use kryon::text;

export fn main() void = {
    let col = column_new();
    col.width("400px");
    col.height("300px");
    col.background("#191970");
    col.padding(20);
    col.gap(10);

    let txt = text_new("Hello from Hare!");
    txt.color("#ffffff");

    col.add(txt.build());

    fmt::println("Running Kryon Hare application...")!;
    let result = types::run(col.build());
    fmt::println(finish::exit(result));
};
```

### 4.5 Makefile

**File:** `/kryon/bindings/hare/Makefile`
```makefile
# Hare bindings Makefile

HARE = hare
CFLAGS = -std=c99 -Wall -O2 -fPIC -D_POSIX_C_SOURCE=200809L
LDFLAGS = -L../../../build -lkryon_ir -lkryon_common -lm

# Directories
BUILD_DIR = ../../../build
INCLUDE_DIR = ../../../include
IR_DIR = ../../../ir
KRYON_DIR = ../..

# Target
LIBKRYON_HARE = libkryon_hare.a
EXAMPLES = hello

.PHONY: all clean examples test

all: $(LIBKRYON_HARE)

$(LIBKRYON_HARE):
	@echo "Hare bindings use direct FFI - no library needed"

examples:
	$(HARE) build -o hello examples/hello.ha

test:
	$(HARE) test

clean:
	rm -f $(EXAMPLES)
	rm -f *.o *.a
```

---

## Phase 5: Hare Parser (LOW PRIORITY)

**Goal:** Enable compiling Hare source code (.ha files) to KIR.

**Estimated Effort:** 6-8 hours

### 5.1 Parser Architecture

**Reference:** `/kryon/ir/parsers/lua/` and `/kryon/ir/parsers/c/`

**Two Approaches:**

**Option A: Native C Parser**
- Parse Hare syntax directly in C
- Pros: Fast, no external dependencies
- Cons: Complex, must track Hare language changes

**Option B: Hare → C transpiler (via Hare stdlib)**
- Use Hare's standard library to parse and transpile
- Pros: Leverages Hare's own parser
- Cons: Requires Hare compiler installed

**Recommended:** Option A (Native C Parser)

### 5.2 Parser File Structure

```
kryon/ir/parsers/hare/
├── hare_parser.c
├── hare_parser.h
├── hare_lexer.c
├── hare_lexer.h
├── hare_ast.c
├── hare_ast.h
├── Makefile
└── README.md
```

### 5.3 Lexer Implementation

**File:** `/kryon/ir/parsers/hare/hare_lexer.h`
```c
#ifndef HARE_LEXER_H
#define HARE_LEXER_H

#include <stdio.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_KEYWORD,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_LBRACKET,     // [
    TOKEN_RBRACKET,     // ]
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_ARROW,        // =>
    TOKEN_EQUAL,        // =
    TOKEN_COMMA,        // ,
    TOKEN_DOT,          // .
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_AMP_AMP,      // &&
    TOKEN_PIPE_PIPE,    // ||
    TOKEN_BANG,         // !
    TOKEN_EQ_EQ,        // ==
    TOKEN_BANG_EQ,      // !=
    TOKEN_LT,           // <
    TOKEN_GT,           // >
    TOKEN_LT_EQ,        // <=
    TOKEN_GT_EQ,        // >=
    TOKEN_SEMICOLON,    // ;
    TOKEN_COLON,        // :
    TOKEN_NEWLINE,
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    size_t line;
    size_t column;
} Token;

typedef struct {
    FILE* input;
    char* buffer;
    size_t buffer_size;
    size_t position;
    size_t line;
    size_t column;
} Lexer;

Lexer* lexer_create(const char* source);
void lexer_free(Lexer* lexer);
Token lexer_next_token(Lexer* lexer);
const char* token_type_name(TokenType type);

#endif
```

### 5.4 AST Node Types

**File:** `/kryon/ir/parsers/hare/hare_ast.h`
```c
#ifndef HARE_AST_H
#define HARE_AST_H

#include <cJSON.h>

typedef enum {
    AST_PROGRAM,
    AST_FN_DECL,
    AST_FN_CALL,
    AST_STRUCT_DECL,
    AST_RETURN_STMT,
    AST_IF_STMT,
    AST_FOR_STMT,
    AST_BLOCK,
    AST_EXPRESSION,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_FIELD_ACCESS,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    struct ASTNode** children;
    int child_count;
    char* value;
    cJSON* metadata;
} ASTNode;

ASTNode* ast_create(ASTNodeType type);
void ast_free(ASTNode* node);
ASTNode* ast_add_child(ASTNode* parent, ASTNode* child);
char* ast_to_json(ASTNode* root);

#endif
```

### 5.5 Parser Interface

**File:** `/kryon/ir/parsers/hare/hare_parser.h`
```c
#ifndef HARE_PARSER_H
#define HARE_PARSER_H

/**
 * Parse Hare source file and convert to KIR JSON
 *
 * @param hare_file Path to .ha source file
 * @return KIR JSON string (caller must free), or NULL on error
 */
char* hare_file_to_kir(const char* hare_file);

/**
 * Parse Hare source string and convert to KIR JSON
 *
 * @param hare_source Hare source code string
 * @return KIR JSON string (caller must free), or NULL on error
 */
char* hare_source_to_kir(const char* hare_source);

#endif
```

### 5.6 Integration with Build Pipeline

**File:** `/kryon/cli/src/utils/build.c`

**Add to `compile_source_to_kir()`:**
```c
// Hare: use native parser
if (strcmp(frontend, "hare") == 0) {
    char* json = hare_file_to_kir(source_file);
    if (!json) {
        fprintf(stderr, "Error: Failed to parse %s\n", source_file);
        return 1;
    }

    // Write to output file
    FILE* out = fopen(output_kir, "w");
    if (!out) {
        fprintf(stderr, "Error: Failed to open output file\n");
        free(json);
        return 1;
    }

    fprintf(out, "%s\n", json);
    fclose(out);
    free(json);
    return 0;
}
```

### 5.7 Supported Hare Subset

**Initial implementation should support:**

```hare
// Component creation
export fn my_component() void = {
    container {
        width = "400px",
        height = "300px",
        column {
            gap = 20,
            text {
                text = "Hello",
                color = "#fff",
            },
            button {
                text = "Click me",
            },
        },
    };
};
```

**Not initially supported (future work):**
- Control flow (if, for, match)
- Custom functions
- Variable declarations
- Type definitions
- Use statements (limited support)

---

## Phase 6: Documentation Improvements (LOW PRIORITY)

**Estimated Effort:** 1-2 hours

### 6.1 Update Feature Matrix in README

**File:** `/kryon/README.md`

**Update Expression Transpiler table:**
```
| [Hare](https://harelang.org/) | ⚠️ | Beta |
```

**Update Runtime Bindings table:**
```
| [Hare](https://harelang.org/) | ⚠️ | Beta |
```

### 6.2 Update Codegens Documentation

**File:** `/kryon-website/docs/codegens.md`

**Add to Hare section:**

#### Limitations

- **No parser yet** - Cannot compile .ha → KIR (only KIR → Hare)
- **No expression transpiler** - Expressions passed through as-is
- **No runtime** - Must use manual FFI bindings
- **Basic component support** - Advanced features limited

#### Roadmap

- [ ] Hare parser (compile .ha files)
- [ ] Expression transpiler integration
- [ ] Runtime bindings complete
- [ ] Hot reload support
- [ ] Type safety improvements

### 6.3 Create Migration Guide

**File:** `/kryon-website/docs/hare-guide.md`

```markdown
# Hare Codegen Guide

## Getting Started

## Syntax Reference

## Migration from Lua

## Migration from C

## Best Practices

## Troubleshooting
```

---

## Implementation Order (Recommended)

1. **Phase 1: Build System Integration** (Do this first!)
   - Without this, hare codegen won't be linked into the CLI
   - Quick win, high impact

2. **Phase 2: Expression Transpiler**
   - Improves codegen quality significantly
   - Enables proper expression handling

3. **Phase 3: Unit Tests**
   - Ensures correctness as we add features
   - Prevents regressions

4. **Phase 4: Runtime Bindings**
   - Enables running Hare applications
   - Required for full workflow

5. **Phase 5: Hare Parser**
   - Completes the round-trip (Hare ↔ KIR)
   - Most complex, save for last

6. **Phase 6: Documentation**
   - Can be done incrementally alongside development

---

## Dependencies Between Phases

```
Phase 1 (Build) ──────────────┐
                               │
Phase 2 (Expressions) ◄────────┤
                               │
Phase 3 (Tests) ───────────────┼──► Phase 4 (Runtime)
                               │        │
Phase 6 (Docs) ◄───────────────┴────► Phase 5 (Parser)
```

**Key:**
- Phase 1 is unblocked (can start immediately)
- Phase 2 is unblocked (can start immediately)
- Phase 3 depends on Phase 2 (test expression results)
- Phase 4 is unblocked but benefits from Phase 2
- Phase 5 is independent but lowest priority
- Phase 6 can be done anytime

---

## Success Criteria

### Phase 1 Complete When:
- [ ] `make clean && make` in cli/ succeeds with hare_codegen.o linked
- [ ] `kryon codegen hare input.kir output/` generates correct files
- [ ] No linker errors about missing hare_codegen symbols

### Phase 2 Complete When:
- [ ] All expression types transpile correctly
- [ ] Test suite passes with expression tests
- [ ] Generated code compiles with Hare compiler

### Phase 3 Complete When:
- [ ] All test cases pass
- [ ] Code coverage > 80% for hare_codegen.c
- [ ] CI/CD includes hare tests

### Phase 4 Complete When:
- [ ] Can compile and run `examples/hello.ha`
- [ ] All component types accessible from Hare
- [ ] Memory safety verified (no leaks)

### Phase 5 Complete When:
- [ ] Can parse any .ha file that codegen produces
- [ ] Round-trip: Hare → KIR → Hare produces equivalent code
- [ ] Parser error messages are helpful

---

## Quick Wins (Can be done in 30 minutes each)

1. Add hare to CODEGEN_OBJS in cli/Makefile
2. Add hare_codegen test runner script
3. Create 5 basic test .kir files
4. Update README.md with hare status
5. Add example hello.ha application

---

## Notes

- Hare language spec: https://harelang.org/specification/
- Hare standard library: https://harelang.org/documentation/
- Hare compiler: https://git.sr.ht/~ecash hare

- Use Hare's tagged unions for component types
- No null in Hare - use option types (`(void | *foo)`)
- Manual memory management - free() must be called
- Snake_case for functions, CamelCase for types
