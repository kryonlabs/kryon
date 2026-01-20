#ifndef IR_CORE_H
#define IR_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Split Type Headers (Phase 2 Refactoring)
// These headers contain the basic type definitions that were previously here
// ============================================================================
#include "ir_types.h"
#include "ir_layout.h"
#include "ir_style.h"
#include "ir_properties.h"
#include "ir_events.h"

// ============================================================================
// Forward Declarations
// ============================================================================
typedef struct IRGradient IRGradient;
typedef struct IRPropertyBinding IRPropertyBinding;
typedef struct IRSourceStructures IRSourceStructures;
typedef struct IRImport IRImport;
typedef struct IRReactiveManifest IRReactiveManifest;
typedef struct IRDynamicBinding IRDynamicBinding;

// ============================================================================
// Dynamic Binding (for runtime-evaluated Lua expressions)
// Dependencies are discovered at runtime, not build time.
// ============================================================================
struct IRDynamicBinding {
    char* binding_id;       // Auto-generated unique ID (e.g., "db-310-text")
    char* element_selector; // CSS selector (e.g., "#month-label-1" or "[data-kryon-id='123']")
    char* update_type;      // "text", "property:disabled", "style:backgroundColor", "visibility", "html"
    char* lua_expr;         // Lua expression to evaluate at runtime (returns the new value)
    // NOTE: No deps field - dependencies are discovered at runtime by kryonEvalWithTracking()
};

// ============================================================================
// Logic Source Types
// ============================================================================
typedef enum {
    IR_LOGIC_C,
    IR_LOGIC_LUA,
    IR_LOGIC_WASM,
    IR_LOGIC_NATIVE
} LogicSourceType;

// ============================================================================
// Handler Source - stores actual source code for language-specific handlers
// Used to preserve function source through KIR for roundtrip and web embedding
// ============================================================================
typedef struct IRHandlerSource {
    char* language;      // "lua", "javascript", etc.
    char* code;          // The actual function source code
    char* file;          // Source file name (for debugging/roundtrip)
    int line;            // Source line number

    // Closure metadata for handlers that reference external variables
    // This allows target-specific codegen to add appropriate wrappers
    bool uses_closures;  // True if handler references closure variables
    char** closure_vars;  // Array of closure variable names
    int closure_var_count;  // Number of closure variables
} IRHandlerSource;

// ============================================================================
// Event Handler
// ============================================================================
typedef struct IREvent {
    IREventType type;
    char* event_name;    // String name for plugin events (NULL for core events)
    char* logic_id;      // References IRLogic (legacy, for C callbacks)
    char* handler_data;  // Event-specific data
    uint32_t bytecode_function_id;  // References bytecode function in IRMetadata (0 = none)
    IRHandlerSource* handler_source; // Embedded handler source code for web/roundtrip
    struct IREvent* next;
} IREvent;

// ============================================================================
// Business Logic Container
// ============================================================================
typedef struct IRLogic {
    char* id;
    char* source_code;
    LogicSourceType type;
    struct IRLogic* next;
} IRLogic;

// ============================================================================
// Layout cache for performance optimization
// ============================================================================
// Note: IRDirtyFlags is now defined in ir_layout_types.h for consolidation
// with IRLayoutState. This header is included by ir_layout_types.h.
typedef struct {
    bool dirty;                          // Cache invalid flag
    float cached_intrinsic_width;        // Cached intrinsic width
    float cached_intrinsic_height;       // Cached intrinsic height
    uint32_t cache_generation;           // Generation counter for invalidation
} IRLayoutCache;

// ============================================================================
// Rendered bounds cache (filled during layout/render)
// ============================================================================
typedef struct IRRenderedBounds {
    float x, y, width, height;
    bool valid;  // true if bounds have been calculated
} IRRenderedBounds;

// ============================================================================
// Forward declaration for tab drag visuals
// ============================================================================
struct TabGroupState;

// ============================================================================
// Dropdown State (stored in IRComponent->custom_data)
// ============================================================================
typedef struct IRDropdownState {
    char* placeholder;      // Placeholder text when nothing selected
    char** options;         // Array of option strings
    uint32_t option_count;  // Number of options
    int32_t selected_index; // Currently selected index (-1 = none)
    bool is_open;           // Whether dropdown menu is open
    int32_t hovered_index;  // Currently hovered option index (-1 = none)
} IRDropdownState;

// ============================================================================
// Modal State (stored in IRComponent->custom_data)
// ============================================================================
typedef struct IRModalState {
    bool is_open;            // Whether modal is visible
    char* title;             // Optional title text (NULL if no title bar)
    uint32_t backdrop_color; // Backdrop color (RGBA, default semi-transparent black)
} IRModalState;

// ============================================================================
// Table Component Structures
// ============================================================================

