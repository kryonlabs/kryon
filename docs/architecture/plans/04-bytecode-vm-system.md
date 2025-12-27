# Bytecode VM System (.krb Format)

## Executive Summary

Implement a complete bytecode format (.krb) and virtual machine for VM-based languages (JavaScript, Lua) to enable efficient execution without source code generation.

## Architecture Overview

### Execution Modes

```
KIR
   ├─→ VM Mode (JavaScript, Lua, Python)
   │       ↓
   │   .krb bytecode
   │       ↓
   │   KryonVM Runtime
   │       ↓
   │   Renderer (DOM, Terminal, SDL3)
   │
   └─→ Native Mode (C, Nim, Rust)
           ↓
       Source Codegen
           ↓
       Native Compiler
           ↓
       Binary Executable
```

## Current State

### Existing Implementation ⚠️ (Partial)

**Files Found:**
- `/mnt/storage/Projects/kryon/ir/ir_krb.h` - Header definitions
- `/mnt/storage/Projects/kryon/ir/ir_krb.c` - Bytecode generator (partial)
- `/mnt/storage/Projects/kryon/packages/vm/src/vm.ts` - TypeScript VM
- `/mnt/storage/Projects/kryon/packages/vm/src/opcodes.ts` - Opcode definitions
- `/mnt/storage/Projects/kryon/packages/vm/src/loader.ts` - KRB loader

### Existing Opcode System

**From:** `/mnt/storage/Projects/kryon/packages/vm/src/opcodes.ts`

```typescript
export enum Opcode {
    CREATE_COMPONENT = 0x01,
    SET_PROPERTY     = 0x02,
    ADD_CHILD        = 0x03,
    CREATE_STATE     = 0x04,
    BIND_EVENT       = 0x05,
    BIND_PROPERTY    = 0x06,
    CALL_FUNCTION    = 0x07,
    JUMP             = 0x08,
    JUMP_IF          = 0x09,
    RETURN           = 0x0A,
    // ... more opcodes
}
```

### Existing Event System

**From analysis:**

```c
typedef enum {
    KRYON_EVT_CLICK      = 0,
    KRYON_EVT_TOUCH      = 1,
    KRYON_EVT_KEY        = 2,
    KRYON_EVT_TIMER      = 3,
    KRYON_EVT_HOVER      = 4,
    KRYON_EVT_SCROLL     = 5,
    KRYON_EVT_FOCUS      = 6,
    KRYON_EVT_BLUR       = 7,
    KRYON_EVT_CUSTOM     = 8
} kryon_event_type_t;
```

### What's Missing

1. **Complete KIR → KRB compilation** - Only partial implementation
2. **Logic block bytecode** - Event handlers need to be compiled to bytecode
3. **Reactive state operations** - Opcodes for state get/set/subscribe
4. **C-based VM** - Currently only TypeScript VM exists
5. **Desktop integration** - Connect KRB to SDL3/Terminal renderers
6. **Embedded VM** - Lightweight VM for MCUs (STM32, RP2040)

## Complete KRB Format Specification

### KRB File Structure

```
┌─────────────────────────────────────────┐
│ Header (32 bytes)                       │
│ - Magic: "KRB\0" (4 bytes)             │
│ - Version: Major.Minor (2 bytes)        │
│ - Flags: 2 bytes                        │
│ - Component Count: 4 bytes              │
│ - State Count: 4 bytes                  │
│ - Logic Block Count: 4 bytes            │
│ - String Table Size: 4 bytes            │
│ - Reserved: 8 bytes                     │
├─────────────────────────────────────────┤
│ String Table                            │
│ - Offset table (4 bytes × N)            │
│ - String data (null-terminated)         │
├─────────────────────────────────────────┤
│ Component Definitions                   │
│ - Component records (variable length)   │
├─────────────────────────────────────────┤
│ Reactive State Table                    │
│ - State definitions                     │
├─────────────────────────────────────────┤
│ Event Binding Table                     │
│ - Component ID → Event → Function Index │
├─────────────────────────────────────────┤
│ Logic Bytecode Section                  │
│ - Function table                        │
│ - Bytecode instructions                 │
└─────────────────────────────────────────┘
```

