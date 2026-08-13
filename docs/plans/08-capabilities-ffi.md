# Phase 08: Capabilities And FFI

## Goal

Define how portable cartridges call platform services and app-specific native
code without pretending arbitrary C is automatically portable.

## Current State

KRB has host import names and bind slots. It does not yet have a complete
capability manifest, signatures, optional imports, or platform configuration.

## Target Design

KRB supports two import layers:

- standard capabilities, such as storage, files, HTTP, clipboard, dialogs,
  timers, audio, and notifications
- app-specific host imports for native extensions controlled by the app shell

Imports are named, versioned, typed, and marked required or optional. Allowed
boundary types are explicit: integers, floats, booleans, strings, bytes,
handles, and records/arrays owned by KRB memory. Raw host pointers do not cross
the portable boundary.

## Implementation Phases

1. Define import signature encoding in KIR and KRB.
2. Define required/optional import metadata and loader diagnostics.
3. Add host registration APIs for capabilities and app imports.
4. Add a small standard capability set, starting with logging, timers, storage,
   and simple files.
5. Add platform config files that bind imports to native implementations.

## Tests And Acceptance

- A cartridge requiring a missing capability fails clearly at load time.
- Optional imports can be absent and queried by logic.
- A native host can bind a test import and receive typed arguments.
- The same cartridge can run against two different host implementations of the
  same capability.

## Risks

- Raw C ABI leakage would make KRB non-portable and unsafe.
- Too broad a standard library too early will slow renderer implementations.