// Table cell data (for colspan/rowspan and alignment)
typedef struct IRTableCellData {
    uint16_t colspan;           // Number of columns this cell spans (default 1)
    uint16_t rowspan;           // Number of rows this cell spans (default 1)
    uint8_t alignment;          // IRAlignment for cell content (horizontal)
    uint8_t vertical_alignment; // IRAlignment for vertical positioning
    bool is_spanned;            // True if this cell position is covered by another cell's span
    uint32_t spanned_by_id;     // ID of the cell that spans over this position (if is_spanned)
} IRTableCellData;

// Table column definition (for auto-sizing and column properties)
typedef struct IRTableColumnDef {
    IRDimension min_width;      // Minimum column width
    IRDimension max_width;      // Maximum column width
    IRDimension width;          // Explicit width (if set)
    IRAlignment alignment;      // Default horizontal alignment for column
    bool auto_size;             // True if width should be calculated from content
} IRTableColumnDef;

// Table styling options
typedef struct IRTableStyle {
    IRColor header_background;    // Background color for header cells
    IRColor even_row_background;  // Background for even rows (striped)
    IRColor odd_row_background;   // Background for odd rows
    IRColor border_color;         // Color for table borders
    float border_width;           // Width of cell borders in pixels
    float cell_padding;           // Padding inside cells in pixels
    bool show_borders;            // Whether to show cell borders
    bool striped_rows;            // Whether to use alternating row colors
    bool header_sticky;           // Whether header sticks to top on scroll
    bool collapse_borders;        // Border-collapse: collapse (true) vs separate (false)
} IRTableStyle;

// Maximum table dimensions
#define IR_MAX_TABLE_COLUMNS 64
#define IR_MAX_TABLE_ROWS 1024

// Runtime table state (stored in IRComponent->custom_data for Table components)
typedef struct IRTableState {
    // Column definitions
    IRTableColumnDef* columns;
    uint32_t column_count;

    // Calculated column widths (after layout)
    float* calculated_widths;

    // Calculated row heights (after layout)
    float* calculated_heights;

    // Row information
    uint32_t row_count;
    uint32_t header_row_count;   // Number of rows in TableHead
    uint32_t footer_row_count;   // Number of rows in TableFoot

    // Table styling
    IRTableStyle style;

    // Cell span tracking (for efficient lookup during layout)
    // 2D array: span_map[row * column_count + col] = IRTableCellData
    IRTableCellData* span_map;
    uint32_t span_map_rows;
    uint32_t span_map_cols;

    // Section component references (for quick access)
    struct IRComponent* head_section;
    struct IRComponent* body_section;
    struct IRComponent* foot_section;

    // Layout cache
    bool layout_valid;
    float cached_total_width;
    float cached_total_height;
    float cached_available_width;   // Input dimensions used for last layout
    float cached_available_height;
} IRTableState;

// ============================================================================
// Markdown Component Structures
// ============================================================================

// Heading data (H1-H6)
typedef struct {
    uint8_t level;          // 1-6 for H1-H6
    char* text;             // Heading text content
    char* id;               // Optional anchor ID for linking (NULL if none)
} IRHeadingData;

// List data
typedef enum {
    IR_LIST_UNORDERED = 0,  // Bullet list (-, *, +)
    IR_LIST_ORDERED = 1     // Numbered list (1., 2., ...)
} IRListType;

typedef struct {
    IRListType type;        // Ordered or unordered
    uint32_t start;         // Starting number (for ordered lists, default 1)
    bool tight;             // Tight vs loose spacing (CommonMark spec)
} IRListData;

// List item data
typedef struct {
    uint32_t number;        // Item number (for ordered lists)
    char* marker;           // Bullet character (-, *, +) or NULL
    bool is_task_item;      // GitHub-style task list item
    bool is_checked;        // Task item checked state
} IRListItemData;

// Blockquote data
typedef struct {
    uint8_t depth;          // Nesting level for nested quotes (1-based)
} IRBlockquoteData;

// Code block data
typedef struct {
    char* language;         // Language tag (e.g., "python", "rust", NULL for none)
    char* code;             // Code content (raw text)
    size_t length;          // Code length in bytes
    bool show_line_numbers; // Enable line numbers
    uint32_t start_line;    // Starting line number (default 1)
    // Syntax highlighting is handled by plugins via web renderer extension point
} IRCodeBlockData;

// Link data
typedef struct {
    char* url;              // Target URL (required)
    char* title;            // Optional title attribute (NULL if none)
    char* target;           // Target window (_blank, _self, _parent, _top)
    char* rel;              // Relationship (noopener, noreferrer, external, etc.)
} IRLinkData;

// Placeholder data (for template placeholders like {{content}}, {{sidebar}})
typedef struct {
    char* name;             // Placeholder name (e.g., "content", "sidebar")
    bool preserve;          // If true, output {{name}} in codegen; if false, must be substituted
} IRPlaceholderData;

