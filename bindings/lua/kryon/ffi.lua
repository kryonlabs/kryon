-- Kryon FFI Bindings
-- Direct LuaJIT FFI bindings to the C IR core (libkryon_ir.so)
-- These bindings provide zero-overhead access to the IR API

local ffi = require("ffi")

-- Determine library name based on platform
-- Try to find the library in common locations
local function findLibrary()
  -- Try environment variable first
  local custom_path = os.getenv("KRYON_LIB_PATH")
  if custom_path then
    return custom_path
  end

  -- Determine library name
  local lib_name
  if ffi.os == "OSX" then
    lib_name = "libkryon_ir.dylib"
  elseif ffi.os == "Windows" then
    lib_name = "kryon_ir.dll"
  else
    lib_name = "libkryon_ir.so"
  end

  -- Just return the library name without path to let the system handle it
  -- This ensures we use whichever libkryon_ir.so is in LD_LIBRARY_PATH
  -- and prevents loading multiple copies of the library
  return lib_name
end

local lib_name = findLibrary()

-- Load the IR core library
local C = ffi.load(lib_name)

-- Load the desktop renderer library (for runDesktop)
local desktop_lib_name = lib_name:gsub("libkryon_ir", "libkryon_desktop")
local Desktop = nil
local success, err = pcall(function()
  Desktop = ffi.load(desktop_lib_name)
end)
if not success then
  io.stderr:write("⚠️ Failed to load desktop library '" .. desktop_lib_name .. "': " .. tostring(err) .. "\n")
  io.stderr:write("   Desktop rendering features will not be available.\n")
end

