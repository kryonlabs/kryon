# Parser Pipeline Unification

## Executive Summary

Unify all frontend parsers to consistently produce complete KIR with full event handler, reactive state, and logic preservation.

## Current State Analysis

### Parser Comparison Table

```
┌─────────┬──────────┬──────────┬────────────┬──────────┬───────────┐
│ Parser  │ Roundtrip│ Fidelity │ Events     │ Metadata │ Status    │
├─────────┼──────────┼──────────┼────────────┼──────────┼───────────┤
│ .kry    │ Perfect  │ 100%     │ ✅ Exprs   │ ✅ Full  │ Complete  │
│ HTML    │ Excellent│ 95%+     │ ⚠️ data-*  │ ✅ data-*│ Good      │
│ Markdown│ Excellent│ 98%      │ ❌ N/A     │ ✅ Full  │ Complete  │
│ TSX     │ Poor     │ 10%      │ ❌ Lost    │ ⚠️ Part  │ Incomplete│
│ C       │ Poor     │ 10%      │ ❌ Lost    │ ⚠️ Part  │ Incomplete│
│ Rust    │ None     │ 0%       │ ❌ Lost    │ ❌ None  │ Stub      │
│ Nim     │ None     │ N/A      │ N/A        │ N/A      │ No Parser │
│ Lua     │ None     │ N/A      │ N/A        │ N/A      │ No Parser │
└─────────┴──────────┴──────────┴────────────┴──────────┴───────────┘
```

### What Works

#### .kry Parser ✅ (Reference Implementation)

**Location:** `/mnt/storage/Projects/kryon/ir/ir_kry_parser.c` (606 lines)

**Strengths:**
- Perfect roundtrip conversion
- Preserves ALL semantic information
- Event expressions stored as source code
- Clean AST → IR pipeline

**Example:**
```kry
Button {
  text: "Click me"
  onClick: { count += 1 }
}
```

**KIR Output:**
```json
{
  "type": "Button",
  "text": "Click me",
  "events": {
    "onClick": {
      "logic_id": "logic_1",
      "source_code": "{ count += 1 }"
    }
  }
}
```

#### HTML Parser ✅ (Good Roundtrip)

**Location:** `/mnt/storage/Projects/kryon/ir/ir_html_parser.c` (564 lines)

**Strengths:**
- Uses `data-ir-*` attributes for metadata
- Preserves component types via `data-ir-type`
- Good fidelity for structural elements

**Example:**
```html
<div data-ir-type="CONTAINER" data-ir-id="1">
  <button data-ir-type="BUTTON" data-onclick-logic="logic_1">
    Click
  </button>
</div>
```

**Improvement Needed:**
- Currently doesn't preserve event handlers
- Need `data-onclick-logic` attribute support

### What's Broken

#### TSX Parser ❌ (Compile-and-Execute, Loses Logic)

**Location:** `/mnt/storage/Projects/kryon/ir/parsers/tsx/ir_tsx_parser.c`

**Current Approach:**
1. Run Bun script to compile TSX → KIR
2. Script generates KIR JSON
3. Event handlers **completely lost**

**Problem:**
```tsx
<Button onClick={() => setCount(count + 1)}>
  Click
</Button>
```

**Current KIR (broken):**
```json
{
  "type": "Button",
  "text": "Click"
  // ❌ onClick handler is GONE!
}
```

**Why:** Bun script doesn't extract/preserve event handler source code.

#### C Parser ❌ (Compile-and-Execute, Loses Source)

**Location:** `/mnt/storage/Projects/kryon/ir/parsers/c/ir_c_parser.c` (650 lines)

**Current Approach:**
1. Compile C code with gcc
2. Execute binary to generate KIR
3. Event handler functions **lost**

**Problem:**
```c
void on_click(kryon_event_t* evt) {
    count++;
}

kryon_button("Click", on_click);
```

**Current KIR:**
```json
{
  "type": "Button",
  "text": "Click"
  // ❌ on_click function body LOST
}
```

**Why:** Executes compiled binary, doesn't parse C source.

## Unified Parser Architecture

### Core Principle

**All parsers must:**
1. Parse source code (AST-based, not compile-execute)
2. Extract event handler source code
3. Preserve reactive state declarations
4. Track original language metadata
5. Output complete KIR

### Unified Pipeline

