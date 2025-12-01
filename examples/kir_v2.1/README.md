# Kryon IR v2.1 Examples

This directory contains example .kir v2.1 files demonstrating the bytecode VM feature.

## Files

### counter_app.kir
**Simple counter with increment/decrement**

Features demonstrated:
- Basic arithmetic operations (ADD, SUB)
- Reactive state management (GET_STATE, SET_STATE)
- Multiple event handlers (3 buttons)
- State-reactive text display

Bytecode functions:
1. `handle_increment` - Adds 1 to counter
2. `handle_decrement` - Subtracts 1 from counter
3. `handle_reset` - Sets counter to 0

### todo_with_save.kir
**Todo app with host function integration**

Features demonstrated:
- String state management
- Conditional logic (JUMP_IF_FALSE)
- Empty string validation
- Host function calls (auto-save)
- Multi-state reactive updates

Bytecode functions:
1. `handle_add_todo` - Validates input, increments count, calls save

Host functions:
- `save_todo_count` - Backend saves count to file (graceful degradation)

## Bytecode Breakdown

### Counter Increment Function

```
GET_STATE 1      ; Load counter value
PUSH_INT 1       ; Push constant 1
ADD              ; counter + 1
SET_STATE 1      ; Save result back to counter
HALT             ; Done
```

Equivalent to: `counter.value += 1`

### Todo Add Function

```
GET_STATE 2      ; Load input text
DUP              ; Duplicate for comparison
PUSH_STRING ""   ; Push empty string
EQ               ; Check if input is empty
JUMP_IF_FALSE 3  ; Skip next 3 instructions if not empty
POP              ; Clean up stack
HALT             ; Exit early if empty

; Input is not empty:
GET_STATE 1      ; Load todo count
PUSH_INT 1       ; Push constant 1
ADD              ; count + 1
SET_STATE 1      ; Save new count
PUSH_STRING ""   ; Push empty string
SET_STATE 2      ; Clear input field
GET_STATE 1      ; Load count for save
CALL_HOST 100    ; Call save_todo_count(count)
HALT             ; Done
```

Equivalent to:
```nim
if input_text.value != "":
  todo_count.value += 1
  input_text.value = ""
  save_todo_count(todo_count.value)
```

## Testing These Files

Once the serialization layer is implemented, you can test these files with:

```bash
# Validate the .kir file
kryon validate counter_app.kir --check-bytecode

# Inspect bytecode functions
kryon inspect counter_app.kir --show-bytecode

# Run the app (when VM integration is complete)
kryon run counter_app.kir --renderer=sdl3
```

## Creating Your Own

To create a .kir v2.1 file:

1. Define your component tree with onClick handlers referencing function IDs
2. Create bytecode functions for each handler
3. Define reactive states accessed by bytecode
4. Declare any host functions you'll call
5. Validate the file format

Example minimal structure:

```json
{
  "version": "2.1",
  "component": { ... },
  "functions": [ ... ],
  "states": [ ... ],
  "host_functions": []
}
```

## Binary Format

These .kir files can be compiled to .kirb (binary) format for production use:

```bash
kryon convert counter_app.kir --format=binary --output=counter_app.kirb
```

The binary format is ~5-10× smaller and loads faster.
