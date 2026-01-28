# KIR Usage Guide

**Kryon Intermediate Representation (KIR)** - Complete User Guide

---

## Table of Contents

1. [Overview](#overview)
2. [Basic Usage](#basic-usage)
3. [Compilation Pipeline](#compilation-pipeline)
4. [Command Reference](#command-reference)
5. [Round-Trip Examples](#round-trip-examples)
6. [Utilities](#utilities)
7. [Tips & Best Practices](#tips--best-practices)

---

## Overview

KIR (Kryon Intermediate Representation) is a JSON-based intermediate format in the Kryon compilation pipeline. It provides:

- **Human-readable** representation of post-expansion AST
- **Machine-parseable** JSON format for tooling
- **Lossless** round-trip capabilities
- **Decompilation** support (krb → kir → kry)

### Architecture

```
.kry Source → [Compile] → .kir (JSON) → [Codegen] → .krb Binary
                                ↓
                          [Decompile] ← .krb Binary
                                ↓
                           [Print] → .kry Source
```

---

## Basic Usage

### 1. Compile with KIR Output

Generate both KIR and KRB from source:

```bash
kryon compile app.kry --kir-output app.kir

# Output:
# - app.kir (JSON intermediate representation)
# - app.krb (binary executable)
```

### 2. KIR-Only Compilation

Generate KIR without creating KRB (for inspection/debugging):

```bash
kryon compile app.kry --no-krb

# Output:
# - app.kir (auto-generated filename)
```

### 3. Compile from KIR

Compile KIR directly to KRB (skip front-end):

```bash
kryon compile app.kir -o app.krb

# Detects .kir extension and uses KIR reader
```

### 4. Decompile Binary to KIR

Extract KIR from compiled binary:

```bash
kryon decompile app.krb -o app.kir

# Output:
# - app.kir (reconstructed intermediate representation)
```

### 5. Generate Source from KIR

Create readable .kry source from KIR:

```bash
kryon print app.kir -o app.kry

# Output:
# - app.kry (formatted source code)
```

---

## Compilation Pipeline

### Standard Pipeline (kry → kir → krb)

```bash
# Step 1: Compile source to KIR + KRB
kryon compile original.kry --kir-output original.kir

# This generates:
# - original.kir (7-15KB JSON)
# - original.krb (500-2000 bytes binary)
```

**What happens internally:**
1. Lexer: `original.kry` → Tokens
2. Parser: Tokens → Raw AST
3. Transformer: Raw AST → Transformed AST (App root, @include)
4. **KIR Writer: Transformed AST → `original.kir`** ✨
5. KIR Reader: `original.kir` → AST (validates round-trip)
6. Codegen: AST → `original.krb`

### Decompilation Pipeline (krb → kir)

```bash
# Step 2: Decompile binary back to KIR
kryon decompile original.krb -o reconstructed.kir

# This reconstructs the AST from binary format
```

**What happens:**
1. KRB Reader: Parse binary format
2. Decompiler: Reconstruct AST from binary elements/properties
3. **KIR Writer: AST → `reconstructed.kir`** ✨

### Source Generation (kir → kry)

```bash
# Step 3: Generate readable source from KIR
kryon print reconstructed.kir -o roundtrip.kry

# This creates formatted .kry source code
```

**What happens:**
1. KIR Reader: Parse JSON to AST
2. **Printer: AST → `roundtrip.kry`** (formatted source) ✨

---

## Command Reference

### `kryon compile`

Compile Kryon source or KIR files.

```bash
# Basic compilation
kryon compile app.kry

# With KIR output
kryon compile app.kry --kir-output app.kir
kryon compile app.kry -k app.kir

# KIR only (no binary)
kryon compile app.kry --no-krb
kryon compile app.kry -n

# From KIR to KRB
kryon compile app.kir -o app.krb

# Verbose mode
kryon compile app.kry --kir-output app.kir -v
```

**Options:**
- `-o, --output <file>` - Output .krb file path
- `-k, --kir-output <file>` - Output .kir file path
- `-n, --no-krb` - Generate KIR only, skip KRB
- `-v, --verbose` - Enable verbose output
- `-O, --optimize` - Enable optimizations
- `-d, --debug` - Enable debug mode

### `kryon decompile`

Decompile KRB binary to KIR format.

```bash
# Basic decompilation
kryon decompile app.krb

# Specify output
kryon decompile app.krb -o app.kir

# Verbose mode
kryon decompile app.krb -o app.kir -v
```

**Options:**
- `-o, --output <file>` - Output .kir file path (default: input.kir)
- `-v, --verbose` - Show decompilation statistics

**Statistics shown (verbose):**
- Elements decompiled
- Properties decompiled
- Strings recovered
- Total AST nodes

### `kryon print`

Generate readable .kry source from KIR.

```bash
# Basic printing
kryon print app.kir

# Specify output
kryon print app.kir -o app.kry

# Compact formatting
kryon print app.kir -o app.kry --compact

# Readable formatting (extra whitespace)
kryon print app.kir -o app.kry --readable

# Verbose mode
kryon print app.kir -o app.kry -v
```

**Options:**
- `-o, --output <file>` - Output .kry file path (default: input.kry)
- `-c, --compact` - Minimal whitespace
- `-r, --readable` - Generous whitespace
- `-v, --verbose` - Show printing statistics

**Statistics shown (verbose):**
- Elements printed
- Properties printed
- Lines generated
- Total bytes

---

## Utilities

### `kryon kir-dump`

Pretty-print KIR structure in human-readable tree format.

```bash
kryon kir-dump app.kir
```

**Example output:**
```
=== KIR Dump: app.kir ===

ROOT (1 children)
  ELEMENT: App (6 props, 1 children)
    PROP: windowWidth = LITERAL: 800
    PROP: windowHeight = LITERAL: 600
    PROP: windowTitle = LITERAL: "My App"
    ELEMENT: Button (3 props, 0 children)
      PROP: width = LITERAL: 200
      PROP: height = LITERAL: 60
      PROP: text = LITERAL: "Click Me"
```

### `kryon kir-validate`

Validate KIR file structure and integrity.

```bash
# Basic validation
kryon kir-validate app.kir

# Verbose validation
kryon kir-validate app.kir -v
```

**Output:**
- ✅ Validation PASSED - File is valid
- ❌ Validation FAILED - Shows errors

**Checks performed:**
- Valid JSON syntax
- Required fields present
- AST structure integrity
- Node type validity

### `kryon kir-stats`

Show detailed statistics about KIR file.

```bash
# Basic statistics
kryon kir-stats app.kir

# Verbose statistics
kryon kir-stats app.kir -v
```

**Example output:**
```
KIR Statistics: app.kir
================================
Total nodes:      157
Elements:         23
Properties:       67
Literals:         54
Variables:        8
Function calls:   3
Binary ops:       2
Max depth:        6

Ratios:
  Props/element:  2.91
  Children/elem:  0.96
```

### `kryon kir-diff`

Compare two KIR files structurally.

```bash
kryon kir-diff original.kir modified.kir
```

**Example output:**
```
Comparing: original.kir <-> modified.kir
================================
✅ Files are structurally identical
```

or

```
Comparing: original.kir <-> modified.kir
================================
❌ Files differ (5 differences found)
```

---

## Round-Trip Examples

### Full Round-Trip (kry → kir → krb → kir → kry)

Complete cycle demonstrating lossless preservation:

```bash
# Start with source
echo "Button { text = \"Hello\" }" > original.kry

# 1. Compile to KIR + KRB
kryon compile original.kry --kir-output original.kir
# Generates: original.kir, original.krb

# 2. Decompile binary back to KIR
kryon decompile original.krb -o step1.kir

# 3. Compare KIR files
kryon kir-diff original.kir step1.kir
# Should show: ✅ Files are structurally identical

# 4. Generate source from KIR
kryon print step1.kir -o roundtrip.kry

# 5. Compile roundtrip source
kryon compile roundtrip.kry --kir-output roundtrip.kir

# 6. Compare binaries
diff original.krb roundtrip.krb && echo "Perfect round-trip!"
```

### Forward Round-Trip (kry → kir → krb)

Test KIR generation and usage:

```bash
# Compile with KIR
kryon compile app.kry --kir-output app.kir

# Compile from KIR
kryon compile app.kir -o app_from_kir.krb

# Compare binaries
ls -lh app.krb app_from_kir.krb
```

### Backward Round-Trip (krb → kir → kry)

Test decompilation:

```bash
# Decompile existing binary
kryon decompile app.krb -o app.kir

# Generate source
kryon print app.kir -o app_recovered.kry

# Inspect recovered source
cat app_recovered.kry
```

---

## Tips & Best Practices

### When to Use KIR

✅ **Good Use Cases:**
- Debugging compilation issues
- Understanding AST transformations
- Testing compiler changes
- Creating custom tooling
- Automated code analysis
- Compiler intermediate caching

❌ **Not Recommended:**
- Production builds (adds overhead)
- Source control (use .kry instead)
- Distribution (use .krb instead)

### Performance Considerations

**KIR Generation Overhead:**
- Adds ~50-100ms to compilation (depends on file size)
- KIR files are 10-20x larger than KRB
- JSON parsing is slower than binary

**Recommendations:**
- Use `--no-krb` for KIR-only inspection
- Cache KIR files for repeated analysis
- Use binary format for production

### File Size Comparison

| Format | Size (example) | Human-Readable | Executable |
|--------|----------------|----------------|------------|
| .kry   | 800 bytes      | ✅ Yes         | ❌ No      |
| .kir   | 7.9 KB         | ✅ Yes (JSON)  | ❌ No      |
| .krb   | 524 bytes      | ❌ No          | ✅ Yes     |

### Debugging Workflow

```bash
# 1. Compile with KIR to inspect transformations
kryon compile problematic.kry --kir-output debug.kir -v

# 2. Validate KIR structure
kryon kir-validate debug.kir

# 3. Dump KIR to see AST tree
kryon kir-dump debug.kir > debug_tree.txt

# 4. Check statistics
kryon kir-stats debug.kir -v

# 5. Generate source to see normalized output
kryon print debug.kir -o normalized.kry
```

### Version Control

```gitignore
# .gitignore
*.kir      # Don't commit intermediate files
*.krb      # Don't commit binaries
*.kry      # DO commit source files
```

### CI/CD Integration

```bash
#!/bin/bash
# Automated testing with KIR

# Compile test files
for file in tests/*.kry; do
    echo "Testing: $file"

    # Compile with KIR
    kryon compile "$file" --kir-output "${file%.kry}.kir"

    # Validate KIR
    kryon kir-validate "${file%.kry}.kir" || exit 1

    # Test round-trip
    kryon decompile "${file%.krb}.krb" -o "${file%.kry}_rt.kir"
    kryon kir-diff "${file%.kry}.kir" "${file%.kry}_rt.kir" || exit 1
done

echo "All tests passed!"
```

---

## Advanced Examples

### Compare Compiler Versions

```bash
# Compile with old compiler version
kryon-v1.0 compile app.kry --kir-output app_v1.kir

# Compile with new compiler version
kryon-v1.1 compile app.kry --kir-output app_v1.1.kir

# Compare outputs
kryon kir-diff app_v1.kir app_v1.1.kir
```

### Extract AST Statistics

```bash
# Generate stats for all examples
for file in examples/*.kry; do
    kryon compile "$file" --no-krb
    kryon kir-stats "${file%.kry}.kir" -v > "${file%.kry}_stats.txt"
done
```

### Batch Validation

```bash
# Validate all KIR files in directory
find . -name "*.kir" -exec kryon kir-validate {} \;
```

---

## Troubleshooting

### Problem: KIR file won't parse

**Solution:**
```bash
# Check if valid JSON
cat file.kir | jq . > /dev/null

# If invalid, regenerate from source
kryon compile source.kry --kir-output file.kir
```

### Problem: Round-trip produces different binary

**Possible causes:**
- Metadata differences (timestamps, source paths)
- Formatting normalization in printer
- Minor semantic differences (acceptable)

**Check:**
```bash
# Compare KIR files instead
kryon kir-diff original.kir roundtrip.kir
```

### Problem: Decompilation missing information

**Note:** Some information may not be preserved in binary format:
- Comments
- Original formatting
- Some metadata

This is expected - binaries contain runtime information only.

---

## Summary

KIR provides a powerful intermediate representation for:
- **Debugging**: Inspect compiler transformations
- **Testing**: Validate round-trip correctness
- **Tooling**: Build custom analysis tools
- **Learning**: Understand AST structure

**Key Commands:**
- `kryon compile --kir-output` - Generate KIR
- `kryon decompile` - Extract KIR from binary
- `kryon print` - Generate source from KIR
- `kryon kir-*` - Utility commands

For more information, see:
- `docs/KIR_FORMAT_SPEC.md` - Complete format specification
- `docs/KIR_IMPLEMENTATION_STATUS.md` - Implementation details