// ============================================================================
// Tab-specific data
// ============================================================================
typedef struct {
    char* title;                   // Tab title text
    bool reorderable;              // Can tabs be reordered via drag-and-drop
    int32_t selected_index;        // Currently selected tab index (for TabGroup)
    uint32_t active_background;    // Active tab background color (RGBA)
    uint32_t text_color;           // Tab text color (RGBA)
    uint32_t active_text_color;    // Active tab text color (RGBA)
} IRTabData;

// ============================================================================
// CSS Selector Type (for HTML roundtrip fidelity)
// ============================================================================
typedef enum {
    IR_SELECTOR_NONE = 0,      // No CSS selector (component-only, no styling rule)
    IR_SELECTOR_ELEMENT,       // Element selector: header { }, nav { }
    IR_SELECTOR_CLASS,         // Class selector: .container { }, .hero { }
    IR_SELECTOR_ID             // ID selector: #main { }
} IRSelectorType;

// ============================================================================
// Main IR Component
// ============================================================================
typedef struct IRComponent {
    uint32_t id;
    IRComponentType type;
    IRSelectorType selector_type;  // How to generate CSS selector (element vs class)
    char* tag;           // HTML semantic tag (e.g., "section", "header", "nav")
    char* css_class;     // CSS class name for styling (e.g., "hero", "features")
    IRStyle* style;
    IREvent* events;
    IRLogic* logic;
    struct IRComponent** children;
    uint32_t child_count;
    uint32_t child_capacity;  // Allocated capacity for exponential growth
    IRLayout* layout;
    IRLayoutState* layout_state;  // Two-pass layout state (replaces dirty_flags)
    struct IRComponent* parent;
    char* text_content;      // For text components (evaluated value)
    char* text_expression;   // For reactive text (e.g., "{{value}}" or "$value")
    char* custom_data;       // For custom components
    char* component_ref;     // For component references (e.g., "Counter")
    char* component_props;   // JSON string of props passed to component instance
    char* module_ref;        // For module references to external KIR files (e.g., "components/tabs")
    char* export_name;       // Export name to use from module (e.g., "buildTabsAndPanels")
    uint32_t owner_instance_id;        // ID of owning component instance (for state isolation)
    char* scope;             // Scope string for variable lookups (e.g., "Counter#0", "Counter#1")
    char* source_module;      // Module that created this component (e.g., "components/calendar")
    IRRenderedBounds rendered_bounds;  // Cached layout bounds
    IRLayoutCache layout_cache;        // Performance cache for layout
    uint32_t dirty_flags;              // Dirty tracking for incremental updates
    bool has_active_animations;        // OPTIMIZATION: Track if this subtree has animations (80% reduction)
    IRTextLayout* text_layout;         // Cached multi-line text layout (for IR_COMPONENT_TEXT)
    IRTabData* tab_data;               // Tab-specific data (for tab components)

    // Source preservation fields (for round-trip codegen)
    IRPropertyBinding** property_bindings;  // Property binding metadata
    uint32_t property_binding_count;

    // Dynamic bindings (for runtime Lua expression evaluation in web)
    IRDynamicBinding** dynamic_bindings;    // Array of dynamic binding pointers
    uint32_t dynamic_binding_count;
    uint32_t dynamic_binding_capacity;
    struct {
        char* generated_by;            // ID of source structure that generated this (e.g., "for_1", "static_1")
        uint32_t iteration_index;      // Iteration index if generated by for loop (0-based)
        bool is_template;              // True if this is a template component (not expanded)
    } source_metadata;

    // Conditional rendering support
    char* visible_condition;           // Variable name that controls visibility (e.g., "showMessage")
    bool visible_when_true;            // True if visible when condition is true, false if visible when condition is false

    // ForEach (dynamic list rendering) support - legacy fields
    char* each_source;                 // Reactive variable name to iterate (e.g., "state.calendarDays")
    char* each_item_name;              // Variable name for each item (e.g., "day")
    char* each_index_name;             // Variable name for index (e.g., "index")

    // ForEach (new modular system) - structured definition with bindings
    struct IRForEachDef* foreach_def;  // New structured ForEach definition (see ir_foreach.h)

    // Interaction state
    bool is_disabled;                  // True if component is disabled (for buttons, inputs, etc.)

    // Memory management flag
    bool is_externally_allocated;      // True if allocated via malloc/calloc (not from pool)
} IRComponent;

// ============================================================================
// Forward declarations
// ============================================================================
typedef struct IRComponentPool IRComponentPool;
typedef struct IRComponentMap IRComponentMap;
typedef struct IRBuffer IRBuffer;

// ============================================================================
// IR Metadata (application-wide information)
// ============================================================================
typedef struct IRMetadata {
    uint32_t version;                 // IR format version
    uint32_t component_count;         // Total number of components
    uint32_t max_depth;               // Maximum tree depth

    // Plugin requirements
    char** required_plugins;          // Array of plugin names (e.g., ["canvas", "markdown"])
    uint32_t plugin_count;            // Number of required plugins

    // Window properties (for App component)
    char* window_title;               // Window title (NULL for default)
    int window_width;                 // Window width (0 for default)
    int window_height;                // Window height (0 for default)

    char reserved[16];                // For future expansion
} IRMetadata;