### Header Format

```c
typedef struct {
    char magic[4];              // "KRB\0"
    uint8_t version_major;      // 1
    uint8_t version_minor;      // 0
    uint16_t flags;             // Reserved
    uint32_t component_count;
    uint32_t state_count;
    uint32_t logic_block_count;
    uint32_t string_table_size;
    uint8_t reserved[8];
} __attribute__((packed)) KRBHeader;
```

### Component Record Format

```c
typedef struct {
    uint32_t id;                // Component ID
    uint8_t type;               // ComponentType enum
    uint32_t parent_id;         // Parent component ID (0 = root)
    uint16_t property_count;    // Number of properties
    // Followed by property records
} __attribute__((packed)) KRBComponent;

typedef struct {
    uint8_t type;               // PropertyType enum
    uint32_t name_offset;       // Offset in string table
    uint32_t value_offset;      // Offset in string table (or immediate value)
} __attribute__((packed)) KRBProperty;
```

### Reactive State Record

```c
typedef struct {
    uint32_t id;                // State ID
    uint32_t name_offset;       // Name in string table
    uint8_t type;               // StateType enum (int, string, bool, etc.)
    uint32_t initial_value;     // Initial value (or offset if complex)
} __attribute__((packed)) KRBState;
```

### Event Binding Record

```c
typedef struct {
    uint32_t component_id;      // Which component
    uint8_t event_type;         // kryon_event_type_t
    uint32_t function_index;    // Index into logic function table
} __attribute__((packed)) KRBEventBinding;
```

### Logic Function Record

```c
typedef struct {
    uint32_t id;                // Function ID
    uint32_t name_offset;       // Function name in string table
    uint32_t bytecode_offset;   // Offset to bytecode in logic section
    uint32_t bytecode_length;   // Length of bytecode
    uint8_t param_count;        // Number of parameters
    uint8_t local_count;        // Number of local variables
} __attribute__((packed)) KRBFunction;
```

## Complete Opcode Set

### Component Operations

```c
// CREATE_COMPONENT <type:u8> <id:u32>
// Creates a new component of the given type with ID
#define OP_CREATE_COMPONENT     0x01

// SET_PROPERTY <comp_id:u32> <prop_type:u8> <value_offset:u32>
// Sets a property on a component
#define OP_SET_PROPERTY         0x02

// ADD_CHILD <parent_id:u32> <child_id:u32>
// Adds child component to parent
#define OP_ADD_CHILD            0x03
```

### State Operations

```c
// CREATE_STATE <state_id:u32> <type:u8> <initial_value:u32>
// Creates a reactive state variable
#define OP_CREATE_STATE         0x04

// GET_STATE <state_id:u32> <dest_reg:u8>
// Loads state value into register
#define OP_GET_STATE            0x05

// SET_STATE <state_id:u32> <src_reg:u8>
// Sets state value from register (triggers reactivity)
#define OP_SET_STATE            0x06

// SUBSCRIBE_STATE <state_id:u32> <callback_fn:u32>
// Subscribes to state changes
#define OP_SUBSCRIBE_STATE      0x07
```

### Event Operations

```c
// BIND_EVENT <comp_id:u32> <event_type:u8> <fn_index:u32>
// Binds event handler to component
#define OP_BIND_EVENT           0x08

// EMIT_EVENT <comp_id:u32> <event_type:u8>
// Emits an event (for testing/synthetic events)
#define OP_EMIT_EVENT           0x09
```

### Control Flow