-- C type definitions (from ir_core.h and ir_builder.h)
ffi.cdef[[
  // ============================================================================
  // Component Types (ir_core.h)
  // ============================================================================
  typedef enum {
    IR_COMPONENT_CONTAINER = 0,
    IR_COMPONENT_TEXT = 1,
    IR_COMPONENT_BUTTON = 2,
    IR_COMPONENT_INPUT = 3,
    IR_COMPONENT_CHECKBOX = 4,
    IR_COMPONENT_DROPDOWN = 5,
    IR_COMPONENT_ROW = 6,
    IR_COMPONENT_COLUMN = 7,
    IR_COMPONENT_CENTER = 8,
    IR_COMPONENT_IMAGE = 9,
    IR_COMPONENT_CANVAS = 10,
    IR_COMPONENT_NATIVE_CANVAS = 11,
    IR_COMPONENT_MARKDOWN = 12,
    IR_COMPONENT_SPRITE = 13,
    IR_COMPONENT_TAB_GROUP = 14,
    IR_COMPONENT_TAB_BAR = 15,
    IR_COMPONENT_TAB = 16,
    IR_COMPONENT_TAB_CONTENT = 17,
    IR_COMPONENT_TAB_PANEL = 18,
    IR_COMPONENT_MODAL = 19,
    IR_COMPONENT_TABLE = 20,
    IR_COMPONENT_TABLE_HEAD = 21,
    IR_COMPONENT_TABLE_BODY = 22,
    IR_COMPONENT_TABLE_FOOT = 23,
    IR_COMPONENT_TABLE_ROW = 24,
    IR_COMPONENT_TABLE_CELL = 25,
    IR_COMPONENT_TABLE_HEADER_CELL = 26,
    IR_COMPONENT_HEADING = 27,
    IR_COMPONENT_PARAGRAPH = 28,
    IR_COMPONENT_BLOCKQUOTE = 29,
    IR_COMPONENT_CODE_BLOCK = 30,
    IR_COMPONENT_HORIZONTAL_RULE = 31,
    IR_COMPONENT_LIST = 32,
    IR_COMPONENT_LIST_ITEM = 33,
    IR_COMPONENT_LINK = 34,
    // Inline semantic components
    IR_COMPONENT_SPAN = 35,
    IR_COMPONENT_STRONG = 36,
    IR_COMPONENT_EM = 37,
    IR_COMPONENT_CODE_INLINE = 38,
    IR_COMPONENT_SMALL = 39,
    IR_COMPONENT_MARK = 40,
    IR_COMPONENT_CUSTOM = 41,
    // Source structure types (for round-trip codegen)
    IR_COMPONENT_STATIC_BLOCK = 42,
    IR_COMPONENT_FOR_LOOP = 43,
    IR_COMPONENT_FOR_EACH = 44,
    IR_COMPONENT_VAR_DECL = 45,
    IR_COMPONENT_PLACEHOLDER = 46
  } IRComponentType;

  // ============================================================================
  // Dimension Types
  // ============================================================================
  typedef enum {
    IR_DIMENSION_AUTO = 0,
    IR_DIMENSION_PX = 1,
    IR_DIMENSION_PERCENT = 2,
    IR_DIMENSION_FLEX = 3,
    IR_DIMENSION_VW = 4,
    IR_DIMENSION_VH = 5,
    IR_DIMENSION_VMIN = 6,
    IR_DIMENSION_VMAX = 7,
    IR_DIMENSION_REM = 8,
    IR_DIMENSION_EM = 9
  } IRDimensionType;

  // CSS Selector Type (for HTML roundtrip fidelity)
  typedef enum {
    IR_SELECTOR_NONE = 0,      // No CSS selector (component-only, no styling rule)
    IR_SELECTOR_ELEMENT,       // Element selector: header { }, nav { }
    IR_SELECTOR_CLASS,         // Class selector: .container { }, .hero { }
    IR_SELECTOR_ID             // ID selector: #main { }
  } IRSelectorType;

  typedef struct {
    IRDimensionType type;
    float value;
  } IRDimension;

  // ============================================================================
  // Color Types
  // ============================================================================
  typedef enum {
    IR_COLOR_SOLID = 0,
    IR_COLOR_TRANSPARENT = 1,
    IR_COLOR_GRADIENT = 2,
    IR_COLOR_VAR_REF = 3
  } IRColorType;

  typedef uint16_t IRStyleVarId;

  // ============================================================================
  // Alignment Types
  // ============================================================================
  typedef enum {
    IR_ALIGNMENT_START = 0,
    IR_ALIGNMENT_CENTER = 1,
    IR_ALIGNMENT_END = 2,
    IR_ALIGNMENT_STRETCH = 3,
    IR_ALIGNMENT_SPACE_BETWEEN = 4,
    IR_ALIGNMENT_SPACE_AROUND = 5,
    IR_ALIGNMENT_SPACE_EVENLY = 6
  } IRAlignment;

  // ============================================================================
  // Text Alignment
  // ============================================================================
  typedef enum {
    IR_TEXT_ALIGN_LEFT = 0,
    IR_TEXT_ALIGN_RIGHT = 1,
    IR_TEXT_ALIGN_CENTER = 2,
    IR_TEXT_ALIGN_JUSTIFY = 3
  } IRTextAlign;

  // ============================================================================
  // Event Types
  // ============================================================================
  typedef enum {
    // Core events (0-99) - hardcoded for performance
    IR_EVENT_CLICK = 0,
    IR_EVENT_HOVER = 1,
    IR_EVENT_FOCUS = 2,
    IR_EVENT_BLUR = 3,
    IR_EVENT_TEXT_CHANGE = 4,  // Text input change event (for Input components)
    IR_EVENT_KEY = 5,
    IR_EVENT_SCROLL = 6,
    IR_EVENT_TIMER = 7,
    IR_EVENT_CUSTOM = 8,

    // Plugin event range (100-255) - dynamically registered
    IR_EVENT_PLUGIN_START = 100,
    IR_EVENT_PLUGIN_END = 255
  } IREventType;

  // ============================================================================
  // Opaque Types (forward declarations)
  // ============================================================================
  typedef struct IRStyle IRStyle;
  typedef struct IRLayout IRLayout;
  typedef struct IREvent IREvent;
  typedef struct IRGradient IRGradient;
  typedef struct TabGroupState TabGroupState;
  typedef struct IRLayoutState IRLayoutState;
  typedef struct IRRenderedBounds IRRenderedBounds;
  typedef struct IRLayoutCache IRLayoutCache;
  typedef struct IRTextLayout IRTextLayout;
  typedef struct IRTabData IRTabData;
  typedef struct IRLogic IRLogic;
  typedef struct IRComponentPool IRComponentPool;
  typedef struct IRComponentMap IRComponentMap;
  typedef struct IRMetadata IRMetadata;

  // ============================================================================
  // Source Metadata (for KIR compilation tracking)
  // ============================================================================
  typedef struct {
    char* source_language;    // Original language: "tsx", "c", "nim", "lua", "kry", "html", "md"
    char* compiler_version;   // Kryon compiler version (e.g., "kryon-1.0.0")
    char* timestamp;          // ISO8601 timestamp when KIR was generated
    char* source_file;        // Path to original source file (for runtime re-execution)
  } IRSourceMetadata;

  // ============================================================================
  // Modal State (for modal overlay components)
  // ============================================================================
  typedef struct {
    bool is_open;            // Whether modal is visible
    char* title;             // Optional title text (NULL if no title bar)
    uint32_t backdrop_color; // Backdrop color (RGBA, default semi-transparent black)
  } IRModalState;

  // IRComponent struct (partial definition for FFI access to text_content)
  // NOTE: Field order MUST match C struct exactly or offsets will be wrong
  typedef struct IRComponent {
    uint32_t id;
    IRComponentType type;
    IRSelectorType selector_type;  // Must match C struct offset
    char* tag;
    char* css_class;               // Must match C struct offset
    IRStyle* style;
    IREvent* events;
    IRLogic* logic;
    struct IRComponent** children;
    uint32_t child_count;
    uint32_t child_capacity;
    IRLayout* layout;
    IRLayoutState* layout_state;
    struct IRComponent* parent;
    char* text_content;
    char* text_expression;
    char* custom_data;
    char* component_ref;
    char* component_props;
    uint32_t owner_instance_id;
    char* scope;
    // Skipping: rendered_bounds, layout_cache, dirty_flags, has_active_animations, text_layout, tab_data
    // Skipping: property_bindings, property_binding_count
    // Skipping: source_metadata
    char* visible_condition;
    bool visible_when_true;

    // ForEach (dynamic list rendering) support
    char* each_source;       // Reactive variable name to iterate
    char* each_item_name;    // Variable name for each item
    char* each_index_name;   // Variable name for index

    bool is_disabled;

    // Additional fields exist in C struct but are not needed for FFI
  } IRComponent;

  // IRContext struct (partial definition for FFI access to metadata)
  typedef struct IRContext {
    IRComponent* root;
    IRLogic* logic_list;
    uint32_t next_component_id;
    uint32_t next_logic_id;
    IRComponentPool* component_pool;
    IRComponentMap* component_map;
    IRMetadata* metadata;
    void* reactive_manifest;
    void* source_structures;         // CRITICAL: Must match C struct field order
    IRSourceMetadata* source_metadata;  // Source file metadata (now at correct offset)
    // Additional fields exist in C struct but are not needed for FFI
  } IRContext;

  // ============================================================================
  // Context Management (ir_builder.h)
  // ============================================================================
  IRContext* ir_create_context(void);
  void ir_destroy_context(IRContext* context);
  void ir_set_context(IRContext* context);
  IRComponent* ir_get_root(void);
  void ir_set_root(IRComponent* root);
  void ir_set_window_metadata(int width, int height, const char* title);

  // ============================================================================
  // Component Creation (ir_builder.h)
  // ============================================================================
  IRComponent* ir_create_component(IRComponentType type);
  IRComponent* ir_create_component_with_id(IRComponentType type, uint32_t id);
  void ir_destroy_component(IRComponent* component);

  // ============================================================================
  // Tree Management (ir_builder.h)
  // ============================================================================
  void ir_add_child(IRComponent* parent, IRComponent* child);
  void ir_remove_child(IRComponent* parent, IRComponent* child);
  void ir_insert_child(IRComponent* parent, IRComponent* child, uint32_t index);
  IRComponent* ir_get_child(IRComponent* component, uint32_t index);
  IRComponent* ir_find_component_by_id(IRComponent* root, uint32_t id);

  // ============================================================================
  // Style Management (ir_builder.h)
  // ============================================================================
  IRStyle* ir_create_style(void);
  void ir_destroy_style(IRStyle* style);
  void ir_set_style(IRComponent* component, IRStyle* style);
  IRStyle* ir_get_style(IRComponent* component);

  // ============================================================================
  // Style Property Setters (ir_builder.h)
  // ============================================================================
  void ir_set_width(IRComponent* component, IRDimensionType type, float value);
  void ir_set_height(IRComponent* component, IRDimensionType type, float value);
  void ir_set_visible(IRStyle* style, bool visible);
  void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
  void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius);
  void ir_set_margin(IRComponent* component, float top, float right, float bottom, float left);
  void ir_set_padding(IRComponent* component, float top, float right, float bottom, float left);
  void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic);
  void ir_set_z_index(IRStyle* style, uint32_t z_index);
  void ir_set_opacity(IRStyle* style, float opacity);
  void ir_set_disabled(IRComponent* component, bool disabled);

  // Text effects
  void ir_set_text_max_width(IRStyle* style, IRDimensionType type, float value);
  void ir_set_text_align(IRStyle* style, IRTextAlign align);
  void ir_set_font_weight(IRStyle* style, uint16_t weight);
  void ir_set_line_height(IRStyle* style, float line_height);
  void ir_set_letter_spacing(IRStyle* style, float spacing);
  void ir_set_word_spacing(IRStyle* style, float spacing);

  // ============================================================================
  // Layout Management (ir_builder.h)
  // ============================================================================
  IRLayout* ir_create_layout(void);
  void ir_destroy_layout(IRLayout* layout);
  void ir_set_layout(IRComponent* component, IRLayout* layout);
  IRLayout* ir_get_layout(IRComponent* component);

  // Layout property setters
  void ir_set_flexbox(IRLayout* layout, bool wrap, uint32_t gap, IRAlignment main_axis, IRAlignment cross_axis);
  void ir_set_flex_properties(IRLayout* layout, uint8_t grow, uint8_t shrink, uint8_t direction);
  void ir_set_justify_content(IRLayout* layout, IRAlignment justify);
  void ir_set_align_items(IRLayout* layout, IRAlignment align);
  void ir_set_align_content(IRLayout* layout, IRAlignment align);
  void ir_set_min_width(IRLayout* layout, IRDimensionType type, float value);
  void ir_set_min_height(IRLayout* layout, IRDimensionType type, float value);
  void ir_set_max_width(IRLayout* layout, IRDimensionType type, float value);
  void ir_set_max_height(IRLayout* layout, IRDimensionType type, float value);
  void ir_set_aspect_ratio(IRLayout* layout, float ratio);

  // ============================================================================
  // Content Management (ir_builder.h)
  // ============================================================================
  void ir_set_text_content(IRComponent* component, const char* text);
  void ir_set_custom_data(IRComponent* component, const char* data);
  void ir_set_tag(IRComponent* component, const char* tag);
  void ir_set_each_source(IRComponent* component, const char* source);
  void ir_set_component_module_ref(IRComponent* component, const char* module_ref, const char* export_name);
  char* ir_clear_tree_module_refs_json(IRComponent* component);
  void ir_restore_tree_module_refs_json(IRComponent* component, const char* json_str);
  char* ir_clear_component_module_ref(IRComponent* component);
  void ir_restore_component_module_ref(IRComponent* component, const char* json_str);

  // ============================================================================
  // Event Management (ir_builder.h)
  // ============================================================================
  IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data);
  void ir_destroy_event(IREvent* event);
  void ir_add_event(IRComponent* component, IREvent* event);
  void ir_remove_event(IRComponent* component, IREvent* event);
  IREvent* ir_find_event(IRComponent* component, IREventType type);

  // Handler Source (for Lua source preservation in KIR)
  typedef struct IRHandlerSource {
    char* language;
    char* code;
    char* file;
    int line;
    // Closure metadata for target-agnostic KIR (IR v2.3)
    bool uses_closures;
    char** closure_vars;
    int closure_var_count;
  } IRHandlerSource;
  IRHandlerSource* ir_create_handler_source(const char* language, const char* code, const char* file, int line);
  void ir_destroy_handler_source(IRHandlerSource* source);
  void ir_event_set_handler_source(IREvent* event, IRHandlerSource* source);
  int ir_handler_source_set_closures(IRHandlerSource* source, const char** vars, int count);

  // ============================================================================
  // Layout Computation (ir_layout.h)
  // ============================================================================
  void ir_layout_compute(IRComponent* root, float parent_width, float parent_height);
  void ir_layout_invalidate_cache(IRComponent* component);

  // ============================================================================
  // Tab Groups (ir_builder.h)
  // ============================================================================
  TabGroupState* ir_tabgroup_create_state(IRComponent* group, IRComponent* tabBar, IRComponent* tabContent, int selectedIndex, bool reorderable);
  void ir_tabgroup_register_bar(TabGroupState* state, IRComponent* tab_bar);
  void ir_tabgroup_register_content(TabGroupState* state, IRComponent* tab_content);
  void ir_tabgroup_register_tab(TabGroupState* state, IRComponent* tab);
  void ir_tabgroup_register_panel(TabGroupState* state, IRComponent* panel);
  void ir_tabgroup_finalize(TabGroupState* state);
  void ir_tabgroup_select(TabGroupState* state, int index);
  int ir_tabgroup_get_selected(TabGroupState* state);
  uint32_t ir_tabgroup_get_tab_count(TabGroupState* state);

  // ============================================================================
  // Tab Visual State (Colors for active/inactive tabs)
  // ============================================================================
  typedef struct TabVisualState {
    uint32_t background_color;        // RGBA packed: 0xRRGGBBAA
    uint32_t active_background_color;
    uint32_t text_color;
    uint32_t active_text_color;
  } TabVisualState;

  void ir_tabgroup_set_tab_visual(TabGroupState* state, int index, TabVisualState visual);

  // ============================================================================
  // Debug Utilities (debug_backend.h)
  // ============================================================================
  void debug_print_tree(IRComponent* root);
  void debug_print_tree_to_file(IRComponent* root, const char* filename);

  // ============================================================================
  // Convenience Functions (ir_builder.h)
  // ============================================================================
  IRComponent* ir_container(void);
  IRComponent* ir_text(const char* content);
  IRComponent* ir_button(const char* content);
  IRComponent* ir_input(void);
  IRComponent* ir_checkbox(const char* label);
  IRComponent* ir_row(void);
  IRComponent* ir_column(void);
  IRComponent* ir_center(void);

  // ============================================================================
  // Component Type and Tree Helpers
  // ============================================================================
  IRComponentType ir_get_component_type(IRComponent* component);
  uint32_t ir_get_component_id(IRComponent* component);
  uint32_t ir_get_child_count(IRComponent* component);
  IRComponent* ir_get_child_at(IRComponent* component, uint32_t index);

  // ============================================================================
  // Color Helpers (ir_builder.h)
  // ============================================================================
  typedef struct IRColor {
    IRColorType type;
    union {
      struct { uint8_t r, g, b, a; };
      IRStyleVarId var_id;
      IRGradient* gradient;
    } data;
  } IRColor;

  IRColor ir_color_rgb(uint8_t r, uint8_t g, uint8_t b);
  IRColor ir_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
  IRColor ir_color_transparent(void);
  IRColor ir_color_named(const char* name);

  // ============================================================================
  // IR Serialization (ir_serialization.h)
  // ============================================================================
  // Forward declare logic block type
  typedef struct IRLogicBlock IRLogicBlock;
  typedef struct IRReactiveManifest IRReactiveManifest;
  typedef struct IRSourceStructures IRSourceStructures;

  // JSON serialization
  char* ir_serialize_json(IRComponent* root);
  char* ir_serialize_json_complete(IRComponent* root, IRReactiveManifest* manifest,
                                    IRLogicBlock* logic_block,
                                    IRSourceMetadata* source_metadata,
                                    IRSourceStructures* source_structures);
  IRComponent* ir_deserialize_json(const char* json_string);
  bool ir_write_json_file(IRComponent* root, const char* filename);
  IRComponent* ir_read_json_file(const char* filename);

  // ForEach expansion (call after loading KIR file to expand ForEach components)
  void ir_expand_foreach(IRComponent* root);

  // ForEach new modular API (ir_foreach.h, ir_foreach_expand.h)
  typedef struct IRForEachDef IRForEachDef;
  typedef struct IRForEachBinding IRForEachBinding;

  // ForEach definition builder
  IRForEachDef* ir_foreach_def_create(const char* item_name, const char* index_name);
  void ir_foreach_def_destroy(IRForEachDef* def);
  void ir_foreach_set_source_literal(IRForEachDef* def, const char* json_array);
  void ir_foreach_set_source_variable(IRForEachDef* def, const char* var_name);
  void ir_foreach_set_source_expression(IRForEachDef* def, const char* expr);
  void ir_foreach_set_template(IRForEachDef* def, IRComponent* template_component);
  void ir_foreach_add_binding(IRForEachDef* def, const char* target_property,
                              const char* source_expression, bool is_computed);
  void ir_foreach_clear_bindings(IRForEachDef* def);

  // ForEach tree expansion
  void ir_foreach_expand_tree(IRComponent* root);

  // Component deep copy utility
  IRComponent* ir_component_deep_copy(IRComponent* src);

  // Context access
  IRContext* ir_get_global_context(void);

  // Memory management (stdlib.h)
  void free(void* ptr);

  // Binary serialization
  bool ir_write_binary_file(IRComponent* root, const char* filename);
  IRComponent* ir_read_binary_file(const char* filename);

  // ============================================================================
  // Desktop Renderer (ir_desktop_renderer.h)
  // ============================================================================
  typedef struct DesktopIRRenderer DesktopIRRenderer;

  typedef struct DesktopRendererConfig {
    int backend_type;  // 0 = SDL3
    int window_width;
    int window_height;
    const char* window_title;
    bool resizable;
    bool fullscreen;
    bool vsync_enabled;
    int target_fps;
  } DesktopRendererConfig;

  // Renderer functions
  DesktopIRRenderer* desktop_ir_renderer_create(const DesktopRendererConfig* config);
  void desktop_ir_renderer_destroy(DesktopIRRenderer* renderer);
  bool desktop_ir_renderer_initialize(DesktopIRRenderer* renderer);
  bool desktop_ir_renderer_render_frame(DesktopIRRenderer* renderer, IRComponent* root);
  bool desktop_ir_renderer_run_main_loop(DesktopIRRenderer* renderer, IRComponent* root);
  void desktop_ir_renderer_stop(DesktopIRRenderer* renderer);

  // Event callback
  // text_data is non-NULL for TEXT_CHANGE events, NULL for other events
  typedef void (*LuaEventCallback)(uint32_t component_id, int event_type, const char* text_data);
  void desktop_ir_renderer_set_lua_event_callback(DesktopIRRenderer* renderer, LuaEventCallback callback);

  void desktop_ir_renderer_update_root(DesktopIRRenderer* renderer, IRComponent* new_root);

  // Plugin event type registration
  bool ir_plugin_register_event_type(const char* plugin_name, const char* event_type_name,
                                      uint32_t event_type_id, const char* description);
  uint32_t ir_plugin_get_event_type_id(const char* event_type_name);
  const char* ir_plugin_get_event_type_name(uint32_t event_type_id);
  bool ir_plugin_has_event_type(const char* event_type_name);
]]