// ============================================================================
// Source file metadata for round-trip serialization
// ============================================================================
typedef struct IRSourceMetadata {
    char* source_language;    // Original language: "tsx", "c", "lua", "kry", "html", "md"
    char* compiler_version;   // Kryon compiler version (e.g., "kryon-1.0.0")
    char* timestamp;          // ISO8601 timestamp when KIR was generated
    char* source_file;        // Path to original source file (for runtime re-execution)
} IRSourceMetadata;

// ============================================================================
// Multi-File Source Preservation (for automatic KIR splitting)
// ============================================================================

// Individual module source file entry for storing original source code in KIR
typedef struct IRModuleSource {
    char* module_id;      // Module identifier (e.g., "main", "components/calendar")
    char* language;       // "lua", "tsx", "kry", etc.
    char* source;         // Original source code
    char* hash;           // SHA-256 hash for change detection
} IRModuleSource;

// Collection of source entries for multi-file KIR
typedef struct IRSourceCollection {
    IRModuleSource* entries;
    uint32_t entry_count;
    uint32_t entry_capacity;
} IRSourceCollection;

// ============================================================================
// Source Preservation Structures (for Kry → KIR → Kry round-trip codegen)
// These structures preserve compile-time constructs (static blocks, const
// declarations, compile-time for loops) that are normally expanded during
// compilation. They enable perfect round-trip code generation.
// ============================================================================

// Property binding metadata (links property value to source expression)
typedef struct IRPropertyBinding {
    char* property_name;        // Property name (e.g., "justifyContent")
    char* source_expr;          // Source expression (e.g., "item.value")
    char* resolved_value;       // Resolved literal value (e.g., "center")
    char* binding_type;         // "static_template" | "const_ref" | "reactive"
} IRPropertyBinding;

// Variable declaration (const/let/var) - compile-time only
typedef struct IRVarDecl {
    char* id;                   // Unique ID (e.g., "const_1")
    char* name;                 // Variable name (e.g., "alignments")
    char* var_type;             // "const" | "let" | "var"
    char* value_type;           // Type hint (e.g., "array", "object", "number")
    char* value_json;           // Value as JSON string
    char* scope;                // "global" | "static:<static_block_id>" | "component:<name>"
} IRVarDecl;

// Compile-time for loop template (ONLY for loops inside static blocks)
// NOTE: Runtime reactive for loops are handled by IRReactiveForLoop
typedef struct IRForLoopData {
    char* id;                   // Unique ID (e.g., "for_1")
    char* parent_id;            // Parent static block ID
    char* iterator_name;        // Loop variable name (e.g., "item")
    char* collection_ref;       // Variable reference (e.g., "alignments")
    char* collection_expr;      // Full collection expression
    IRComponent* template_component;  // Template component (before expansion)
    uint32_t* expanded_component_ids; // IDs of expanded components in root tree
    uint32_t expanded_count;
} IRForLoopData;

// Static block metadata (compile-time code execution block)
typedef struct IRStaticBlockData {
    char* id;                   // Unique ID (e.g., "static_1")
    uint32_t parent_component_id;  // Parent component ID in tree
    uint32_t* children_ids;     // IDs of components generated by this block
    uint32_t children_count;
    char** var_declaration_ids; // IDs of const/let/var declarations in this block
    uint32_t var_decl_count;
    char** for_loop_ids;        // IDs of compile-time for loops in this block
    uint32_t for_loop_count;
} IRStaticBlockData;

// Import statement structure (for KRY import statements: import Math from "math")
typedef struct IRImport {
    char* variable;       // Imported variable name (e.g., "Math", "Storage")
    char* module;         // Module path (e.g., "math", "storage", "components.calendar")
} IRImport;

// ============================================================================
// Struct Type Support
// ============================================================================

// Struct field definition
typedef struct IRStructField {
    char* name;              // Field name
    char* type;              // Field type: "string", "int", "float", "bool", "map"
    char* default_value;     // Default value as string/expression
    uint32_t line;           // Source line for error reporting
} IRStructField;

// Struct type definition
typedef struct IRStructType {
    char* name;              // Struct type name (e.g., "Habit")
    IRStructField** fields;  // Array of field definitions
    uint32_t field_count;    // Number of fields
    uint32_t line;           // Source line for error reporting
} IRStructType;

// Struct instance (instantiation)
typedef struct IRStructInstance {
    char* struct_type;       // Type name being instantiated
    char** field_names;      // Array of field names being set
    char** field_values;     // Array of field values (as JSON/expression strings)
    uint32_t field_count;    // Number of fields
    uint32_t line;           // Source line for error reporting
} IRStructInstance;

