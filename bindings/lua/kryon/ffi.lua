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

  -- Try various paths
  local search_paths = {
    "build/" .. lib_name,  -- Relative to project root
    "/home/wao/Projects/kryon/build/" .. lib_name,  -- Absolute path
    "/usr/local/lib/" .. lib_name,  -- System install
    "/usr/lib/" .. lib_name,  -- System install
    lib_name,  -- Just the name (system LD_LIBRARY_PATH)
  }

  -- Check which one exists by attempting to open
  for _, path in ipairs(search_paths) do
    local f = io.open(path, "r")
    if f then
      f:close()
      return path
    end
  end

  -- Default fallback
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
  print("⚠️ Failed to load desktop library '" .. desktop_lib_name .. "': " .. tostring(err))
  print("   Desktop rendering features will not be available.")
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
    IR_COMPONENT_MARKDOWN = 11,
    IR_COMPONENT_CUSTOM = 12,
    IR_COMPONENT_TAB_GROUP = 13,
    IR_COMPONENT_TAB_BAR = 14,
    IR_COMPONENT_TAB = 15,
    IR_COMPONENT_TAB_CONTENT = 16,
    IR_COMPONENT_TAB_PANEL = 17
  } IRComponentType;

  // ============================================================================
  // Dimension Types
  // ============================================================================
  typedef enum {
    IR_DIMENSION_PX = 0,
    IR_DIMENSION_PERCENT = 1,
    IR_DIMENSION_AUTO = 2,
    IR_DIMENSION_FLEX = 3,
    IR_DIMENSION_VW = 4,
    IR_DIMENSION_VH = 5,
    IR_DIMENSION_VMIN = 6,
    IR_DIMENSION_VMAX = 7,
    IR_DIMENSION_REM = 8,
    IR_DIMENSION_EM = 9
  } IRDimensionType;

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
    IR_EVENT_CLICK = 0,
    IR_EVENT_HOVER = 1,
    IR_EVENT_FOCUS = 2,
    IR_EVENT_BLUR = 3,
    IR_EVENT_KEY = 4,
    IR_EVENT_SCROLL = 5,
    IR_EVENT_TIMER = 6,
    IR_EVENT_CUSTOM = 7
  } IREventType;

  // ============================================================================
  // Opaque Types (forward declarations)
  // ============================================================================
  typedef struct IRComponent IRComponent;
  typedef struct IRStyle IRStyle;
  typedef struct IRLayout IRLayout;
  typedef struct IRContext IRContext;
  typedef struct IREvent IREvent;
  typedef struct IRGradient IRGradient;
  typedef struct TabGroupState TabGroupState;

  // ============================================================================
  // Context Management (ir_builder.h)
  // ============================================================================
  IRContext* ir_create_context(void);
  void ir_destroy_context(IRContext* context);
  void ir_set_context(IRContext* context);
  IRComponent* ir_get_root(void);
  void ir_set_root(IRComponent* root);

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
  void ir_set_width(IRStyle* style, IRDimensionType type, float value);
  void ir_set_height(IRStyle* style, IRDimensionType type, float value);
  void ir_set_visible(IRStyle* style, bool visible);
  void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
  void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius);
  void ir_set_margin(IRStyle* style, float top, float right, float bottom, float left);
  void ir_set_padding(IRStyle* style, float top, float right, float bottom, float left);
  void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic);
  void ir_set_z_index(IRStyle* style, uint32_t z_index);
  void ir_set_opacity(IRStyle* style, float opacity);

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

  // ============================================================================
  // Event Management (ir_builder.h)
  // ============================================================================
  IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data);
  void ir_destroy_event(IREvent* event);
  void ir_add_event(IRComponent* component, IREvent* event);
  void ir_remove_event(IRComponent* component, IREvent* event);
  IREvent* ir_find_event(IRComponent* component, IREventType type);

  // ============================================================================
  // Layout Computation (ir_layout.h)
  // ============================================================================
  void ir_layout_compute(IRComponent* root, float parent_width, float parent_height);
  void ir_layout_invalidate_cache(IRComponent* component);

  // ============================================================================
  // Tab Groups (ir_builder.h)
  // ============================================================================
  TabGroupState* ir_tabgroup_create_state(IRComponent* group, IRComponent* tabBar, IRComponent* tabContent, int selectedIndex, bool reorderable);
  void ir_tabgroup_register_tab(TabGroupState* state, IRComponent* tab);
  void ir_tabgroup_register_panel(TabGroupState* state, IRComponent* panel);
  void ir_tabgroup_finalize(TabGroupState* state);
  void ir_tabgroup_select(TabGroupState* state, int index);
  int ir_tabgroup_get_selected(TabGroupState* state);

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
  // JSON serialization
  char* ir_serialize_json(IRComponent* root);
  IRComponent* ir_deserialize_json(const char* json_string);
  bool ir_write_json_file(IRComponent* root, const char* filename);
  IRComponent* ir_read_json_file(const char* filename);

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
  typedef void (*LuaEventCallback)(uint32_t component_id, int event_type);
  void desktop_ir_renderer_set_lua_event_callback(DesktopIRRenderer* renderer, LuaEventCallback callback);
  void desktop_ir_renderer_update_root(DesktopIRRenderer* renderer, IRComponent* new_root);
]]

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
  },
}
