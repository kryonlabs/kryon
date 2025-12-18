# Code Block Examples

This document demonstrates code blocks and syntax highlighting in Kryon's markdown renderer.

## Fenced Code Blocks

### Python

```python
def fibonacci(n):
    """Calculate the nth Fibonacci number."""
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

# Example usage
for i in range(10):
    print(f"F({i}) = {fibonacci(i)}")
```

### Nim

```nim
proc fibonacci(n: int): int =
  ## Calculate the nth Fibonacci number
  if n <= 1:
    return n
  return fibonacci(n-1) + fibonacci(n-2)

# Example usage
for i in 0..<10:
  echo &"F({i}) = {fibonacci(i)}"
```

### C

```c
#include <stdio.h>

int fibonacci(int n) {
    // Calculate the nth Fibonacci number
    if (n <= 1) {
        return n;
    }
    return fibonacci(n-1) + fibonacci(n-2);
}

int main() {
    for (int i = 0; i < 10; i++) {
        printf("F(%d) = %d\n", i, fibonacci(i));
    }
    return 0;
}
```

### JavaScript

```javascript
function fibonacci(n) {
    // Calculate the nth Fibonacci number
    if (n <= 1) {
        return n;
    }
    return fibonacci(n-1) + fibonacci(n-2);
}

// Example usage
for (let i = 0; i < 10; i++) {
    console.log(`F(${i}) = ${fibonacci(i)}`);
}
```

### Rust

```rust
fn fibonacci(n: u32) -> u32 {
    /// Calculate the nth Fibonacci number
    match n {
        0 | 1 => n,
        _ => fibonacci(n-1) + fibonacci(n-2)
    }
}

fn main() {
    for i in 0..10 {
        println!("F({}) = {}", i, fibonacci(i));
    }
}
```

## Shell Commands

```bash
# Build Kryon
make clean
make build
make install

# Run examples
kryon run examples/md/hello_world.md
kryon run examples/md/flowchart.md

# Build for web
kryon build examples/md/documentation.md --targets=web
```

## JSON

```json
{
  "name": "kryon-app",
  "version": "1.0.0",
  "dependencies": {
    "kryon": "^1.3.0"
  },
  "config": {
    "renderer": "sdl3",
    "width": 800,
    "height": 600
  }
}
```

## YAML

```yaml
app:
  name: kryon-app
  version: 1.0.0

dependencies:
  - kryon: ^1.3.0

config:
  renderer: sdl3
  width: 800
  height: 600
```

## TOML

```toml
[app]
name = "kryon-app"
version = "1.0.0"

[dependencies]
kryon = "^1.3.0"

[config]
renderer = "sdl3"
width = 800
height = 600
```

## Indented Code Blocks

You can also create code blocks by indenting with 4 spaces:

    This is an indented code block.
    It doesn't have syntax highlighting.
    But it preserves formatting.

## Code Block with No Language

```
Plain code block
No syntax highlighting
Just monospace font
```

## Long Lines

```python
# This is a very long line that demonstrates how code blocks handle horizontal scrolling or wrapping when content exceeds the available width
def very_long_function_name_that_demonstrates_horizontal_scrolling(parameter1, parameter2, parameter3, parameter4, parameter5):
    result = parameter1 + parameter2 + parameter3 + parameter4 + parameter5
    return result
```

## Code with Line Numbers (if supported)

```python {.line-numbers}
def binary_search(arr, target):
    left, right = 0, len(arr) - 1
    while left <= right:
        mid = (left + right) // 2
        if arr[mid] == target:
            return mid
        elif arr[mid] < target:
            left = mid + 1
        else:
            right = mid - 1
    return -1
```

---

*Run with `kryon run code_blocks.md` to see syntax highlighting in action!*