// Module export descriptor (for module-level return statements)
typedef struct IRModuleExport {
    char* name;              // Export name (e.g., "COLORS", "getRandomColor", "Habit")
    char* type;              // "function", "constant", "array", "object", "struct"
    char* value_json;        // Value as JSON string (for constants/objects)
    char* function_name;     // If type is "function", reference to IRLogicFunction
    IRStructType* struct_def; // If type is "struct", struct definition
    uint32_t line;           // Source line number
} IRModuleExport;

// Container for all source preservation structures
typedef struct IRSourceStructures {
    // Static blocks (compile-time execution)
    IRStaticBlockData** static_blocks;
    uint32_t static_block_count;
    uint32_t static_block_capacity;

    // Variable declarations (const/let/var)
    IRVarDecl** var_decls;
    uint32_t var_decl_count;
    uint32_t var_decl_capacity;

    // Compile-time for loops (only inside static blocks)
    IRForLoopData** for_loops;
    uint32_t for_loop_count;
    uint32_t for_loop_capacity;

    // Import statements (import Name from "module")
    IRImport** imports;       // Array of import statements
    uint32_t import_count;
    uint32_t import_capacity;

    // Module exports (from module-level return statements)
    IRModuleExport** exports;       // Array of module exports
    uint32_t export_count;
    uint32_t export_capacity;

    // Struct type definitions
    IRStructType** struct_types;    // Array of struct type definitions
    uint32_t struct_type_count;
    uint32_t struct_type_capacity;

    // Next ID counters for generation
    uint32_t next_static_block_id;
    uint32_t next_var_decl_id;
    uint32_t next_for_loop_id;
} IRSourceStructures;

// Forward declaration for stylesheet (defined in ir_stylesheet.h)
typedef struct IRStylesheet IRStylesheet;

// ============================================================================
// Lua Source Module System (for Lua → KIR → Codegen round-trip)
// These structures capture the complete Lua source context in KIR so that
// codegens can generate complete, working code without reading Lua files.
// Note: Prefixed with "IRLua" to avoid conflict with IRLogicFunction in ir_logic.h
// ============================================================================

// Variable captured from Lua source
typedef struct IRLuaVariable {
    char* name;                    // Variable name (e.g., "state", "editingState")
    char* initial_value_source;    // Lua source for initialization (e.g., "{ year = 2024 }")
    bool is_reactive;              // True if created via Reactive.reactive()
    char* var_type;                // "local" | "global" | "reactive"
} IRLuaVariable;

// Function captured from Lua source
typedef struct IRLuaFunc {
    char* name;                    // Function name (e.g., "navigateMonth", "toggleHabitCompletion")
    char* source;                  // Complete function source including "function...end"
    char** parameters;             // Array of parameter names
    uint32_t parameter_count;
    char** closure_vars;           // Variables captured from outer scope
    uint32_t closure_var_count;
    bool is_local;                 // true if "local function", false if global
} IRLuaFunc;

// ForEach data provider expression
typedef struct IRLuaForEachProvider {
    char* container_id;            // ID of the ForEach container (e.g., "foreach-38")
    char* expression;              // Lua expression that generates data (e.g., "Calendar.generateRows(...)")
    char* source_function;         // Function where this ForEach was defined
} IRLuaForEachProvider;

// Complete Lua module captured for codegen
typedef struct IRLuaModule {
    char* name;                    // Module identifier (e.g., "main", "components/calendar")
    char* source_language;         // "lua"
    char* full_source;             // Original complete source code (for debugging/reference)

    // Parsed elements
    IRLuaVariable* variables;
    uint32_t variable_count;
    uint32_t variable_capacity;

    IRLuaFunc* functions;
    uint32_t function_count;
    uint32_t function_capacity;

    IRLuaForEachProvider* foreach_providers;
    uint32_t foreach_provider_count;
    uint32_t foreach_provider_capacity;

    // Module imports (require statements)
    char** imports;                // Array of module names this module requires
    uint32_t import_count;
    uint32_t import_capacity;
} IRLuaModule;

// Collection of all Lua modules in the application
typedef struct IRLuaModuleCollection {
    IRLuaModule* modules;
    uint32_t module_count;
    uint32_t module_capacity;
} IRLuaModuleCollection;

// Lua module API
IRLuaModuleCollection* ir_lua_modules_create(void);
void ir_lua_modules_destroy(IRLuaModuleCollection* collection);

IRLuaModule* ir_lua_modules_add(IRLuaModuleCollection* collection, const char* name, const char* source_language);
IRLuaModule* ir_lua_modules_find(IRLuaModuleCollection* collection, const char* name);

void ir_lua_module_add_variable(IRLuaModule* module, const char* name, const char* initial_value, bool is_reactive, const char* var_type);
void ir_lua_module_add_function(IRLuaModule* module, const char* name, const char* source, bool is_local);
void ir_lua_module_add_import(IRLuaModule* module, const char* import_name);
void ir_lua_module_add_foreach_provider(IRLuaModule* module, const char* container_id, const char* expression, const char* source_function);