-- Desktop-specific FFI declarations (for symbols in libkryon_desktop.so)
if Desktop then
  ffi.cdef[[
    // Canvas callbacks (Desktop-specific, not in IR core)
    typedef void (*LuaCanvasDrawCallback)(uint32_t component_id);
    typedef void (*LuaCanvasUpdateCallback)(uint32_t component_id, double delta_time);
    void desktop_ir_renderer_set_lua_canvas_draw_callback(DesktopIRRenderer* renderer, LuaCanvasDrawCallback callback);
    void desktop_ir_renderer_set_lua_canvas_update_callback(DesktopIRRenderer* renderer, LuaCanvasUpdateCallback callback);
  ]]
end

-- Export the C namespace and type constants
return {
  C = C,
  Desktop = Desktop,
  lib_name = lib_name,

  -- Component types
  ComponentType = {
    CONTAINER = C.IR_COMPONENT_CONTAINER,
    TEXT = C.IR_COMPONENT_TEXT,
    BUTTON = C.IR_COMPONENT_BUTTON,
    INPUT = C.IR_COMPONENT_INPUT,
    CHECKBOX = C.IR_COMPONENT_CHECKBOX,
    DROPDOWN = C.IR_COMPONENT_DROPDOWN,
    ROW = C.IR_COMPONENT_ROW,
    COLUMN = C.IR_COMPONENT_COLUMN,
    CENTER = C.IR_COMPONENT_CENTER,
    IMAGE = C.IR_COMPONENT_IMAGE,
    CANVAS = C.IR_COMPONENT_CANVAS,
    MARKDOWN = C.IR_COMPONENT_MARKDOWN,
    CUSTOM = C.IR_COMPONENT_CUSTOM,
    TAB_GROUP = C.IR_COMPONENT_TAB_GROUP,
    TAB_BAR = C.IR_COMPONENT_TAB_BAR,
    TAB = C.IR_COMPONENT_TAB,
    TAB_CONTENT = C.IR_COMPONENT_TAB_CONTENT,
    TAB_PANEL = C.IR_COMPONENT_TAB_PANEL,
    MODAL = C.IR_COMPONENT_MODAL,
    FOR_EACH = C.IR_COMPONENT_FOR_EACH,
  },

  -- Dimension types
  DimensionType = {
    PX = C.IR_DIMENSION_PX,
    PERCENT = C.IR_DIMENSION_PERCENT,
    AUTO = C.IR_DIMENSION_AUTO,
    FLEX = C.IR_DIMENSION_FLEX,
    VW = C.IR_DIMENSION_VW,
    VH = C.IR_DIMENSION_VH,
    VMIN = C.IR_DIMENSION_VMIN,
    VMAX = C.IR_DIMENSION_VMAX,
    REM = C.IR_DIMENSION_REM,
    EM = C.IR_DIMENSION_EM,
  },

  -- Alignment types
  Alignment = {
    START = C.IR_ALIGNMENT_START,
    CENTER = C.IR_ALIGNMENT_CENTER,
    END = C.IR_ALIGNMENT_END,
    STRETCH = C.IR_ALIGNMENT_STRETCH,
    SPACE_BETWEEN = C.IR_ALIGNMENT_SPACE_BETWEEN,
    SPACE_AROUND = C.IR_ALIGNMENT_SPACE_AROUND,
    SPACE_EVENLY = C.IR_ALIGNMENT_SPACE_EVENLY,
  },

  -- Text alignment
  TextAlign = {
    LEFT = C.IR_TEXT_ALIGN_LEFT,
    RIGHT = C.IR_TEXT_ALIGN_RIGHT,
    CENTER = C.IR_TEXT_ALIGN_CENTER,
    JUSTIFY = C.IR_TEXT_ALIGN_JUSTIFY,
  },

  -- Event types
  EventType = {
    CLICK = C.IR_EVENT_CLICK,
    HOVER = C.IR_EVENT_HOVER,
    FOCUS = C.IR_EVENT_FOCUS,
    BLUR = C.IR_EVENT_BLUR,
    KEY = C.IR_EVENT_KEY,
    SCROLL = C.IR_EVENT_SCROLL,
    TIMER = C.IR_EVENT_TIMER,
    CUSTOM = C.IR_EVENT_CUSTOM,
    PLUGIN_START = C.IR_EVENT_PLUGIN_START,
    PLUGIN_END = C.IR_EVENT_PLUGIN_END,
  },
}
