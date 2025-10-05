COMPILE PHASE:

src/cli/main.c

src/cli/compile/compile_command.c 

src/core/memory_manager.c

src/compiler/lexer/lexer.c 

src/compiler/parser/parser.c 

rc/core/error_system.c 

src/compiler/codegen/codegen.c 

src/shared/krb_schema.c


RUNTIME PHASE:

src/cli/main.c -> src/cli/run/run_command.c -> src/runtime/core/runtime.c -> src/core/binary_io.c -> src/runtime/elements/elements.c -> src/renderers/raylib/raylib_renderer.c -> src/runtime/core/runtime.c

[RENDER LOOP START]
    -> src/cli/run/run_command.c
    -> src/renderers/raylib/raylib_renderer.c
    -> src/events/events.c
    -> src/runtime/core/runtime.c
    -> src/runtime/behavior_integration.c
    -> src/runtime/expression_evaluator.c
    -> (custom scripting engine pending)
    -> src/runtime/elements/elements.c
    -> src/renderers/raylib/raylib_renderer.c
[RENDER LOOP END]