// ============================================================================
// Global IR Context
// ============================================================================
typedef struct IRContext {
    IRComponent* root;
    IRLogic* logic_list;
    uint32_t next_component_id;
    uint32_t next_logic_id;
    IRComponentPool* component_pool;  // Memory pool for components
    IRComponentMap* component_map;     // Hash map for fast ID lookups
    IRMetadata* metadata;              // Application metadata (including plugin requirements)
    IRReactiveManifest* reactive_manifest;  // Reactive state and bindings
    IRSourceStructures* source_structures;  // Source preservation (for round-trip codegen)
    IRSourceMetadata* source_metadata;      // Source file metadata (language, file path, etc.)
    IRStylesheet* stylesheet;               // Global stylesheet with CSS rules and variables
    IRLuaModuleCollection* lua_modules;     // Complete Lua source context (for web codegen)
} IRContext;

// ============================================================================
// IR Type System Functions
// ============================================================================
const char* ir_component_type_to_string(IRComponentType type);
const char* ir_event_type_to_string(IREventType type);
const char* ir_logic_type_to_string(LogicSourceType type);
IRComponentType ir_get_component_type(IRComponent* component);
uint32_t ir_get_component_id(IRComponent* component);

// ============================================================================
// Component Tree Accessors (for Lua FFI)
// ============================================================================
uint32_t ir_get_child_count(IRComponent* component);
IRComponent* ir_get_child_at(IRComponent* component, uint32_t index);

// ============================================================================
// IR Layout API (defined in ir_layout.c)
// ============================================================================
void ir_layout_compute(IRComponent* root, float available_width, float available_height);
void ir_layout_compute_table(IRComponent* table, float available_width, float available_height);
void ir_layout_mark_dirty(IRComponent* component);
void ir_component_mark_style_dirty(IRComponent* component);  // Marks dirty for style changes
void ir_layout_mark_render_dirty(IRComponent* component);
void ir_layout_invalidate_subtree(IRComponent* component);
float ir_get_component_intrinsic_width(IRComponent* component);
float ir_get_component_intrinsic_height(IRComponent* component);

// Text measurement callback for two-pass layout system
// Backend-specific text measurement (SDL3 TTF, terminal, etc.)
typedef void (*IRTextMeasureCallback)(const char* text, float font_size,
                                       float max_width, float* out_width, float* out_height);
void ir_layout_set_text_measure_callback(IRTextMeasureCallback callback);

// Text width estimation (used by component modules)
float ir_get_text_width_estimate(const char* text, float font_size);

// Single-pass recursive layout system
void ir_layout_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                           float parent_x, float parent_y);
void ir_layout_compute_tree(IRComponent* root, float viewport_width, float viewport_height);
IRComputedLayout* ir_layout_get_bounds(IRComponent* component);

// ============================================================================
// Text Layout API (defined in ir_layout.c)
// ============================================================================
IRTextLayout* ir_text_layout_create(uint32_t max_lines);
void ir_text_layout_destroy(IRTextLayout* layout);
void ir_text_layout_compute(IRComponent* component, float max_width, float font_size);
float ir_text_layout_get_width(IRTextLayout* layout);
float ir_text_layout_get_height(IRTextLayout* layout);

// Font metrics callback (implemented by backends)
typedef struct {
    float (*get_text_width)(const char* text, uint32_t length, float font_size, const char* font_family);
    float (*get_font_height)(float font_size, const char* font_family);
    float (*get_word_width)(const char* word, uint32_t length, float font_size, const char* font_family);
} IRFontMetrics;

// Global font metrics (set by backend)
extern IRFontMetrics* g_ir_font_metrics;
void ir_set_font_metrics(IRFontMetrics* metrics);

// ============================================================================
// Reactive State Manifest System (for hot reload & state persistence)
// ============================================================================

// Reactive variable value types
typedef enum {
    IR_REACTIVE_TYPE_INT,
    IR_REACTIVE_TYPE_FLOAT,
    IR_REACTIVE_TYPE_STRING,
    IR_REACTIVE_TYPE_BOOL,
    IR_REACTIVE_TYPE_CUSTOM  // For complex types (serialized as JSON)
} IRReactiveVarType;

// Reactive variable value (union for different types)
typedef union {
    int64_t as_int;
    double as_float;
    char* as_string;
    bool as_bool;
} IRReactiveValue;

// Reactive variable descriptor
typedef struct {
    uint32_t id;                    // Unique ID for this reactive variable
    char* name;                     // Variable name (for debugging/reconnection)
    IRReactiveVarType type;         // Type of the value
    IRReactiveValue value;          // Current value
    uint32_t version;               // Version counter (for change tracking)

    // Metadata for reconnection
    char* source_location;          // Source file:line where variable was created
    uint32_t binding_count;         // Number of bindings to components

    // Declarative metadata for .kir v2.1 serialization
    char* type_string;              // Type as string ("int", "float", "string", "bool", "array<T>")
    char* initial_value_json;       // Initial value as JSON string
    char* scope;                    // Scope ("global", "component:123")
} IRReactiveVarDescriptor;