```c
// JUMP <offset:i32>
// Unconditional jump (relative offset)
#define OP_JUMP                 0x10

// JUMP_IF <offset:i32> <reg:u8>
// Jump if register is truthy
#define OP_JUMP_IF              0x11

// JUMP_IF_NOT <offset:i32> <reg:u8>
// Jump if register is falsy
#define OP_JUMP_IF_NOT          0x12

// CALL <fn_index:u32> <arg_count:u8>
// Call function by index
#define OP_CALL                 0x13

// RETURN <reg:u8>
// Return from function with value in register
#define OP_RETURN               0x14
```

### Arithmetic & Logic

```c
// ADD <dest:u8> <src1:u8> <src2:u8>
#define OP_ADD                  0x20

// SUB <dest:u8> <src1:u8> <src2:u8>
#define OP_SUB                  0x21

// MUL <dest:u8> <src1:u8> <src2:u8>
#define OP_MUL                  0x22

// DIV <dest:u8> <src1:u8> <src2:u8>
#define OP_DIV                  0x23

// MOD <dest:u8> <src1:u8> <src2:u8>
#define OP_MOD                  0x24

// CMP <src1:u8> <src2:u8>
// Sets comparison flags
#define OP_CMP                  0x25

// AND <dest:u8> <src1:u8> <src2:u8>
#define OP_AND                  0x26

// OR <dest:u8> <src1:u8> <src2:u8>
#define OP_OR                   0x27

// NOT <dest:u8> <src:u8>
#define OP_NOT                  0x28
```

### Memory Operations

```c
// LOAD_IMMEDIATE <dest:u8> <value:u32>
// Load immediate value into register
#define OP_LOAD_IMM             0x30

// LOAD_STRING <dest:u8> <str_offset:u32>
// Load string from string table
#define OP_LOAD_STR             0x31

// MOVE <dest:u8> <src:u8>
// Copy register to register
#define OP_MOVE                 0x32
```

### Renderer Operations

```c
// INVALIDATE
// Mark UI as dirty, trigger re-render
#define OP_INVALIDATE           0x40

// SET_ROOT <comp_id:u32>
// Set root component for rendering
#define OP_SET_ROOT             0x41
```

## KIR → KRB Compiler

### Implementation

**File:** `/mnt/storage/Projects/kryon/ir/ir_krb.c`

