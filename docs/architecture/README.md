# Kryon Architecture Documentation

This directory contains comprehensive architectural documentation and implementation plans for the Kryon UI framework.

## Overview

Kryon is a universal UI framework with a clean IR-based pipeline that supports multiple frontends and backends.

## Directory Structure

- `plans/` - Detailed implementation plans for each architectural component
- `diagrams/` - Visual architecture diagrams (to be added)
- `specs/` - Technical specifications for formats and protocols

## Core Architecture Principles

1. **Universal IR Pipeline** - All frontends flow through KIR (Kryon Intermediate Representation)
2. **Complete Round-Trip** - KIR preserves all data needed to regenerate any frontend
3. **Dual Execution Modes**:
   - **VM Mode**: KIR → .krb bytecode → VM execution
   - **Binary Mode**: KIR → native code → compiled binary
4. **Clean Separation** - No language-bypassing, everything goes through IR

## Status

🔄 **In Progress** - Currently analyzing codebase and creating detailed implementation plans

## Plans Overview

Each plan addresses a specific architectural component:

- `01-kir-format-complete.md` - Complete KIR format specification
- `02-parser-pipeline.md` - Unified parser pipeline for all frontends
- `03-codegen-pipeline.md` - Complete codegen for all frontends
- `04-bytecode-vm.md` - .krb bytecode format and VM implementation
- `05-binary-compilation.md` - Native binary compilation pipeline
- `06-renderer-backends.md` - Renderer architecture and backends
- `07-metadata-system.md` - Source language metadata preservation

*(Plans will be generated based on codebase analysis)*