// Binding type (how the binding updates the component)
typedef enum {
    IR_BINDING_TEXT,           // Updates text content
    IR_BINDING_CONDITIONAL,    // Shows/hides component
    IR_BINDING_ATTRIBUTE,      // Updates a component attribute
    IR_BINDING_FOR_LOOP,       // Rebuilds for-loop items
    IR_BINDING_CUSTOM          // Custom update procedure (language-specific)
} IRBindingType;

// Reactive binding (links component to reactive variable)
typedef struct {
    uint32_t component_id;          // ID of the bound component
    uint32_t reactive_var_id;       // ID of the reactive variable
    IRBindingType binding_type;     // Type of binding
    char* expression;               // Expression string (e.g., "$value", "count > 5")
    char* update_code;              // Language-specific update code (optional)
} IRReactiveBinding;

// Reactive conditional (for if/else components)
typedef struct {
    uint32_t component_id;          // Component that is conditionally shown
    char* condition;                // Condition expression (e.g., "count > 5")
    uint32_t* dependent_var_ids;    // Array of reactive var IDs this depends on
    uint32_t dependent_var_count;
    bool last_eval_result;          // Last evaluation result (for caching)
    bool suspended;                 // If true, don't re-initialize (for tab switching)
    // Branch children IDs (for serialization/codegen round-trip)
    uint32_t* then_children_ids;    // Component IDs in then-branch
    uint32_t then_children_count;
    uint32_t* else_children_ids;    // Component IDs in else-branch
    uint32_t else_children_count;
} IRReactiveConditional;

// Reactive for-loop (for dynamic lists)
typedef struct {
    uint32_t parent_component_id;   // Parent container
    char* collection_expr;          // Collection expression (e.g., "items")
    uint32_t collection_var_id;     // Reactive var ID for the collection
    char* loop_variable_name;       // Loop variable name (e.g., "item", "todo")
    char* item_template;            // Template for each item (language-specific)
    uint32_t* child_component_ids;  // Current child components
    uint32_t child_count;
} IRReactiveForLoop;

// Component property definition (for custom components)
typedef struct {
    char* name;                     // Prop name (e.g., "initialValue")
    char* type;                     // Type as string (e.g., "int", "string")
    char* default_value;            // Default value as JSON string
} IRComponentProp;

// Component state variable definition
typedef struct {
    char* name;                     // State var name (e.g., "value")
    char* type;                     // Type as string
    char* initial_expr;             // Initial expression (e.g., "{{initialValue}}")
} IRComponentStateVar;

// Custom component definition
typedef struct {
    char* name;                     // Component name (e.g., "Counter", "HabitPanel")
    char* module_path;              // File path (e.g., "components/habit_panel")
    char* source_module;            // Source module (for runtime loading)
    IRComponentProp* props;         // Array of prop definitions
    uint32_t prop_count;
    IRComponentStateVar* state_vars; // Array of state variable definitions
    uint32_t state_var_count;
    IRComponent* template_root;     // Template component tree
} IRComponentDefinition;

// Source code entry (for round-trip serialization)
typedef struct {
    char* lang;  // Language identifier (e.g., "lua", "js", "c")
    char* code;  // Source code content
} IRSourceEntry;
// ============================================================================
// Complete reactive manifest
struct IRReactiveManifest {
    // Reactive variables
    IRReactiveVarDescriptor* variables;
    uint32_t variable_count;
    uint32_t variable_capacity;

    // Bindings
    IRReactiveBinding* bindings;
    uint32_t binding_count;
    uint32_t binding_capacity;

    // Conditionals
    IRReactiveConditional* conditionals;
    uint32_t conditional_count;
    uint32_t conditional_capacity;

    // For-loops
    IRReactiveForLoop* for_loops;
    uint32_t for_loop_count;
    uint32_t for_loop_capacity;

    // Component definitions (custom components)
    IRComponentDefinition* component_defs;
    uint32_t component_def_count;
    uint32_t component_def_capacity;

    // Source code entries (for round-trip preservation)
    IRSourceEntry* sources;
    uint32_t source_count;
    uint32_t source_capacity;

    // Metadata
    uint32_t next_var_id;
};

// ============================================================================
// Reactive manifest management functions
// ============================================================================
IRReactiveManifest* ir_reactive_manifest_create(void);
void ir_reactive_manifest_destroy(IRReactiveManifest* manifest);

uint32_t ir_reactive_manifest_add_var(IRReactiveManifest* manifest,
                                      const char* name,
                                      IRReactiveVarType type,
                                      IRReactiveValue value);

void ir_reactive_manifest_add_binding(IRReactiveManifest* manifest,
                                      uint32_t component_id,
                                      uint32_t reactive_var_id,
                                      IRBindingType binding_type,
                                      const char* expression);