```c
typedef struct {
    uint8_t* bytecode;
    size_t capacity;
    size_t length;
    StringTable strings;
    FunctionTable functions;
} KRBBuilder;

bool ir_compile_to_krb(const char* kir_path, const char* krb_path) {
    // 1. Parse KIR
    char* kir_json = read_file(kir_path);
    KIRDocument* doc = parse_kir_v3(kir_json);

    // 2. Initialize builder
    KRBBuilder builder;
    krb_builder_init(&builder);

    // 3. Write header
    krb_write_header(&builder, doc);

    // 4. Build string table
    krb_build_string_table(&builder, doc);

    // 5. Compile component tree
    krb_compile_components(&builder, doc->root_component);

    // 6. Compile reactive state
    if (doc->reactive_manifest) {
        krb_compile_reactive_state(&builder, doc->reactive_manifest);
    }

    // 7. Compile logic blocks
    if (doc->logic_manifest) {
        krb_compile_logic_blocks(&builder, doc->logic_manifest);
    }

    // 8. Compile event bindings
    krb_compile_event_bindings(&builder, doc);

    // 9. Write to file
    FILE* f = fopen(krb_path, "wb");
    if (!f) return false;

    fwrite(builder.bytecode, 1, builder.length, f);
    fclose(f);

    krb_builder_destroy(&builder);
    free(kir_json);
    return true;
}

static void krb_compile_components(KRBBuilder* builder, IRComponent* comp) {
    // Emit CREATE_COMPONENT opcode
    krb_emit_u8(builder, OP_CREATE_COMPONENT);
    krb_emit_u8(builder, comp->type);
    krb_emit_u32(builder, comp->id);

    // Emit properties
    if (comp->style) {
        krb_compile_properties(builder, comp);
    }

    // Recursively compile children
    for (size_t i = 0; i < comp->child_count; i++) {
        krb_compile_components(builder, comp->children[i]);

        // Emit ADD_CHILD
        krb_emit_u8(builder, OP_ADD_CHILD);
        krb_emit_u32(builder, comp->id);              // parent
        krb_emit_u32(builder, comp->children[i]->id); // child
    }
}

static void krb_compile_reactive_state(KRBBuilder* builder, IRReactiveManifest* manifest) {
    for (size_t i = 0; i < manifest->state_count; i++) {
        IRReactiveState* state = &manifest->states[i];

        // Emit CREATE_STATE
        krb_emit_u8(builder, OP_CREATE_STATE);
        krb_emit_u32(builder, state->id);
        krb_emit_u8(builder, state_type_to_enum(state->type));
        krb_emit_u32(builder, parse_initial_value(state->initial_value));
    }
}

static void krb_compile_logic_blocks(KRBBuilder* builder, IRLogicManifest* logic) {
    IRLogicBlock* block = logic->blocks;
    while (block) {
        if (block->type == LOGIC_EVENT_HANDLER) {
            // Add function to function table
            uint32_t fn_index = krb_add_function(builder, block->id);

            // Compile logic source code to bytecode
            uint8_t* bytecode = compile_logic_to_bytecode(block);
            uint32_t length = get_bytecode_length(bytecode);

            // Write function record
            krb_write_function(builder, fn_index, block->id, bytecode, length);

            free(bytecode);
        }
        block = block->next;
    }
}

static uint8_t* compile_logic_to_bytecode(IRLogicBlock* block) {
    // Simple expression compiler
    // Example: "count += 1" → bytecode

    KRBBuilder local_builder;
    krb_builder_init(&local_builder);

    if (strcmp(block->language, "kry") == 0 ||
        strcmp(block->language, "typescript") == 0) {

        // Parse expression (simplified)
        // Real implementation needs proper parser/compiler

        // Example: count += 1
        // →  GET_STATE <count_id> R0
        //    LOAD_IMM R1, 1
        //    ADD R0, R0, R1
        //    SET_STATE <count_id> R0
        //    INVALIDATE

        krb_emit_u8(&local_builder, OP_GET_STATE);
        krb_emit_u32(&local_builder, find_state_id("count"));
        krb_emit_u8(&local_builder, 0); // R0

        krb_emit_u8(&local_builder, OP_LOAD_IMM);
        krb_emit_u8(&local_builder, 1); // R1
        krb_emit_u32(&local_builder, 1); // value

        krb_emit_u8(&local_builder, OP_ADD);
        krb_emit_u8(&local_builder, 0); // dest: R0
        krb_emit_u8(&local_builder, 0); // src1: R0
        krb_emit_u8(&local_builder, 1); // src2: R1

        krb_emit_u8(&local_builder, OP_SET_STATE);
        krb_emit_u32(&local_builder, find_state_id("count"));
        krb_emit_u8(&local_builder, 0); // R0

        krb_emit_u8(&local_builder, OP_INVALIDATE);

        krb_emit_u8(&local_builder, OP_RETURN);
        krb_emit_u8(&local_builder, 0); // R0
    }

    return local_builder.bytecode;
}

static void krb_compile_event_bindings(KRBBuilder* builder, KIRDocument* doc) {
    // Walk component tree and emit event bindings
    compile_event_bindings_recursive(builder, doc->root_component);
}

static void compile_event_bindings_recursive(KRBBuilder* builder, IRComponent* comp) {
    if (comp->event_bindings) {
        IREventBinding* binding = comp->event_bindings->bindings;
        while (binding) {
            // Find function index for this logic block
            uint32_t fn_index = krb_find_function_index(builder, binding->logic_block_id);

            // Emit BIND_EVENT
            krb_emit_u8(builder, OP_BIND_EVENT);
            krb_emit_u32(builder, comp->id);
            krb_emit_u8(builder, event_name_to_type(binding->event_name));
            krb_emit_u32(builder, fn_index);

            binding = binding->next;
        }
    }

    // Recurse to children
    for (size_t i = 0; i < comp->child_count; i++) {
        compile_event_bindings_recursive(builder, comp->children[i]);
    }
}
```

