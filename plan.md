                  
●User approved Claude's plan:
  ⎿ Comprehensive Kryon-C Refactoring and Directive Serialization Plan

    Phase 1: Fix KRB Widget Instance Writing (Immediate)

    Problem: KRB generation fails with "Failed to write widget instance" because directive nodes (@variables, 
    @event, @component, @function) aren't handled in write_widget_instance() section at codegen.c:1517.

    Fix:
    - Add directive node handling in write_complex_krb_format() around line 1517
    - Skip directive nodes in widget instance section (they're handled elsewhere)
    - Add proper filtering to only process KRYON_AST_ELEMENT nodes for widget instances

    Phase 2: Standardize Naming Convention

    Decision: Use "elements" consistently throughout codebase (not "widgets")
    Rationale: 
    - AST uses KRYON_AST_ELEMENT 
    - Parser uses element_type, element.h
    - KRB format uses "Widget Instance" but this is binary format legacy
    - "Elements" is more web/UI framework standard (React, HTML)

    Changes:
    - Keep KRB binary format as-is for compatibility  
    - Rename all code references from "widget" → "element"
    - Update function names: write_widget_instance → write_element_instance
    - Update variable names: widget_type → element_type
    - Update comments and documentation

    Phase 3: File Refactoring for Maintainability

    Target Large Files:

    3.1 Split codegen.c (3071 lines) into:

    - codegen/krb_writer.c - Core KRB writing logic
    - codegen/element_serializer.c - Element serialization
    - codegen/directive_serializer.c - Directive serialization  
    - codegen/string_table.c - String table management
    - codegen/ast_expander.c - Component expansion logic

    3.2 Split runtime.c (2577 lines) into:

    - runtime/runtime_core.c - Core runtime logic
    - runtime/element_lifecycle.c - Element creation/destruction
    - runtime/event_system.c - Event handling
    - runtime/variable_manager.c - Variable/state management

    3.3 Split parser.c (2081 lines) into:

    - parser/parser_core.c - Core parsing logic
    - parser/directive_parser.c - All directive parsing
    - parser/expression_parser.c - Expression parsing
    - parser/error_recovery.c - Error handling

    Phase 4: Directive Serialization Strategy

    Design: Two-tier approach based on directive lifecycle

    4.1 Compile-Time Directives (Processed & Removed)

    - @const / @const_for - Expanded inline, not stored in KRB
    - @include - Merged into AST, not stored in KRB  
    - @metadata - Merged into App element properties, directive removed

    4.2 Runtime Directives (Stored in KRB)

    - @variables - Stored in Variables section with reactive flags
    - @function - Stored in Script section with language metadata
    - @component - Stored in Component Definition section
    - @event - Stored as Event Handler references

    4.3 KRB Format Extensions

    New Sections:
    - Event Handlers Section: Store @event directive mappings
    - Reactive Variables: Extend Variables section with reactive flags
    - Component Metadata: Store component parameter schemas

    Variable Types:
    0x00 = Static String
    0x01 = Static Integer  
    0x02 = Static Float
    0x03 = Static Boolean
    0x10 = Reactive String (triggers re-render)
    0x11 = Reactive Integer 
    0x12 = Reactive Float
    0x13 = Reactive Boolean

    Phase 5: Complete AST Implementation

    Missing Features:
    - Event directive serialization in KRB
    - Component parameter substitution  
    - Reactive variable dependency tracking
    - State change notification system

    Implementation Order:

    1. Fix directive filtering (1-2 hours)
    2. Rename widgets→elements (2-3 hours)  
    3. Refactor large files (6-8 hours)
    4. Implement directive strategy (8-12 hours)
    5. Complete reactive variables (4-6 hours)

    Testing Strategy:

    - Test each phase with --debug mode (auto-includes inspector.kry)
    - Verify all 17 AST nodes serialize correctly
    - Test reactive variable updates
    - Test component instantiation with parameters