```
Source Code (.tsx, .c, .nim, .lua, etc.)
    ↓
┌───────────────────────────────────────┐
│ Language-Specific Parser              │
│ - Tokenizer/Lexer                     │
│ - AST Builder                         │
│ - Symbol Table (optional)             │
└───────────────────────────────────────┘
    ↓
┌───────────────────────────────────────┐
│ Logic Extractor                       │
│ - Find event handlers                 │
│ - Extract function bodies             │
│ - Preserve source code strings        │
│ - Track dependencies                  │
└───────────────────────────────────────┘
    ↓
┌───────────────────────────────────────┐
│ IR Builder                            │
│ - Create IRComponent tree             │
│ - Create IRLogicBlock for handlers    │
│ - Create IRReactiveManifest           │
│ - Link components ↔ logic             │
└───────────────────────────────────────┘
    ↓
KIR (Complete, Round-Trip Capable)
```

## Implementation Plan

### Phase 1: Fix TSX Parser

**Replace compile-execute with AST parsing**

**New Approach:**
Use Bun's TypeScript API to parse AST and extract logic

**File:** `/mnt/storage/Projects/kryon/ir/parsers/tsx/tsx_ast_parser.ts`

```typescript
import { parse } from '@typescript-eslint/typescript-estree';

interface LogicBlock {
    id: string;
    type: 'event_handler';
    event: string;
    componentId: number;
    language: 'typescript';
    sourceCode: string;
    dependencies: string[];
}

function extractEventHandlers(ast: any): LogicBlock[] {
    const handlers: LogicBlock[] = [];
    let logicId = 1;

    // Traverse AST looking for JSX attributes like onClick={...}
    function visit(node: any, componentId: number) {
        if (node.type === 'JSXAttribute') {
            const name = node.name.name;
            if (name.startsWith('on')) { // onClick, onHover, etc.
                const handler = node.value;
                if (handler.type === 'JSXExpressionContainer') {
                    // Extract arrow function or function call
                    const source = getSourceCode(handler.expression);
                    handlers.push({
                        id: `logic_${logicId++}`,
                        type: 'event_handler',
                        event: name,
                        componentId,
                        language: 'typescript',
                        sourceCode: source,
                        dependencies: extractDependencies(handler.expression)
                    });
                }
            }
        }

        // Recurse
        for (const key in node) {
            if (node[key] && typeof node[key] === 'object') {
                visit(node[key], componentId);
            }
        }
    }

    return handlers;
}

function getSourceCode(node: any): string {
    // Get original source code for this AST node
    // Preserve formatting, comments, etc.
    return node.getText(); // or similar API
}

function extractDependencies(node: any): string[] {
    // Find all identifiers used in expression
    // E.g., "count" in "() => setCount(count + 1)"
    const deps: string[] = [];
    // ... AST traversal to find identifiers
    return deps;
}
```

**Updated C wrapper:**
```c
// ir/parsers/tsx/ir_tsx_parser.c

char* ir_tsx_file_to_kir(const char* filepath) {
    // Run new Bun script with AST parsing
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "bun run /mnt/storage/Projects/kryon/ir/parsers/tsx/tsx_ast_parser.ts \"%s\"",
             filepath);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return NULL;

    // Read complete KIR JSON (includes logic blocks)
    char* kir = read_pipe_output(pipe);
    pclose(pipe);

    return kir;
}
```

### Phase 2: Fix C Parser

**Replace compile-execute with libclang AST parsing**

**New Dependency:** libclang (for C/C++ AST parsing)

**File:** `/mnt/storage/Projects/kryon/ir/parsers/c/ir_c_ast_parser.c`

```c
#include <clang-c/Index.h>

typedef struct {
    IRLogicBlock** blocks;
    size_t count;
    size_t capacity;
} LogicCollector;

CXChildVisitResult visit_function(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    LogicCollector* collector = (LogicCollector*)client_data;

    if (clang_getCursorKind(cursor) == CXCursor_FunctionDecl) {
        CXString name = clang_getCursorSpelling(cursor);
        const char* func_name = clang_getCString(name);

        // Check if this is an event handler (e.g., "on_click", "handle_*")
        if (strstr(func_name, "on_") || strstr(func_name, "handle_")) {
            // Extract function body source code
            CXSourceRange range = clang_getCursorExtent(cursor);
            CXToken* tokens;
            unsigned num_tokens;
            CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
            clang_tokenize(tu, range, &tokens, &num_tokens);

            // Reconstruct source code from tokens
            char* source_code = reconstruct_source(tokens, num_tokens);

            // Create logic block
            IRLogicBlock* logic = malloc(sizeof(IRLogicBlock));
            logic->id = generate_logic_id();
            logic->type = LOGIC_EVENT_HANDLER;
            logic->language = "c";
            logic->source_code = source_code;

            // Add to collector
            add_logic_block(collector, logic);

            clang_disposeTokens(tu, tokens, num_tokens);
        }

        clang_disposeString(name);
    }

    return CXChildVisit_Recurse;
}

char* ir_c_file_to_kir_v3(const char* filepath) {
    // Parse C file with libclang
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index,
        filepath,
        NULL, 0,
        NULL, 0,
        CXTranslationUnit_None
    );

    if (!unit) {
        fprintf(stderr, "Unable to parse C file: %s\n", filepath);
        clang_disposeIndex(index);
        return NULL;
    }

    // Collect logic blocks
    LogicCollector collector = {0};
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(cursor, visit_function, &collector);

    // Also parse kryon API calls to build component tree
    IRComponent* root = parse_kryon_api_calls(unit);

    // Link components to logic blocks
    link_events_to_logic(root, collector.blocks, collector.count);

    // Create metadata
    IRSourceMetadata metadata = {
        .language = "c",
        .file_path = filepath
    };

    // Serialize to KIR
    char* kir = ir_serialize_json_v3(root, NULL, &collector, &metadata);

    // Cleanup
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);

    return kir;
}
```

