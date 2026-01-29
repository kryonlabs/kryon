# Inferno Shell (sh) in Kryon

This guide explains how to use sh shell (Plan 9/Inferno shell) for writing event handlers and functions in Kryon applications.

## Overview

sh is the shell from Plan 9 and Inferno operating systems. It provides a clean, powerful scripting language with excellent support for pipelines and system integration. Kryon allows you to write functions in sh shell to leverage existing shell scripts, system utilities, and TaijiOS/Inferno services.

## Basic Syntax

### Declaring sh Shell Functions

Specify the language as `"sh"` before the function name:

```kry
function "sh" myFunction() {
    echo Hello from sh shell!
}
```

### Accessing Kryon Variables

Use built-in commands to interact with Kryon application state:

```kry
var count = 0

function "sh" increment() {
    # Get current value
    count=`{kryonget count}

    # Increment
    count=`{expr $count + 1}

    # Set new value
    kryonset count $count
}
```

## Built-in Kryon Commands

### `kryonget`

Retrieves the value of a Kryon variable.

**Syntax:**
```sh
value=`{kryonget varname}
```

**Example:**
```kry
var username = "Alice"

function "sh" greet() {
    name=`{kryonget username}
    echo Hello, $name!
}
```

### `kryonset`

Sets a Kryon variable to a new value.

**Syntax:**
```sh
kryonset varname value
```

**Example:**
```kry
var message = ""

function "sh" updateMessage() {
    kryonset message 'Updated at: '^`{date}
}
```

## sh Shell Language Basics

### Variables

```sh
# Simple assignment
name=Alice
count=42

# Command substitution
now=`{date}
files=`{ls *.kry}

# String concatenation
greeting='Hello, '^$name
```

### Command Execution

```sh
# Run a command
echo Hello, world!

# Pipeline
cat file.txt | grep pattern | wc -l

# Command substitution
result=`{cat file.txt}
```

### Conditionals

```sh
# if statement
if (test -f file.txt) {
    echo File exists
}

if (test $count -gt 10) {
    echo Count is greater than 10
}

# if-else
if (~ $status 0) {
    echo Success
}
if not {
    echo Failed
}
```

### Loops

```sh
# for loop over list
for (file in *.kry) {
    echo Processing $file
}

# for loop with range
for (i in `{seq 1 10}) {
    echo Iteration $i
}

# while loop
while (test $count -lt 100) {
    count=`{expr $count + 1}
}
```

### Functions

```sh
# Define a function
fn greet {
    echo Hello, $1!
}

# Call it
greet Alice
```

## Practical Examples

### Counter Application

```kry
var count = 0

function "sh" increment() {
    count=`{kryonget count}
    count=`{expr $count + 1}
    kryonset count $count
}

function "sh" decrement() {
    count=`{kryonget count}
    count=`{expr $count - 1}
    kryonset count $count
}

function "sh" reset() {
    kryonset count 0
}

App {
    Column {
        Text { text = "Count: ${count}" }

        Row {
            Button { text = "+"; onClick = "increment" }
            Button { text = "-"; onClick = "decrement" }
            Button { text = "Reset"; onClick = "reset" }
        }
    }
}
```

### File System Integration

```kry
var fileList = ""
var fileCount = 0

function "sh" refreshFiles() {
    # Get list of .kry files
    files=`{ls *.kry | tr '\n' ', '}
    kryonset fileList $files

    # Count files
    count=`{ls *.kry | wc -l | tr -d ' '}
    kryonset fileCount $count
}

App {
    Column {
        Text { text = "Files: ${fileList}" }
        Text { text = "Count: ${fileCount}" }
        Button {
            text = "Refresh"
            onClick = "refreshFiles"
        }
    }
}
```

### Date and Time

```kry
var currentTime = ""
var formattedDate = ""

function "sh" updateTime() {
    # Get current time
    time=`{date '+%H:%M:%S'}
    kryonset currentTime $time

    # Get formatted date
    date=`{date '+%Y-%m-%d'}
    kryonset formattedDate $date
}

App {
    Column {
        Text { text = "Time: ${currentTime}" }
        Text { text = "Date: ${formattedDate}" }
        Button {
            text = "Update"
            onClick = "updateTime"
        }
    }
}
```

### System Command Integration

```kry
var diskUsage = ""
var memoryInfo = ""

function "sh" getSystemInfo() {
    # Get disk usage
    usage=`{du -sh . | awk '{print $1}'}
    kryonset diskUsage $usage

    # Get memory info (example - adjust for TaijiOS)
    mem=`{cat /dev/sysctl | grep memory | head -1}
    kryonset memoryInfo $mem
}

App {
    Column {
        Text { text = "Disk Usage: ${diskUsage}" }
        Text { text = "Memory: ${memoryInfo}" }
        Button {
            text = "Refresh"
            onClick = "getSystemInfo"
        }
    }
}
```

### Form Validation

```kry
var email = ""
var isValid = false

function "sh" validateEmail() {
    # Get email value
    addr=`{kryonget email}

    # Simple validation: check for @ symbol
    if (echo $addr | grep -q '@') {
        kryonset isValid true
    }
    if not {
        kryonset isValid false
    }
}

