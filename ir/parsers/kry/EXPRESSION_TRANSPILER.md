# KRY Expression Transpiler

## Overview

The KRY Expression Transpiler enables "write once, run everywhere" syntax for KRY files. It automatically converts ES6-style expressions to both Lua and JavaScript, eliminating the need for separate `@lua` and `@js` blocks for simple expressions.

## Features

### Supported Expression Types

- **Literals**: strings, numbers, booleans, null, undefined
  ```javascript
  42 → Lua: 42, JS: 42
  "hello" → Lua: "hello", JS: "hello"
  true → Lua: true, JS: true
  null → Lua: nil, JS: null
  ```

- **Binary Operators**: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`
  ```javascript
  a + b → Lua: (a + b), JS: (a + b)
  a && b → Lua: (a and b), JS: (a && b)
  a != b → Lua: (a ~= b), JS: (a != b)
  ```

- **Unary Operators**: `-`, `!`, typeof
  ```javascript
  !a → Lua: not a, JS: !a
  -a → Lua: -a, JS: -a
  ```

- **Property Access**: `obj.prop`, `obj["prop"]`
  ```javascript
  obj.prop → Lua: obj.prop, JS: obj.prop
  ```

- **Array Access**: `arr[index]` (automatically converts 0-index to 1-index for Lua)
  ```javascript
  arr[0] → Lua: arr[(0 + 1)], JS: arr[0]
  arr[i] → Lua: arr[(i + 1)], JS: arr[i]
  ```

- **Function Calls**: `func(arg1, arg2)`
  ```javascript
  add(a, b) → Lua: add(a, b), JS: add(a, b)
  ```

- **Array Literals**: `[1, 2, 3]`
  ```javascript
  [1, 2, 3] → Lua: {1, 2, 3}, JS: [1, 2, 3]
  ```

- **Object Literals**: `{key: val}`
  ```javascript
  {key: val} → Lua: {key = val}, JS: {key: val}
  {a: 1, b: 2} → Lua: {a = 1, b = 2}, JS: {a: 1, b: 2}
  ```

- **Arrow Functions**: `x => x * 2`
  ```javascript
  x => x * 2 → Lua: function(x) return (x * 2); end, JS: (x) => (x * 2)
  (a, b) => a + b → Lua: function(a, b) return (a + b); end, JS: (a, b) => (a + b)
  ```

## Usage

### In KRY Files

Expressions are automatically transpiled when used in property values:

```kry
App {
  // Array literal - works natively
  colors = ["Red", "Green", "Blue"]

  // Object literal - works natively
  config = {
    width: 800,
    height: 600,
    title: "My App"
  }

  // Expression - automatically transpiled
  title = config.title

  // Arrow function - automatically transpiled
  getRandomColor = () => colors[Math.floor(Math.random() * colors.length)]
}
```

### Platform Code Generation

When generating code for different platforms, expressions are automatically converted:

**For Lua:**
```lua
local colors = {"Red", "Green", "Blue"}
local config = {width = 800, height = 600, title = "My App"}
local title = config.title
local getRandomColor = function() return colors[(math.floor(math.random() * ( #(colors) )) + 1)] end
```

**For JavaScript:**
```javascript
const colors = ["Red", "Green", "Blue"];
const config = {width: 800, height: 600, title: "My App"};
const title = config.title;
const getRandomColor = () => colors[Math.floor(Math.random() * colors.length)];
```

## Implementation

### Architecture

The expression transpiler consists of three main components:

1. **Parser** (`kry_expression.c`):
   - Recursive descent parser for ES6-style expressions
   - Builds an Abstract Syntax Tree (AST) with strongly typed nodes
   - Handles operator precedence and associativity

2. **Type System** (`kry_expression.h`):
   - `KryExprType`: Enum for all expression node types
   - `KryExprNode`: Discriminated union with type tag
   - Proper C types throughout (`size_t`, `bool`, `const char*`, etc.)

3. **Code Generators**:
   - Lua generator: Converts AST to Lua syntax
   - JavaScript generator: Converts AST to JavaScript/ES6 syntax
   - Automatic handling of platform differences (e.g., array indexing)

### Integration

The transpiler is integrated into `kry_to_ir.c`:

1. **Conversion Context** includes `target_platform` field
2. **Expression Handling**:
   - `kry_value_transpile_expression()`: Transpiles a single expression
   - `kry_value_to_target_code()`: Converts KRY values to target platform code
3. **Fallback**: If transpilation fails, falls back to raw expression for compatibility

## Type Safety

All structures and functions use proper C types:

- `enum` types for categorizations (no magic integers)
- `union` with explicit type tags for discriminated unions
- `size_t` for all counts and lengths
- `bool` for boolean values
- `const char*` for immutable strings
- Explicit typed parameters and return values

## Testing

A comprehensive test suite (`test_expression.c`) validates:
- Literals (strings, numbers, booleans, null)
- Operators (binary, unary, logical)
- Property and array access
- Function calls
- Array and object literals
- Arrow functions
- Complex nested expressions

Run tests:
```bash
cd /mnt/storage/Projects/KryonLabs/kryon/ir/parsers/kry
gcc -o test_expression test_expression.c kry_expression.c -I.
./test_expression
```

## Examples

### Example 1: Simple Expression

**KRY:**
```kry
Text {
  text = state.count + 1
}
```

**Lua:**
```lua
text = (state.count + 1)
```

**JavaScript:**
```javascript
text = (state.count + 1)
```

### Example 2: Array with Arrow Function

**KRY:**
```kry
App {
  items = [1, 2, 3, 4, 5]
  doubled = items.map(x => x * 2)
}
```

**Lua:**
```lua
local items = {1, 2, 3, 4, 5}
local doubled = items.map(function(x) return (x * 2) end)
```

**JavaScript:**
```javascript
const items = [1, 2, 3, 4, 5];
const doubled = items.map((x) => (x * 2));
```

### Example 3: Complex Object with Nested Functions

**KRY:**
```kry
App {
  handler = {
    onClick: (e) => console.log(e.target.value),
    onHover: (el) => el.style.background = "blue"
  }
}
```

**Lua:**
```lua
local handler = {
  onClick = function(e) return console.log((e.target.value)) end,
  onHover = function(el) return (el.style.background) = "blue" end
}
```

**JavaScript:**
```javascript
const handler = {
  onClick: (e) => console.log(e.target.value),
  onHover: (el) => el.style.background = "blue"
};
```

## Limitations

Currently NOT supported:
- Block arrow functions: `x => { return x * 2; }` (use `x => x * 2` instead)
- Destructuring: `const {a, b} = obj`
- Spread operator: `...arr`
- Template literals: `` `Hello ${name}` ``
- Async/await
- Classes and prototypes

For complex logic, use `@lua`/`@js` blocks:

```kry
@lua
function complexLogic(x)
  -- Multi-line Lua code
  local result = x * 2
  if result > 100 then
    result = 100
  end
  return result
end
@end

@js
function complexLogic(x) {
  // Multi-line JavaScript code
  let result = x * 2;
  if (result > 100) {
    result = 100;
  }
  return result;
}
@end
```

## Future Enhancements

Planned features:
- Block arrow functions with multi-line bodies
- Destructuring patterns
- Spread operator for arrays and objects
- Template literals
- Array methods (`.filter()`, `.reduce()`, etc.)
- Property shorthand: `{x, y}` instead of `{x: x, y: y}`
- Computed properties: `{[key]: value}`

## API Reference

### Main Functions

```c
// Parse an expression string into an AST
KryExprNode* kry_expr_parse(const char* source, char** error_output);

// Free an expression AST node
void kry_expr_free(KryExprNode* node);

// Transpile expression AST to target platform code
char* kry_expr_transpile(KryExprNode* node, KryExprOptions* options, size_t* output_length);

// Convenience function: Parse and transpile in one step
char* kry_expr_transpile_source(const char* source, KryExprTarget target, size_t* output_length);

// Get the last error message
const char* kry_expr_get_error(void);

// Clear the last error message
void kry_expr_clear_error(void);
```

### Options

```c
typedef enum {
    KRY_TARGET_LUA,
    KRY_TARGET_JAVASCRIPT
} KryExprTarget;

typedef struct {
    KryExprTarget target;
    bool pretty_print;
    int indent_level;
} KryExprOptions;
```

## License

MIT License - Part of the KRYON Framework