## C-based VM Implementation

**File:** `/mnt/storage/Projects/kryon/ir/ir_krb_vm.c`

```c
typedef struct {
    uint32_t registers[16];     // General-purpose registers
    uint8_t* pc;                // Program counter
    uint8_t* bytecode;
    size_t bytecode_length;
    IRComponent** components;   // Component table
    KRBState* states;           // Reactive state table
    KRBFunction* functions;     // Function table
    uint32_t component_count;
    uint32_t state_count;
    uint32_t function_count;
} KRBVMContext;

bool krb_vm_execute(const char* krb_path) {
    // 1. Load KRB file
    uint8_t* krb = load_krb_file(krb_path);
    if (!krb) return false;

    // 2. Initialize VM
    KRBVMContext vm;
    krb_vm_init(&vm, krb);

    // 3. Execute initialization bytecode
    while (vm.pc < vm.bytecode + vm.bytecode_length) {
        uint8_t opcode = *vm.pc++;

        switch (opcode) {
            case OP_CREATE_COMPONENT: {
                uint8_t type = *vm.pc++;
                uint32_t id = read_u32(&vm.pc);
                vm.components[id] = ir_create_component(type);
                break;
            }

            case OP_SET_PROPERTY: {
                uint32_t comp_id = read_u32(&vm.pc);
                uint8_t prop_type = *vm.pc++;
                uint32_t value_offset = read_u32(&vm.pc);
                // Set property...
                break;
            }

            case OP_ADD_CHILD: {
                uint32_t parent_id = read_u32(&vm.pc);
                uint32_t child_id = read_u32(&vm.pc);
                ir_add_child(vm.components[parent_id], vm.components[child_id]);
                break;
            }

            case OP_CREATE_STATE: {
                uint32_t state_id = read_u32(&vm.pc);
                uint8_t type = *vm.pc++;
                uint32_t initial_value = read_u32(&vm.pc);
                vm.states[state_id].value = initial_value;
                vm.states[state_id].type = type;
                break;
            }

            case OP_BIND_EVENT: {
                uint32_t comp_id = read_u32(&vm.pc);
                uint8_t event_type = *vm.pc++;
                uint32_t fn_index = read_u32(&vm.pc);

                // Register event handler
                register_event_handler(
                    vm.components[comp_id],
                    event_type,
                    vm.functions[fn_index].bytecode
                );
                break;
            }

            case OP_SET_ROOT: {
                uint32_t comp_id = read_u32(&vm.pc);
                kryon_set_root(vm.components[comp_id]);
                break;
            }

            case OP_GET_STATE: {
                uint32_t state_id = read_u32(&vm.pc);
                uint8_t dest_reg = *vm.pc++;
                vm.registers[dest_reg] = vm.states[state_id].value;
                break;
            }

            case OP_SET_STATE: {
                uint32_t state_id = read_u32(&vm.pc);
                uint8_t src_reg = *vm.pc++;
                vm.states[state_id].value = vm.registers[src_reg];
                // Trigger reactivity
                notify_state_change(state_id);
                break;
            }

            case OP_ADD: {
                uint8_t dest = *vm.pc++;
                uint8_t src1 = *vm.pc++;
                uint8_t src2 = *vm.pc++;
                vm.registers[dest] = vm.registers[src1] + vm.registers[src2];
                break;
            }

            // ... implement all opcodes

            case OP_INVALIDATE:
                kryon_invalidate();
                break;

            case OP_RETURN:
                // Pop call frame, return to caller
                return true;

            default:
                fprintf(stderr, "Unknown opcode: 0x%02x\n", opcode);
                return false;
        }
    }

    return true;
}
```

