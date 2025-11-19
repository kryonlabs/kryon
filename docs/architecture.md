# Kryon Architecture

## Overview

Kryon is a multi-platform UI framework designed with a clean separation between the core logic, rendering backends, and language frontends.

### Core Philosophy
*   **Minimal surfaces, maximal flexibility.**
*   **Hardware-aware abstractions.**
*   **Declarative UI in your application language.**

## System Architecture

### Layered Onion Model

1.  **Application Layer**: Your business logic (Nim/Rust/C/JS/Lua).
2.  **Interface Layer**: Language bindings & macros (e.g., Nim DSL).
3.  **Core Layer**: Component model, state, layout (C ABI).
4.  **Renderer Abstraction**: Primitive drawing commands (C ABI).
5.  **Platform Backends**: Target-specific implementations (SDL3, Framebuffer, Terminal).

### Critical Boundary

The **Core Layer** and **Renderer Abstraction** expose a pure C ABI. This allows:
*   Microcontrollers to link directly to the C core.
*   Desktop apps to use high-level bindings.
*   Web apps to compile to WASM.

## Directory Structure

*   `core/`: Platform-agnostic C engine.
*   `renderers/`: Backend implementations (SDL3, Framebuffer, Terminal).
*   `bindings/`: Language frontends (Nim, Lua).
*   `platforms/`: Platform integration kits.
*   `cli/`: Kryon CLI tool.

## Core Layer (C99 ABI)

The core layer handles:
*   Component tree lifecycle.
*   Unified event system.
*   Flexbox-inspired layout engine.
*   Style resolution.
*   Storage API.

It guarantees:
*   **No global state.**
*   **Deterministic layout.**
*   **No hidden allocations.**
*   **Fixed-point math** for MCU compatibility.

## Renderer Abstraction

All backends must implement a minimal set of primitive commands:
*   `draw_rect`
*   `draw_text`
*   `draw_line`
*   `swap_buffers`

## Language Bindings

### Nim Frontend
The Nim frontend provides a declarative DSL that compiles to C core component trees. It uses macros to provide a React-like developer experience with zero runtime overhead.

### Lua Frontend
The Lua frontend allows dynamic UI definition, suitable for scripting and hot-reloading.