void ir_reactive_manifest_add_conditional(IRReactiveManifest* manifest,
                                         uint32_t component_id,
                                         const char* condition,
                                         const uint32_t* dependent_var_ids,
                                         uint32_t dependent_var_count);

void ir_reactive_manifest_set_conditional_branches(IRReactiveManifest* manifest,
                                                   uint32_t component_id,
                                                   const uint32_t* then_children_ids,
                                                   uint32_t then_children_count,
                                                   const uint32_t* else_children_ids,
                                                   uint32_t else_children_count);

void ir_reactive_manifest_add_for_loop(IRReactiveManifest* manifest,
                                      uint32_t parent_component_id,
                                      const char* collection_expr,
                                      uint32_t collection_var_id);

IRReactiveVarDescriptor* ir_reactive_manifest_find_var(IRReactiveManifest* manifest,
                                                       const char* name);

IRReactiveVarDescriptor* ir_reactive_manifest_get_var(IRReactiveManifest* manifest,
                                                      uint32_t var_id);

bool ir_reactive_manifest_update_var(IRReactiveManifest* manifest,
                                     uint32_t var_id,
                                     IRReactiveValue new_value);

void ir_reactive_manifest_set_var_metadata(IRReactiveManifest* manifest,
                                           uint32_t var_id,
                                           const char* type_string,
                                           const char* initial_value_json,
                                           const char* scope);

void ir_reactive_manifest_print(IRReactiveManifest* manifest);

void ir_reactive_manifest_add_component_def(IRReactiveManifest* manifest,
                                            const char* name,
                                            IRComponentProp* props,
                                            uint32_t prop_count,
                                            IRComponentStateVar* state_vars,
                                            uint32_t state_var_count,
                                            IRComponent* template_root);

IRComponentDefinition* ir_reactive_manifest_find_component_def(IRReactiveManifest* manifest,
                                                               const char* name);

void ir_reactive_manifest_add_source(IRReactiveManifest* manifest,
                                     const char* lang,
                                     const char* code);

const char* ir_reactive_manifest_get_source(IRReactiveManifest* manifest,
                                            const char* lang);

// ============================================================================
// Source Structures API (for Kry → KIR → Kry round-trip codegen)
// ============================================================================

// Create/destroy source structures container
IRSourceStructures* ir_source_structures_create(void);
void ir_source_structures_destroy(IRSourceStructures* ss);

// Static block management
IRStaticBlockData* ir_source_structures_add_static_block(IRSourceStructures* ss,
                                                          uint32_t parent_component_id);
void ir_static_block_add_child(IRStaticBlockData* block, uint32_t child_id);
void ir_static_block_add_var_decl(IRStaticBlockData* block, const char* var_decl_id);
void ir_static_block_add_for_loop(IRStaticBlockData* block, const char* for_loop_id);

// Variable declaration management
IRVarDecl* ir_source_structures_add_var_decl(IRSourceStructures* ss,
                                              const char* name,
                                              const char* var_type,
                                              const char* value_json,
                                              const char* scope);

// Compile-time for loop management (static blocks only)
IRForLoopData* ir_source_structures_add_for_loop(IRSourceStructures* ss,
                                                  const char* parent_static_id,
                                                  const char* iterator_name,
                                                  const char* collection_ref,
                                                  IRComponent* template_component);
void ir_for_loop_add_expanded_component(IRForLoopData* loop, uint32_t component_id);

// Import management (for KRY import statements)
IRImport* ir_source_structures_add_import(IRSourceStructures* ss,
                                          const char* variable,
                                          const char* module);

// Property binding management
IRPropertyBinding* ir_component_add_property_binding(IRComponent* component,
                                                      const char* property_name,
                                                      const char* source_expr,
                                                      const char* resolved_value,
                                                      const char* binding_type);

// Lookup functions
IRStaticBlockData* ir_source_structures_find_static_block(IRSourceStructures* ss, const char* id);
IRVarDecl* ir_source_structures_find_var_decl(IRSourceStructures* ss, const char* id);
IRForLoopData* ir_source_structures_find_for_loop(IRSourceStructures* ss, const char* id);

// ============================================================================
// Dynamic Binding API (for runtime Lua expression evaluation in web)
// ============================================================================

// Create a new dynamic binding
IRDynamicBinding* ir_dynamic_binding_create(const char* binding_id,
                                             const char* element_selector,
                                             const char* update_type,
                                             const char* lua_expr);

// Destroy a dynamic binding
void ir_dynamic_binding_destroy(IRDynamicBinding* binding);

// Add a dynamic binding to a component
void ir_component_add_dynamic_binding(IRComponent* component,
                                       const char* binding_id,
                                       const char* element_selector,
                                       const char* update_type,
                                       const char* lua_expr);

// Get dynamic binding count for a component
uint32_t ir_component_get_dynamic_binding_count(IRComponent* component);

// Get dynamic binding at index
IRDynamicBinding* ir_component_get_dynamic_binding(IRComponent* component, uint32_t index);

#endif // IR_CORE_H