## Desktop Integration

**File:** `/mnt/storage/Projects/kryon/backends/desktop/krb_executor.c`

```c
// Execute KRB file with SDL3 renderer
bool kryon_run_krb(const char* krb_path) {
    // 1. Initialize renderer
    if (!sdl3_renderer_init()) {
        return false;
    }

    // 2. Execute KRB bytecode
    if (!krb_vm_execute(krb_path)) {
        return false;
    }

    // 3. Main loop
    while (kryon_should_run()) {
        // Process events
        kryon_event_t evt;
        while (kryon_poll_event(&evt)) {
            // Dispatch to VM event handler
            krb_vm_dispatch_event(&evt);
        }

        // Render
        sdl3_renderer_render();
    }

    // 4. Cleanup
    sdl3_renderer_shutdown();
    krb_vm_destroy();

    return true;
}
```

## CLI Integration

**File:** `/mnt/storage/Projects/kryon/cli/src/commands/cmd_build.c`

```c
int cmd_build(int argc, char** argv) {
    const char* input = argv[0];
    const char* output = argc > 1 ? argv[1] : "app.krb";

    // Detect format
    const char* ext = path_extension(input);

    if (strcmp(ext, ".kir") == 0) {
        // KIR → KRB
        printf("Compiling %s → %s\n", input, output);
        if (!ir_compile_to_krb(input, output)) {
            fprintf(stderr, "Error: Compilation failed\n");
            return 1;
        }
    } else {
        // Source → KIR → KRB
        char kir_path[1024];
        snprintf(kir_path, sizeof(kir_path), "/tmp/%s.kir", basename(input));

        printf("Step 1: %s → %s\n", input, kir_path);
        if (system("kryon compile \"%s\" -o \"%s\"", input, kir_path) != 0) {
            return 1;
        }

        printf("Step 2: %s → %s\n", kir_path, output);
        if (!ir_compile_to_krb(kir_path, output)) {
            fprintf(stderr, "Error: Bytecode compilation failed\n");
            return 1;
        }
    }

    printf("✓ Built: %s\n", output);
    return 0;
}
```

## Usage Examples

```bash
# Compile to bytecode
kryon build app.tsx -o app.krb

# Run bytecode (desktop)
kryon run app.krb

# Run bytecode (web - via Node.js VM)
node kryon-vm-runner.js app.krb

# Run bytecode (embedded - via C VM)
./kryon-vm app.krb  # On device
```

## Files to Create/Modify

```
ir/ir_krb.h ........................ MODIFY: Complete spec
ir/ir_krb.c ........................ MODIFY: Complete compiler
ir/ir_krb_vm.h ..................... CREATE: C VM header
ir/ir_krb_vm.c ..................... CREATE: C VM implementation
backends/desktop/krb_executor.c .... CREATE: Desktop KRB runner
packages/vm/src/vm.ts .............. MODIFY: Complete TypeScript VM
packages/vm/src/loader.ts .......... MODIFY: KRB file loader
cli/src/commands/cmd_build.c ....... MODIFY: Add KRB compilation
```

## Estimated Effort

- KRB Compiler: 5 days
- C VM: 7 days
- Desktop Integration: 2 days
- Web VM Updates: 2 days
- Testing: 3 days

**Total: ~19 days (4 weeks)**

## Success Criteria

✅ KIR compiles to valid KRB bytecode
✅ KRB files are ~70% smaller than JSON KIR
✅ C VM executes KRB correctly
✅ Desktop renderer works with KRB
✅ Web VM (TypeScript) works with KRB
✅ Event handlers execute from bytecode
✅ Reactive state updates trigger re-renders
✅ All examples compile to KRB and run