### Phase 3: Implement Nim Parser

**Nim uses significant whitespace like Python**

**File:** `/mnt/storage/Projects/kryon/ir/parsers/nim/ir_nim_parser.c`

**Strategy:** Token-based parser (like .kry parser)

```c
typedef enum {
    NIM_TOK_IDENTIFIER,     // kryonApp, Container, Button
    NIM_TOK_COLON,          // :
    NIM_TOK_EQUALS,         // =
    NIM_TOK_STRING,         // "text"
    NIM_TOK_NUMBER,         // 100
    NIM_TOK_INDENT,         // Significant whitespace
    NIM_TOK_DEDENT,
    NIM_TOK_NEWLINE,
    NIM_TOK_PROC,           // proc keyword
    NIM_TOK_EOF
} NimTokenType;

typedef struct {
    NimTokenType type;
    const char* start;
    size_t length;
    int line;
    int column;
    int indent_level;
} NimToken;

// Parse Nim DSL to IR
char* ir_nim_to_kir(const char* source, size_t length) {
    NimLexer lexer;
    nim_lexer_init(&lexer, source, length);

    // Parse component tree
    IRComponent* root = parse_nim_component_tree(&lexer);

    // Parse event handlers (proc definitions)
    IRLogicManifest logic = parse_nim_event_handlers(&lexer);

    // Metadata
    IRSourceMetadata metadata = {
        .language = "nim",
        .file_path = "stdin"
    };

    // Serialize to KIR
    char* kir = ir_serialize_json_v3(root, NULL, &logic, &metadata);

    return kir;
}

static IRLogicManifest parse_nim_event_handlers(NimLexer* lexer) {
    IRLogicManifest manifest = {0};

    while (!nim_lexer_is_eof(lexer)) {
        NimToken tok = nim_lexer_peek(lexer);

        if (tok.type == NIM_TOK_PROC) {
            // Found proc definition
            nim_lexer_advance(lexer); // consume 'proc'

            NimToken name = nim_lexer_expect(lexer, NIM_TOK_IDENTIFIER);
            nim_lexer_expect(lexer, NIM_TOK_COLON);

            // Extract proc body
            char* source_code = extract_nim_proc_body(lexer);

            // Create logic block
            IRLogicBlock* logic = malloc(sizeof(IRLogicBlock));
            logic->id = generate_logic_id();
            logic->type = LOGIC_EVENT_HANDLER;
            logic->language = "nim";
            logic->source_code = source_code;

            add_logic_block(&manifest, logic);
        } else {
            nim_lexer_advance(lexer);
        }
    }

    return manifest;
}
```

### Phase 4: Implement Lua Parser

**Lua has C-like syntax (easier than Nim)**

**File:** `/mnt/storage/Projects/kryon/ir/parsers/lua/ir_lua_parser.c`