App {
    Column {
        Input {
            value = email
            onChange = "validateEmail"
        }

        Text {
            text = isValid ? "✓ Valid" : "✗ Invalid"
            color = isValid ? "#00ff00" : "#ff0000"
        }
    }
}
```

## TaijiOS/Inferno Configuration

### How sh is Executed

Kryon uses the Inferno emulator to execute sh shell scripts. The runtime configuration is:

```sh
emu -r. dis/sh.dis
```

This:
- Launches the Inferno emulator (`emu`)
- Sets the root directory to current directory (`-r.`)
- Runs the sh shell bytecode (`dis/sh.dis`)

### Environment Setup

The Kryon runtime automatically:
1. Creates a temporary sh script file with your function code
2. Injects `kryonget` and `kryonset` as shell functions
3. Executes the script using `emu -r. dis/sh.dis`
4. Captures the output and updates Kryon state accordingly

### Available Utilities

Since sh runs in the Inferno environment, you have access to:

- **Text processing**: `grep`, `sed`, `awk`, `tr`, `cut`, `sort`, `uniq`
- **File operations**: `ls`, `cat`, `cp`, `mv`, `rm`, `mkdir`, `du`
- **System info**: `date`, `ps`, `wc`, `file`
- **Networking**: `wget`, `curl` (if available in your Inferno setup)
- **Custom utilities**: Any Inferno programs in your path

## Best Practices

### When to Use sh Shell

✅ **Good use cases:**
- Calling system commands
- File system operations
- Text processing with grep/sed/awk
- Integrating with existing shell scripts
- System monitoring
- Batch operations

❌ **Avoid sh shell for:**
- Simple arithmetic (use native Kryon)
- Direct widget state management (use native Kryon)
- Complex application logic (use native Kryon)
- Performance-critical code (shell has overhead)

### Performance Considerations

Each sh shell function call:
1. Creates a temporary script file
2. Spawns an Inferno emulator process
3. Executes the script
4. Parses the output

This has overhead compared to native Kryon functions. For performance-critical operations, prefer native Kryon.

### Error Handling

```kry
function "sh" safeOperation() {
    # Check if file exists before processing
    if (test -f data.txt) {
        result=`{cat data.txt}
        kryonset output $result
    }
    if not {
        kryonset output 'File not found'
    }
}
```

### Debugging

Add debug output to your sh functions:

```kry
function "sh" debugExample() {
    echo 'Starting function' >[1=2]  # Stderr

    value=`{kryonget myvar}
    echo 'Got value:' $value >[1=2]

    # ... rest of function
}
```

The Kryon runtime will capture stderr output for debugging.

## Mixing Languages

You can mix sh shell and native Kryon functions in the same file:

```kry
var count = 0
var status = ""

// Native Kryon - fast, simple
function incrementNative() {
    count += 1
}

// sh shell - for system integration
function "sh" incrementAndLog() {
    count=`{kryonget count}
    count=`{expr $count + 1}
    kryonset count $count

    # Log to system
    echo `{date} ': count = ' $count >> app.log
}

App {
    Column {
        Button {
            text = "Fast Increment"
            onClick = "incrementNative"
        }

        Button {
            text = "Increment + Log"
            onClick = "incrementAndLog"
        }
    }
}
```

## Common Patterns

### Reading Input

```kry
var userInput = ""

function "sh" processInput() {
    input=`{kryonget userInput}

    # Process the input
    result=`{echo $input | tr '[a-z]' '[A-Z]'}

    kryonset userInput $result
}
```

### File Operations

```kry
var fileContent = ""

function "sh" loadFile() {
    if (test -f config.txt) {
        content=`{cat config.txt}
        kryonset fileContent $content
    }
}

function "sh" saveFile() {
    content=`{kryonget fileContent}
    echo $content > config.txt
}
```

### Conditional Updates

```kry
var value = 0
var message = ""

function "sh" updateConditionally() {
    val=`{kryonget value}

    if (test $val -gt 10) {
        kryonset message 'Value is large'
    }
    if not {
        kryonset message 'Value is small'
    }
}
```

## References

- **sh shell documentation**: See Plan 9 or Inferno documentation
- **TaijiOS setup**: Check your TaijiOS installation for available utilities
- **Kryon language spec**: [KRY_LANGUAGE_SPEC.md](KRY_LANGUAGE_SPEC.md)
- **KRB binary format**: [KRB_BINARY_SPEC.md](KRB_BINARY_SPEC.md)

## Troubleshooting

### Function Not Executing

1. Check that `emu` is in your PATH
2. Verify `dis/sh.dis` exists in your Inferno installation
3. Check Kryon runtime logs for error messages

### Variable Not Updating

1. Ensure you're using `kryonset` to update Kryon variables
2. Check variable name spelling matches exactly
3. Verify the function is being called (add debug echo statements)

### Command Not Found

1. Check that the command exists in your Inferno environment
2. Use full paths if necessary: `/bin/ls` instead of `ls`
3. Check PATH environment variable in the sh shell

### Syntax Errors

1. Remember sh shell syntax is different from bash/sh
2. Use `~` for string comparison, not `==`
3. Use `test` for file/number comparisons
4. Parentheses are required for conditionals: `if (test ...) { ... }`

## License

This documentation is part of the Kryon project and follows the same license.
