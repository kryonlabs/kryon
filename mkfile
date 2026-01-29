# Kryon Native TaijiOS Build
# This mkfile integrates with TaijiOS build system using relative paths

# Set TaijiOS root to parent directory (works in both host and emu)
ROOT=..

# Include TaijiOS configuration
<$ROOT/mkconfig

TARG=kryon
BIN=$ROOT/$OBJDIR/bin

# Core object files (relative to Kryon directory)
OFILES=\
	third-party/cjson/cJSON.$O\
	src/shared/kryon_mappings.$O\
	src/shared/krb_schema.$O\
	src/core/memory_manager.$O\
	src/core/binary_io.$O\
	src/core/arena.$O\
	src/core/color_utils.$O\
	src/core/error_system.$O\
	src/core/containers.$O\
	src/compiler/lexer/unicode.$O\
	src/compiler/lexer/lexer.$O\
	src/compiler/parser/parser.$O\
	src/compiler/parser/ast_utils.$O\
	src/compiler/diagnostics/diagnostics.$O\
	src/compiler/codegen/krb_file.$O\
	src/compiler/codegen/krb_writer.$O\
	src/compiler/codegen/krb_reader.$O\
	src/compiler/codegen/string_table.$O\
	src/compiler/codegen/ast_expression_serializer.$O\
	src/compiler/codegen/event_serializer.$O\
	src/compiler/codegen/element_serializer.$O\
	src/compiler/codegen/directive_serializer.$O\
	src/compiler/codegen/codegen.$O\
	src/compiler/codegen/ast_expander.$O\
	src/compiler/kir/kir_writer.$O\
	src/compiler/kir/kir_reader.$O\
	src/compiler/kir/kir_utils.$O\
	src/compiler/kir/kir_printer.$O\
	src/compiler/krl/krl_lexer.$O\
	src/compiler/krl/krl_parser.$O\
	src/compiler/krl/krl_to_kir.$O\
	src/compiler/krb/krb_decompiler.$O\
	src/runtime/core/runtime.$O\
	src/runtime/core/validation.$O\
	src/runtime/core/component_state.$O\
	src/runtime/core/krb_loader.$O\
	src/runtime/core/state.$O\
	src/runtime/compilation/runtime_compiler.$O\
	src/runtime/elements.$O\
	src/runtime/elements/app.$O\
	src/runtime/elements/button.$O\
	src/runtime/elements/checkbox.$O\
	src/runtime/elements/container.$O\
	src/runtime/elements/dropdown.$O\
	src/runtime/elements/element_mixins.$O\
	src/runtime/elements/grid.$O\
	src/runtime/elements/image.$O\
	src/runtime/elements/input.$O\
	src/runtime/elements/link.$O\
	src/runtime/elements/navigation_utils.$O\
	src/runtime/elements/slider.$O\
	src/runtime/elements/tabbar.$O\
	src/runtime/elements/tab.$O\
	src/runtime/elements/tab_content.$O\
	src/runtime/elements/tabgroup.$O\
	src/runtime/elements/tabpanel.$O\
	src/runtime/elements/text.$O\
	src/runtime/element_behaviors.$O\
	src/runtime/behavior_integration.$O\
	src/runtime/expression_evaluator.$O\
	src/runtime/navigation/navigation.$O\
	src/events/events.$O\
	src/cli/main.$O\
	src/cli/run_command.$O\
	src/cli/compile_command.$O\
	src/cli/decompile_command.$O\
	src/cli/print_command.$O\
	src/cli/kir_commands.$O\
	src/cli/debug_command.$O\
	src/cli/dev_command.$O\
	src/cli/package_command.$O\
	src/services/services_registry.$O\
	src/services/services_null.$O\
	src/plugins/inferno/plugin.$O\
	src/plugins/inferno/extended_file_io.$O\
	src/plugins/inferno/namespace.$O\
	src/plugins/inferno/process_control.$O\

# Append Kryon's include paths to TaijiOS CFLAGS
CFLAGS=$CFLAGS -Iinclude -Isrc -Ithird-party/cjson

# Link with lib9 (Plan 9 API) and math library
LIBS=9
SYSLIBS=-lm

# Use TaijiOS build template
<$ROOT/mkfiles/mkone-$SHELLTYPE
