# Expression Engine Enhancement - Implementation Plan

## Overview

This document outlines the complete implementation plan for enhancing Kryon's Universal Expression Engine. This is **Priority 1** improvement as it forms the foundation for all reactive features in the framework.

## Current State

The expression system (`ir/ir_executor.c:1641-1689`) has critical limitations:

- Only simple variable references: `{"var": "count"}`
- No nested property access: `user.profile.name`
- No method calls: `array.length()`, `string.toUpperCase()`
- No operator precedence or parentheses
- No expression compilation - everything parsed at runtime from JSON
- No caching of frequently-used expressions

## Target State

After completion, the expression engine will support:

1. **Complex Property Access**
   - `user.profile.settings.theme`
   - `items[0].name`
   - Computed property access: `obj[dynamicKey]`

2. **Method Calls**
   - Built-in methods: `array.length()`, `string.toUpperCase()`
   - Extensible builtin function registry
   - Chained calls: `user.getProfile().getName()`

3. **Advanced Expressions**
   - Ternary operators: `condition ? thenValue : elseValue`
   - Operator precedence: `(a + b) * c`
   - String concatenation: `"Hello, " + name`

4. **Performance**
   - Bytecode compilation (10-100x faster)
   - LRU expression cache
   - Lazy evaluation for conditional branches

## Implementation Phases

| Phase | Title | Duration | Dependencies |
|-------|-------|----------|--------------|
| [Phase 1](./phase-1-data-structures.md) | Core Data Structures & AST Enhancement | 3 days | None |
| [Phase 2](./phase-2-parser-integration.md) | Parser Integration | 4 days | Phase 1 |
| [Phase 3](./phase-3-bytecode-compiler.md) | Bytecode Compiler | 5 days | Phase 1 |
| [Phase 4](./phase-4-evaluation-engine.md) | Evaluation Engine | 4 days | Phase 3 |
| [Phase 5](./phase-5-expression-cache.md) | Expression Cache | 3 days | Phase 4 |
| [Phase 6](./phase-6-builtin-functions.md) | Builtin Function Library | 4 days | Phase 4 |
| [Phase 7](./phase-7-integration-testing.md) | Integration & Testing | 4 days | All phases |
| [Phase 8](./phase-8-optimization.md) | Performance Optimization | 3 days | Phase 5, 7 |

**Total Estimated Time: 30 days (6 weeks)**

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Expression Pipeline                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Source Code                                                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  TSX:  onClick={() => setUser(user.profile.name)}       │   │
│  │  Kry:  on click { set user user.profile.name }          │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  Parser (Existing)                                              │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Output: JSON Expression AST                            │   │
│  │  {"type": "member_access",                              │   │
│  │   "object": {"var": "user"},                            │   │
│  │   "property": "profile"}                                │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  NEW: Bytecode Compiler                                        │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Input:  JSON Expression AST                            │   │
│  │  Output: Compiled Bytecode                              │   │
│  │  [LOAD_VAR "user"]                                      │   │
│  │  [LOAD_PROP "profile"]                                  │   │
│  │  [LOAD_PROP "name"]                                     │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  Expression Cache (NEW)                                         │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Key: Hash of bytecode                                  │   │
│  │  Value: Compiled expression + cached result (if pure)   │   │
│  │  Eviction: LRU with max 256 entries                     │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  Evaluation Engine (NEW)                                        │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Input:  Bytecode + Executor Context                    │   │
│  │  Output: IRValue (int, string, array, object)           │   │
│  │                                                           │   │
│  │  Variable Lookup:                                        │   │
│  │  - Local scope (component instance)                     │   │
│  │  - Parent scope (inherited state)                       │   │
│  │  - Global scope (app state)                             │   │
│  └─────────────────────────────────────────────────────────┘   │
│                           │                                     │
│                           ▼                                     │
│  Result: IRValue (used in statements, bindings)               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## File Structure

```
ir/
├── ir_expression.h              # MODIFY: Add new expression types
├── ir_expression.c              # MODIFY: Implement new expression operations
├── ir_expression_compiler.h     # NEW: Bytecode compiler interface
├── ir_expression_compiler.c     # NEW: Bytecode compilation implementation
├── ir_expression_cache.h        # NEW: Expression cache interface
├── ir_expression_cache.c        # NEW: LRU cache implementation
├── ir_expression_eval.h         # NEW: Evaluation engine interface
├── ir_expression_eval.c         # NEW: Bytecode evaluation
├── ir_builtin_registry.h        # NEW: Builtin function registry
├── ir_builtin_registry.c        # NEW: Builtin function implementations
├── ir_executor.c                # MODIFY: Use new evaluation engine
└── ir_executor.h                # MODIFY: Update context for expression cache

ir/parsers/tsx/
└── tsx_to_kir.ts                # MODIFY: Parse complex expressions

ir/parsers/kry/
└── kry_parser.c                 # MODIFY: Parse property access syntax

tests/
└── expression-engine/           # NEW: Test suite
    ├── test_parser.c
    ├── test_compiler.c
    ├── test_eval.c
    └── test_cache.c
```

## Success Criteria

Each phase has specific acceptance criteria defined in its documentation. Overall success criteria:

1. **Functionality**
   - All expression types from existing code continue to work
   - New expression types (member access, method calls, ternaries) work correctly
   - Parsers output enhanced AST format

2. **Performance**
   - Compiled expressions execute 10x faster than current AST traversal
   - Cache hit rate > 80% for typical applications
   - Memory overhead < 5MB for 1000 cached expressions

3. **Compatibility**
   - All existing tests pass
   - Round-trip conversion (TSX → KIR → TSX) preserves expression semantics
   - Backends can evaluate compiled expressions

4. **Code Quality**
   - No memory leaks (verified with valgrind)
   - Test coverage > 90% for new code
   - Documentation complete with examples

## Getting Started

Start with **[Phase 1: Core Data Structures & AST Enhancement](./phase-1-data-structures.md)**.

Each phase document includes:
- Detailed requirements
- Code examples
- Testing strategy
- Acceptance criteria
- Dependencies on previous phases

## Progress Tracking

As you complete each phase:
1. Check off the acceptance criteria in the phase document
2. Run the test suite: `make test-expression-engine`
3. Update this README with completion status
4. Move to the next phase

---

**Status**: Phase 0 (Planning) - Ready to begin Phase 1
**Last Updated**: 2026-01-11