```c
typedef enum {
    LUA_TOK_LOCAL,          // local
    LUA_TOK_FUNCTION,       // function
    LUA_TOK_IDENTIFIER,     // UI, Container, state
    LUA_TOK_DOT,            // .
    LUA_TOK_LPAREN,         // (
    LUA_TOK_RPAREN,         // )
    LUA_TOK_LBRACE,         // {
    LUA_TOK_RBRACE,         // }
    LUA_TOK_STRING,
    LUA_TOK_NUMBER,
    LUA_TOK_COMMA,
    LUA_TOK_EOF
} LuaTokenType;

char* ir_lua_to_kir(const char* source, size_t length) {
    LuaLexer lexer;
    lua_lexer_init(&lexer, source, length);

    // Parse UI tree (UI.Container { ... })
    IRComponent* root = parse_lua_ui_tree(&lexer);

    // Parse event handlers (function definitions)
    IRLogicManifest logic = parse_lua_functions(&lexer);

    // Metadata
    IRSourceMetadata metadata = {
        .language = "lua",
        .file_path = "stdin"
    };

    char* kir = ir_serialize_json_v3(root, NULL, &logic, &metadata);

    return kir;
}

static IRComponent* parse_lua_ui_component(LuaLexer* lexer) {
    // Expect: UI.Container { properties }
    lua_lexer_expect(lexer, LUA_TOK_IDENTIFIER); // UI
    lua_lexer_expect(lexer, LUA_TOK_DOT);
    LuaToken component_type = lua_lexer_expect(lexer, LUA_TOK_IDENTIFIER); // Container

    IRComponent* comp = ir_create_component(component_type.value);

    lua_lexer_expect(lexer, LUA_TOK_LBRACE);

    // Parse table constructor
    while (!lua_lexer_match(lexer, LUA_TOK_RBRACE)) {
        LuaToken key = lua_lexer_expect(lexer, LUA_TOK_IDENTIFIER);
        lua_lexer_expect(lexer, LUA_TOK_EQUALS);

        if (lua_lexer_match(lexer, LUA_TOK_FUNCTION)) {
            // Event handler: onClick = function() ... end
            char* source_code = extract_lua_function_body(lexer);

            IRLogicBlock* logic = create_logic_block(
                LOGIC_EVENT_HANDLER,
                key.value,  // "onClick"
                "lua",
                source_code
            );

            attach_event_to_component(comp, key.value, logic->id);
        } else {
            // Regular property
            parse_lua_property_value(lexer, comp, key.value);
        }

        if (lua_lexer_match(lexer, LUA_TOK_COMMA)) {
            lua_lexer_advance(lexer);
        }
    }

    lua_lexer_expect(lexer, LUA_TOK_RBRACE);

    return comp;
}
```

### Phase 5: Rust Parser (Optional)

**File:** `/mnt/storage/Projects/kryon/ir/parsers/rust/ir_rust_parser.c`

**Strategy:** Use `syn` crate (Rust's AST parser) via FFI

**Defer this until Rust bindings are prioritized.**

## Testing Strategy

### Round-Trip Tests

For each parser, verify:

```bash
# Original source → KIR → Source (regenerated) → KIR (again)
kryon compile original.tsx -o original.kir
kryon codegen tsx original.kir -o generated.tsx
kryon compile generated.tsx -o generated.kir
diff original.kir generated.kir  # Should be identical (or semantically equivalent)
```

### Logic Preservation Tests

```bash
# Verify event handlers are in KIR
cat original.kir | jq '.logic_blocks[] | select(.type == "event_handler")'

# Should output:
# {
#   "id": "logic_1",
#   "type": "event_handler",
#   "event": "onClick",
#   "source_code": "() => setCount(count + 1)"
# }
```

## Files to Create/Modify

```
ir/parsers/tsx/tsx_ast_parser.ts ........ NEW: TypeScript AST parser
ir/parsers/tsx/ir_tsx_parser.c .......... MODIFY: Use AST parser
ir/parsers/c/ir_c_ast_parser.c .......... NEW: libclang-based parser
ir/parsers/c/ir_c_parser.c .............. MODIFY: Use AST parser
ir/parsers/nim/ir_nim_parser.h .......... NEW: Nim parser header
ir/parsers/nim/ir_nim_parser.c .......... NEW: Nim parser implementation
ir/parsers/nim/nim_lexer.c .............. NEW: Nim tokenizer
ir/parsers/lua/ir_lua_parser.h .......... NEW: Lua parser header
ir/parsers/lua/ir_lua_parser.c .......... NEW: Lua parser implementation
ir/parsers/lua/lua_lexer.c .............. NEW: Lua tokenizer
ir/Makefile ............................. MODIFY: Add new parsers
cli/src/commands/cmd_compile.c .......... MODIFY: Add Nim/Lua support
```

## Dependencies

- **TSX Parser:** Bun, @typescript-eslint/typescript-estree
- **C Parser:** libclang (apt-get install libclang-dev)
- **Nim Parser:** None (custom lexer)
- **Lua Parser:** None (custom lexer)

## Estimated Effort

- Phase 1 (TSX): 3 days
- Phase 2 (C): 4 days
- Phase 3 (Nim): 5 days
- Phase 4 (Lua): 4 days
- Phase 5 (Testing): 2 days

**Total: ~18 days (3.5 weeks)**

## Success Criteria

✅ All parsers output complete KIR
✅ Event handlers preserved as source code
✅ Reactive state declarations preserved
✅ Round-trip conversion works (source → KIR → source → KIR)
✅ Original language metadata tracked
✅ All existing examples still compile
