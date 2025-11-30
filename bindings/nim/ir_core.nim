## Universal IR Core Bindings for Nim
## Direct bindings to the new IR system - no legacy compatibility layers

# Type definitions from ir_core.h
type
  IRComponentType* {.size: sizeof(cint).} = enum
    IR_COMPONENT_CONTAINER = 0
    IR_COMPONENT_TEXT
    IR_COMPONENT_BUTTON
    IR_COMPONENT_INPUT
    IR_COMPONENT_CHECKBOX
    IR_COMPONENT_DROPDOWN
    IR_COMPONENT_ROW
    IR_COMPONENT_COLUMN
    IR_COMPONENT_CENTER
    IR_COMPONENT_IMAGE
    IR_COMPONENT_CANVAS
    IR_COMPONENT_MARKDOWN
    IR_COMPONENT_CUSTOM

  IRDimensionType* {.size: sizeof(cint).} = enum
    IR_DIMENSION_PX = 0      # Pixels
    IR_DIMENSION_PERCENT   # Percentage (0-100)
    IR_DIMENSION_AUTO      # Auto-calculated
    IR_DIMENSION_FLEX      # Flex units

  IRAlignment* {.size: sizeof(cint).} = enum
    IR_ALIGNMENT_START = 0
    IR_ALIGNMENT_CENTER
    IR_ALIGNMENT_END
    IR_ALIGNMENT_STRETCH
    IR_ALIGNMENT_SPACE_BETWEEN
    IR_ALIGNMENT_SPACE_AROUND
    IR_ALIGNMENT_SPACE_EVENLY

  IRTextAlign* {.size: sizeof(cint).} = enum
    IR_TEXT_ALIGN_LEFT = 0
    IR_TEXT_ALIGN_RIGHT
    IR_TEXT_ALIGN_CENTER
    IR_TEXT_ALIGN_JUSTIFY

  IREventType* {.size: sizeof(cint).} = enum
    IR_EVENT_CLICK = 0
    IR_EVENT_HOVER
    IR_EVENT_FOCUS
    IR_EVENT_BLUR
    IR_EVENT_KEY
    IR_EVENT_SCROLL
    IR_EVENT_TIMER
    IR_EVENT_CUSTOM

  LogicSourceType* {.size: sizeof(cint).} = enum
    LOGIC_NONE = 0
    LOGIC_JAVASCRIPT
    LOGIC_WEBASSEMBLY
    LOGIC_LUA
    LOGIC_PYTHON

# Core structures
type
  IRDimension* = object
    `type`*: IRDimensionType
    value*: cfloat

  IRColorType* {.size: sizeof(cint).} = enum
    IR_COLOR_SOLID = 0
    IR_COLOR_TRANSPARENT
    IR_COLOR_GRADIENT
    IR_COLOR_VAR_REF

  # Gradient types
  IRGradientType* {.size: sizeof(cint).} = enum
    IR_GRADIENT_LINEAR = 0
    IR_GRADIENT_RADIAL
    IR_GRADIENT_CONIC

  IRGradientStop* {.importc: "IRGradientStop", header: "ir_core.h".} = object
    r*, g*, b*, a*: uint8
    position*: cfloat  # 0.0 to 1.0

  IRGradient* {.importc: "struct IRGradient", header: "ir_core.h".} = object
    `type`*: IRGradientType
    stops*: array[8, IRGradientStop]
    stop_count*: uint8
    angle*: cfloat           # For linear gradients (degrees)
    center_x*: cfloat        # For radial/conic (0.0 to 1.0)
    center_y*: cfloat        # For radial/conic (0.0 to 1.0)

  # Union data for IRColor - matches C union layout exactly
  # C has: union { struct { uint8_t r, g, b, a; }; IRStyleVarId var_id; ptr[IRGradient] gradient; }
  # Use importc to match the C field names
  IRColorData* {.importc: "IRColorData", header: "ir_core.h", union.} = object
    r*, g*, b*, a*: uint8   # Anonymous struct fields (accessed directly)
    var_id*: uint16         # Style variable reference
    gradient*: ptr IRGradient  # Gradient pointer

  IRColor* {.importc: "IRColor", header: "ir_core.h".} = object
    `type`*: IRColorType
    data*: IRColorData

# IRColor convenience accessors for backwards compatibility
template r*(c: IRColor): uint8 = c.data.r
template g*(c: IRColor): uint8 = c.data.g
template b*(c: IRColor): uint8 = c.data.b
template a*(c: IRColor): uint8 = c.data.a
template var_id*(c: IRColor): uint16 = c.data.var_id

template `r=`*(c: var IRColor, v: uint8) = c.data.r = v
template `g=`*(c: var IRColor, v: uint8) = c.data.g = v
template `b=`*(c: var IRColor, v: uint8) = c.data.b = v
template `a=`*(c: var IRColor, v: uint8) = c.data.a = v
template `var_id=`*(c: var IRColor, v: uint16) = c.data.var_id = v

type
  IRSpacing* = object
    top*: cfloat
    right*: cfloat
    bottom*: cfloat
    left*: cfloat

  IRFont* = object
    family*: cstring
    size*: cfloat
    color*: IRColor
    bold*: bool
    italic*: bool

  IRBorder* = object
    width*: cfloat
    color*: IRColor
    radius*: uint8

  IRTypography* = object
    size*: cfloat
    color*: IRColor
    bold*: bool
    italic*: bool
    family*: cstring

  IRFlexbox* = object
    wrap*: bool
    gap*: uint32
    main_axis*: IRAlignment
    cross_axis*: IRAlignment
    justify_content*: IRAlignment
    grow*: uint8      # Flex grow factor (0-255)
    shrink*: uint8    # Flex shrink factor (0-255)
    direction*: uint8 # Layout direction: 0=column, 1=row

  IRPositionMode* {.size: sizeof(cint).} = enum
    IR_POSITION_RELATIVE = 0
    IR_POSITION_ABSOLUTE

  # Text Overflow Behavior
  IRTextOverflowType* {.size: sizeof(cint).} = enum
    IR_TEXT_OVERFLOW_VISIBLE = 0  # Show all text (may overflow bounds)
    IR_TEXT_OVERFLOW_CLIP         # Hard clip at bounds
    IR_TEXT_OVERFLOW_ELLIPSIS     # Show "..." when truncated
    IR_TEXT_OVERFLOW_FADE         # Gradient fade at edge

  # Text Fade Direction
  IRTextFadeType* {.size: sizeof(cint).} = enum
    IR_TEXT_FADE_NONE = 0
    IR_TEXT_FADE_HORIZONTAL       # Fade left/right edges
    IR_TEXT_FADE_VERTICAL         # Fade top/bottom edges
    IR_TEXT_FADE_RADIAL           # Fade from center outward

  IRStyle* {.importc: "IRStyle", header: "ir_core.h", incompleteStruct.} = object
    width*: IRDimension
    height*: IRDimension
    min_width*: IRDimension
    min_height*: IRDimension
    max_width*: IRDimension
    max_height*: IRDimension
    background*: IRColor
    border*: IRBorder
    margin*: IRSpacing
    padding*: IRSpacing
    font*: IRTypography
    visible*: bool
    z_index*: uint32
    position_mode*: IRPositionMode
    absolute_x*: cfloat
    absolute_y*: cfloat

  IRLayout* {.importc: "IRLayout", header: "ir_core.h", incompleteStruct.} = object
    min_width*: IRDimension
    min_height*: IRDimension
    max_width*: IRDimension
    max_height*: IRDimension
    flex*: IRFlexbox
    margin*: IRSpacing
    padding*: IRSpacing
    align_items*: IRAlignment
    align_content*: IRAlignment

  IREvent* {.importc: "IREvent", header: "ir_core.h", incompleteStruct.} = object

  LogicSource* = object
    `type`*: LogicSourceType
    source*: cstring
    language*: cstring

  # Layout cache for performance optimization
  IRLayoutCache* {.importc: "IRLayoutCache", header: "ir_core.h".} = object
    dirty*: bool
    cached_intrinsic_width*: cfloat
    cached_intrinsic_height*: cfloat
    cache_generation*: uint32

  IRComponent* {.importc: "IRComponent", header: "ir_core.h", incompleteStruct.} = object
    id*: uint32
    `type`*: IRComponentType
    tag*: cstring
    style*: ptr IRStyle
    events*: ptr IREvent
    logic*: ptr LogicSource
    children*: ptr ptr IRComponent
    child_count*: uint32
    layout*: ptr IRLayout
    parent*: ptr IRComponent
    text_content*: cstring
    custom_data*: cstring
    layout_cache*: IRLayoutCache

  IRContext* {.importc: "IRContext", header: "ir_core.h", incompleteStruct.} = object
  TabGroupState* {.importc: "TabGroupState", header: "ir_builder.h", incompleteStruct.} = object

  # Tab visual state for active/inactive tab colors (matches C struct)
  CTabVisualState* {.importc: "TabVisualState", header: "ir_builder.h".} = object
    background_color*: uint32
    active_background_color*: uint32
    text_color*: uint32
    active_text_color*: uint32

# Context Management
proc ir_create_context*(): ptr IRContext {.importc, cdecl, header: "ir_builder.h".}
proc ir_destroy_context*(context: ptr IRContext) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_context*(context: ptr IRContext) {.importc, cdecl, header: "ir_builder.h".}
proc ir_get_root*(): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_root*(root: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}

# Component Creation
proc ir_create_component*(component_type: IRComponentType): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}
proc ir_create_component_with_id*(component_type: IRComponentType; id: uint32): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}
proc ir_destroy_component*(component: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}

# Tree Structure Management
proc ir_add_child*(parent: ptr IRComponent; child: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_remove_child*(parent: ptr IRComponent; child: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_insert_child*(parent: ptr IRComponent; child: ptr IRComponent; index: uint32) {.importc, cdecl, header: "ir_builder.h".}
proc ir_get_child*(component: ptr IRComponent; index: uint32): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}
proc ir_find_component_by_id*(root: ptr IRComponent; id: uint32): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}

# Style Management
proc ir_create_style*(): ptr IRStyle {.importc, cdecl, header: "ir_builder.h".}
proc ir_destroy_style*(style: ptr IRStyle) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_style*(component: ptr IRComponent; style: ptr IRStyle) {.importc, cdecl, header: "ir_builder.h".}
proc ir_get_style*(component: ptr IRComponent): ptr IRStyle {.importc, cdecl, header: "ir_builder.h".}

# Style Property Helpers
proc ir_set_width*(style: ptr IRStyle; dimension_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_height*(style: ptr IRStyle; dimension_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_min_width*(layout: ptr IRLayout; dimension_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_min_height*(layout: ptr IRLayout; dimension_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_max_width*(layout: ptr IRLayout; dimension_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_max_height*(layout: ptr IRLayout; dimension_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_aspect_ratio*(layout: ptr IRLayout; ratio: cfloat) {.importc, cdecl, header: "ir_builder.h".}

proc ir_set_background_color*(style: ptr IRStyle; r: uint8; g: uint8; b: uint8; a: uint8) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_background_gradient*(style: ptr IRStyle; gradient: ptr IRGradient) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_border*(style: ptr IRStyle; width: cfloat; r: uint8; g: uint8; b: uint8; a: uint8; radius: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_margin*(style: ptr IRStyle; top: cfloat; right: cfloat; bottom: cfloat; left: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_padding*(style: ptr IRStyle; top: cfloat; right: cfloat; bottom: cfloat; left: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_font*(style: ptr IRStyle; size: cfloat; family: cstring; r: uint8; g: uint8; b: uint8; a: uint8; bold: bool; italic: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_visible*(style: ptr IRStyle; visible: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_z_index*(style: ptr IRStyle; z_index: uint32) {.importc, cdecl, header: "ir_builder.h".}

# Text Effect Property Helpers
proc ir_set_text_overflow*(style: ptr IRStyle; overflow: IRTextOverflowType) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_text_fade*(style: ptr IRStyle; fade_type: IRTextFadeType; fade_length: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_text_shadow*(style: ptr IRStyle; offset_x, offset_y, blur_radius: cfloat; r, g, b, a: uint8) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_opacity*(style: ptr IRStyle; opacity: cfloat) {.importc, cdecl, header: "ir_builder.h".}

# Layout Management
proc ir_create_layout*(): ptr IRLayout {.importc, cdecl, header: "ir_builder.h".}
proc ir_destroy_layout*(layout: ptr IRLayout) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_layout*(component: ptr IRComponent; layout: ptr IRLayout) {.importc, cdecl, header: "ir_builder.h".}
proc ir_get_layout*(component: ptr IRComponent): ptr IRLayout {.importc, cdecl, header: "ir_builder.h".}

# Layout Property Helpers
proc ir_set_flexbox*(layout: ptr IRLayout; wrap: bool; gap: uint32; main_axis: IRAlignment; cross_axis: IRAlignment) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_flex_properties*(layout: ptr IRLayout; grow: uint8; shrink: uint8; direction: uint8) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_justify_content*(layout: ptr IRLayout; justify: IRAlignment) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_align_items*(layout: ptr IRLayout; align: IRAlignment) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_align_content*(layout: ptr IRLayout; align: IRAlignment) {.importc, cdecl, header: "ir_builder.h".}

# Color Helpers
proc ir_color_rgb*(r: uint8; g: uint8; b: uint8): IRColor {.importc, cdecl, header: "ir_builder.h".}
proc ir_color_rgba*(r: uint8; g: uint8; b: uint8; a: uint8): IRColor {.importc, cdecl, header: "ir_builder.h".}
proc ir_color_transparent*(): IRColor {.importc, cdecl, header: "ir_builder.h".}
proc ir_color_named*(name: cstring): IRColor {.importc, cdecl, header: "ir_builder.h".}

# Extended Typography (Phase 3)
proc ir_set_font_weight*(style: ptr IRStyle; weight: uint16) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_letter_spacing*(style: ptr IRStyle; spacing: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_word_spacing*(style: ptr IRStyle; spacing: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_line_height*(style: ptr IRStyle; line_height: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_text_align*(style: ptr IRStyle; align: IRTextAlign) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_text_decoration*(style: ptr IRStyle; decoration: uint8) {.importc, cdecl, header: "ir_builder.h".}

# Text decoration constants
const
  IR_TEXT_DECORATION_NONE* = 0x00'u8
  IR_TEXT_DECORATION_UNDERLINE* = 0x01'u8
  IR_TEXT_DECORATION_OVERLINE* = 0x02'u8
  IR_TEXT_DECORATION_LINE_THROUGH* = 0x04'u8

# Box Shadow (Phase 2)
proc ir_set_box_shadow*(style: ptr IRStyle; offset_x: cfloat; offset_y: cfloat; blur_radius: cfloat; spread_radius: cfloat; r: uint8; g: uint8; b: uint8; a: uint8; inset: bool) {.importc, cdecl, header: "ir_builder.h".}

# CSS Filters (Phase 3)
type
  IRFilterType* {.size: sizeof(cint).} = enum
    IR_FILTER_BLUR = 0              # Gaussian blur (value in px)
    IR_FILTER_BRIGHTNESS            # 0 = black, 1 = normal, >1 = brighter
    IR_FILTER_CONTRAST              # 0 = gray, 1 = normal, >1 = more contrast
    IR_FILTER_GRAYSCALE             # 0 = color, 1 = fully grayscale
    IR_FILTER_HUE_ROTATE            # Rotate hue (value in degrees)
    IR_FILTER_INVERT                # 0 = normal, 1 = inverted
    IR_FILTER_OPACITY               # 0 = transparent, 1 = opaque
    IR_FILTER_SATURATE              # 0 = desaturated, 1 = normal, >1 = more saturated
    IR_FILTER_SEPIA                 # 0 = normal, 1 = fully sepia

proc ir_add_filter*(style: ptr IRStyle; filter_type: IRFilterType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_clear_filters*(style: ptr IRStyle) {.importc, cdecl, header: "ir_builder.h".}

# Grid Layout (Phase 4)
type
  IRLayoutMode* {.size: sizeof(cint).} = enum
    IR_LAYOUT_MODE_FLEX = 0         # Flexbox layout (default)
    IR_LAYOUT_MODE_GRID             # CSS Grid layout
    IR_LAYOUT_MODE_BLOCK            # Block layout (stacked)

  IRGridTrackType* {.size: sizeof(cint).} = enum
    IR_GRID_TRACK_AUTO = 0          # auto
    IR_GRID_TRACK_FR                # fractional unit (fr)
    IR_GRID_TRACK_PX                # pixels
    IR_GRID_TRACK_PERCENT           # percentage
    IR_GRID_TRACK_MIN_CONTENT       # min-content
    IR_GRID_TRACK_MAX_CONTENT       # max-content

  IRGridTrack* {.importc: "IRGridTrack", header: "ir_core.h".} = object
    track_type* {.importc: "type".}: IRGridTrackType
    value*: cfloat

# Grid track helpers
proc ir_grid_track_px*(value: cfloat): IRGridTrack {.importc, cdecl, header: "ir_builder.h".}
proc ir_grid_track_percent*(value: cfloat): IRGridTrack {.importc, cdecl, header: "ir_builder.h".}
proc ir_grid_track_fr*(value: cfloat): IRGridTrack {.importc, cdecl, header: "ir_builder.h".}
proc ir_grid_track_auto*(): IRGridTrack {.importc, cdecl, header: "ir_builder.h".}
proc ir_grid_track_min_content*(): IRGridTrack {.importc, cdecl, header: "ir_builder.h".}
proc ir_grid_track_max_content*(): IRGridTrack {.importc, cdecl, header: "ir_builder.h".}

# Grid layout functions
# Note: ir_set_layout_mode not yet implemented in C core - mode is inferred when grid properties are set
proc ir_set_grid_template_rows*(layout: ptr IRLayout; tracks: ptr IRGridTrack; count: uint8) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_grid_template_columns*(layout: ptr IRLayout; tracks: ptr IRGridTrack; count: uint8) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_grid_gap*(layout: ptr IRLayout; row_gap: cfloat; column_gap: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_grid_auto_flow*(layout: ptr IRLayout; row_direction: bool; dense: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_grid_alignment*(layout: ptr IRLayout; justify_items: IRAlignment; align_items: IRAlignment; justify_content: IRAlignment; align_content: IRAlignment) {.importc, cdecl, header: "ir_builder.h".}

# Grid item placement
proc ir_set_grid_item_placement*(style: ptr IRStyle; row_start: int16; row_end: int16; column_start: int16; column_end: int16) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_grid_item_alignment*(style: ptr IRStyle; justify_self: IRAlignment; align_self: IRAlignment) {.importc, cdecl, header: "ir_builder.h".}

# Gradient Helpers
proc ir_gradient_create*(gradient_type: IRGradientType): ptr IRGradient {.importc, cdecl, header: "ir_builder.h".}
proc ir_gradient_add_stop*(gradient: ptr IRGradient; position: cfloat; r, g, b, a: uint8) {.importc, cdecl, header: "ir_builder.h".}
proc ir_gradient_set_angle*(gradient: ptr IRGradient; angle: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_gradient_set_center*(gradient: ptr IRGradient; x, y: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_color_from_gradient*(gradient: ptr IRGradient): IRColor {.importc, cdecl, header: "ir_builder.h".}

# Tab Group Management
proc ir_tabgroup_create_state*(group, tabBar, tabContent: ptr IRComponent; selectedIndex: cint; reorderable: bool): ptr TabGroupState {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_register_bar*(state: ptr TabGroupState; tabBar: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_register_content*(state: ptr TabGroupState; tabContent: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_register_tab*(state: ptr TabGroupState; tab: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_register_panel*(state: ptr TabGroupState; panel: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_finalize*(state: ptr TabGroupState) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_select*(state: ptr TabGroupState; index: cint) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_reorder*(state: ptr TabGroupState; from_index: cint; to_index: cint) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_set_reorderable*(state: ptr TabGroupState; reorderable: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_set_tab_visual*(state: ptr TabGroupState; index: cint; visual: CTabVisualState) {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_apply_visuals*(state: ptr TabGroupState) {.importc, cdecl, header: "ir_builder.h".}

# Tab Group Query Functions
proc ir_tabgroup_get_tab_count*(state: ptr TabGroupState): uint32 {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_get_panel_count*(state: ptr TabGroupState): uint32 {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_get_selected*(state: ptr TabGroupState): cint {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_get_tab*(state: ptr TabGroupState; index: uint32): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}
proc ir_tabgroup_get_panel*(state: ptr TabGroupState; index: uint32): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}

# Tab User Callback - called BEFORE tab selection
type TabClickCallback* = proc(tab_index: uint32; user_data: pointer) {.cdecl.}

proc ir_tabgroup_set_tab_callback*(state: ptr TabGroupState; index: uint32;
                                    callback: TabClickCallback;
                                    user_data: pointer) {.importc, cdecl, header: "ir_builder.h".}

# Tab Click Handling - combines user callback + selection
proc ir_tabgroup_handle_tab_click*(state: ptr TabGroupState; tab_index: uint32) {.importc, cdecl, header: "ir_builder.h".}

# Tab Group Cleanup
proc ir_tabgroup_destroy_state*(state: ptr TabGroupState) {.importc, cdecl, header: "ir_builder.h".}

# Layout setters not in C yet - add stubs
proc ir_set_gap*(layout: ptr IRLayout; gap: uint32) =
  # Gap is part of flexbox in IR
  discard

# Serialization
proc ir_serialize_binary*(root: ptr IRComponent; filename: cstring): bool {.importc, cdecl, header: "ir_serialization.h".}
proc ir_deserialize_binary*(filename: cstring): ptr IRComponent {.importc, cdecl, header: "ir_serialization.h".}

# Content Management
proc ir_set_text_content*(component: ptr IRComponent; text: cstring) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_custom_data*(component: ptr IRComponent; data: cstring) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_tag*(component: ptr IRComponent; tag: cstring) {.importc, cdecl, header: "ir_builder.h".}

# Event Management
proc ir_create_event*(event_type: IREventType; logic_id: cstring; handler_data: cstring): ptr IREvent {.importc, cdecl, header: "ir_builder.h".}
proc ir_destroy_event*(event: ptr IREvent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_add_event*(component: ptr IRComponent; event: ptr IREvent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_remove_event*(component: ptr IRComponent; event: ptr IREvent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_find_event*(component: ptr IRComponent; event_type: IREventType): ptr IREvent {.importc, cdecl, header: "ir_builder.h".}

# Utility functions
proc ir_component_type_to_string*(component_type: IRComponentType): cstring {.importc, cdecl, header: "ir_core.h".}
proc ir_event_type_to_string*(event_type: IREventType): cstring {.importc, cdecl, header: "ir_core.h".}
proc ir_logic_type_to_string*(logic_type: LogicSourceType): cstring {.importc, cdecl, header: "ir_core.h".}

# Convenience functions for common operations
proc ir_container*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CONTAINER)

proc ir_text*(content: cstring): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_TEXT)
  result.text_content = content

proc ir_button*(content: cstring): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_BUTTON)
  result.text_content = content

proc ir_input*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_INPUT)

proc ir_checkbox*(label: cstring): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}

proc ir_row*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_ROW)

proc ir_column*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_COLUMN)

proc ir_center*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CENTER)

# Dimension helpers
proc ir_px*(value: cfloat): IRDimension =
  result.`type` = IR_DIMENSION_PX
  result.value = value

proc ir_percent*(value: cfloat): IRDimension =
  result.`type` = IR_DIMENSION_PERCENT
  result.value = value

proc ir_auto*(): IRDimension =
  result.`type` = IR_DIMENSION_AUTO
  result.value = 0.0

proc ir_flex*(value: cfloat): IRDimension =
  result.`type` = IR_DIMENSION_FLEX
  result.value = value

# Color helpers
proc ir_rgb*(r: uint8; g: uint8; b: uint8): IRColor =
  result.r = r
  result.g = g
  result.b = b
  result.a = 255

proc ir_rgba*(r: uint8; g: uint8; b: uint8; a: uint8): IRColor =
  result.r = r
  result.g = g
  result.b = b
  result.a = a

proc ir_hex*(hex: uint32): IRColor =
  result.r = uint8((hex shr 16) and 0xFF)
  result.g = uint8((hex shr 8) and 0xFF)
  result.b = uint8(hex and 0xFF)
  result.a = 255

# Spacing helpers
proc ir_spacing_all*(value: cfloat): IRSpacing =
  result.top = value
  result.right = value
  result.bottom = value
  result.left = value

proc ir_spacing_xy*(x: cfloat; y: cfloat): IRSpacing =
  result.top = y
  result.right = x
  result.bottom = y
  result.left = x

proc ir_spacing_custom*(top: cfloat; right: cfloat; bottom: cfloat; left: cfloat): IRSpacing =
  result.top = top
  result.right = right
  result.bottom = bottom
  result.left = left

# Validation
proc ir_validate_component*(component: ptr IRComponent): bool {.importc, cdecl, header: "ir_core.h".}
proc ir_print_component_info*(component: ptr IRComponent; depth: cint) {.importc, cdecl, header: "ir_core.h".}
proc ir_optimize_component*(component: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}

# Debug Renderer
proc debug_print_tree*(root: ptr IRComponent) {.importc, cdecl, header: "debug_backend.h".}
proc debug_print_tree_to_file*(root: ptr IRComponent; filename: cstring) {.importc, cdecl, header: "debug_backend.h".}

# Hit Testing
proc ir_is_point_in_component*(component: ptr IRComponent; x: cfloat; y: cfloat): bool {.importc, cdecl, header: "ir_builder.h".}
proc ir_find_component_at_point*(root: ptr IRComponent; x: cfloat; y: cfloat): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_rendered_bounds*(component: ptr IRComponent; x: cfloat; y: cfloat; width: cfloat; height: cfloat) {.importc, cdecl, header: "ir_builder.h".}

# Dropdown functions
proc ir_dropdown*(placeholder: cstring; options: ptr cstring; option_count: uint32): ptr IRComponent {.importc, cdecl, header: "ir_builder.h".}
proc ir_get_dropdown_selected_index*(component: ptr IRComponent): int32 {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_dropdown_selected_index*(component: ptr IRComponent; index: int32) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_dropdown_options*(component: ptr IRComponent; options: ptr cstring; count: uint32) {.importc, cdecl, header: "ir_builder.h".}
proc ir_get_dropdown_open_state*(component: ptr IRComponent): bool {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_dropdown_open_state*(component: ptr IRComponent; is_open: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_toggle_dropdown_open_state*(component: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_get_dropdown_hovered_index*(component: ptr IRComponent): int32 {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_dropdown_hovered_index*(component: ptr IRComponent; index: int32) {.importc, cdecl, header: "ir_builder.h".}

# ============================================================================
# Animation System
# ============================================================================

type
  IREasingType* {.size: sizeof(cint).} = enum
    IR_EASING_LINEAR = 0
    IR_EASING_EASE_IN
    IR_EASING_EASE_OUT
    IR_EASING_EASE_IN_OUT
    IR_EASING_EASE_IN_QUAD
    IR_EASING_EASE_OUT_QUAD
    IR_EASING_EASE_IN_OUT_QUAD
    IR_EASING_EASE_IN_CUBIC
    IR_EASING_EASE_OUT_CUBIC
    IR_EASING_EASE_IN_OUT_CUBIC
    IR_EASING_EASE_IN_BOUNCE
    IR_EASING_EASE_OUT_BOUNCE

  IRAnimationProperty* {.size: sizeof(cint).} = enum
    IR_ANIM_PROP_OPACITY = 0
    IR_ANIM_PROP_TRANSLATE_X
    IR_ANIM_PROP_TRANSLATE_Y
    IR_ANIM_PROP_SCALE_X
    IR_ANIM_PROP_SCALE_Y
    IR_ANIM_PROP_ROTATE
    IR_ANIM_PROP_WIDTH
    IR_ANIM_PROP_HEIGHT
    IR_ANIM_PROP_BACKGROUND_COLOR
    IR_ANIM_PROP_CUSTOM

  IRKeyframe* {.importc: "IRKeyframe", header: "ir_core.h", incompleteStruct.} = object
  IRAnimation* {.importc: "IRAnimation", header: "ir_core.h", incompleteStruct.} = object
  IRTransition* {.importc: "IRTransition", header: "ir_core.h", incompleteStruct.} = object

# Animation creation and management
proc ir_animation_create_keyframe*(name: cstring; duration: cfloat): ptr IRAnimation {.importc, cdecl, header: "ir_builder.h".}
proc ir_animation_add_keyframe*(anim: ptr IRAnimation; offset: cfloat): ptr IRKeyframe {.importc, cdecl, header: "ir_builder.h".}
proc ir_keyframe_set_property*(kf: ptr IRKeyframe; prop: IRAnimationProperty; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_keyframe_set_color_property*(kf: ptr IRKeyframe; prop: IRAnimationProperty; color: IRColor) {.importc, cdecl, header: "ir_builder.h".}

# Animation configuration
proc ir_animation_set_delay*(anim: ptr IRAnimation; delay: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_animation_set_iterations*(anim: ptr IRAnimation; iterations: int32) {.importc, cdecl, header: "ir_builder.h".}
proc ir_animation_set_alternate*(anim: ptr IRAnimation; alternate: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_animation_set_default_easing*(anim: ptr IRAnimation; easing: IREasingType) {.importc, cdecl, header: "ir_builder.h".}

# Attach animations to components
proc ir_component_add_animation*(component: ptr IRComponent; anim: ptr IRAnimation) {.importc, cdecl, header: "ir_builder.h".}

# Helper animation creators
proc ir_animation_fade_in_out*(duration: cfloat): ptr IRAnimation {.importc, cdecl, header: "ir_builder.h".}
proc ir_animation_pulse*(duration: cfloat): ptr IRAnimation {.importc, cdecl, header: "ir_builder.h".}
proc ir_animation_slide_in_left*(duration: cfloat): ptr IRAnimation {.importc, cdecl, header: "ir_builder.h".}

# Tree-wide animation update (call once per frame with current time)
proc ir_animation_tree_update*(root: ptr IRComponent; current_time: cfloat) {.importc, cdecl, header: "ir_builder.h".}

# Re-propagate animation flags after tree construction (call once after building tree)
proc ir_animation_propagate_flags*(root: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}

# General component subtree finalization (call after adding children, especially from static loops)
proc ir_component_finalize_subtree*(component: ptr IRComponent) {.importc, cdecl, header: "ir_builder.h".}

# Transitions (Phase 6)
proc ir_transition_create*(property: IRAnimationProperty; duration: cfloat): ptr IRTransition {.importc, cdecl, header: "ir_builder.h".}
proc ir_transition_destroy*(transition: ptr IRTransition) {.importc, cdecl, header: "ir_builder.h".}
proc ir_transition_set_easing*(transition: ptr IRTransition; easing: IREasingType) {.importc, cdecl, header: "ir_builder.h".}
proc ir_transition_set_delay*(transition: ptr IRTransition; delay: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_transition_set_trigger*(transition: ptr IRTransition; state_mask: uint32) {.importc, cdecl, header: "ir_builder.h".}
proc ir_component_add_transition*(component: ptr IRComponent; transition: ptr IRTransition) {.importc, cdecl, header: "ir_builder.h".}

# Container Queries (Phase 7)
type IRQueryType* {.size: sizeof(cint).} = enum
  IR_QUERY_MIN_WIDTH
  IR_QUERY_MAX_WIDTH
  IR_QUERY_MIN_HEIGHT
  IR_QUERY_MAX_HEIGHT

type IRContainerType* {.size: sizeof(cint).} = enum
  IR_CONTAINER_TYPE_NORMAL
  IR_CONTAINER_TYPE_SIZE
  IR_CONTAINER_TYPE_INLINE_SIZE

type IRQueryCondition* {.importc: "IRQueryCondition", header: "ir_core.h".} = object
  `type`*: IRQueryType
  value*: cfloat

# Constants
const
  IR_MAX_BREAKPOINTS* = 4
  IR_MAX_QUERY_CONDITIONS* = 2

# Container setup functions
proc ir_set_container_type*(style: ptr IRStyle; container_type: IRContainerType) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_container_name*(style: ptr IRStyle; name: cstring) {.importc, cdecl, header: "ir_builder.h".}

# Query condition builder functions
proc ir_query_min_width*(value: cfloat): IRQueryCondition {.importc, cdecl, header: "ir_builder.h".}
proc ir_query_max_width*(value: cfloat): IRQueryCondition {.importc, cdecl, header: "ir_builder.h".}
proc ir_query_min_height*(value: cfloat): IRQueryCondition {.importc, cdecl, header: "ir_builder.h".}
proc ir_query_max_height*(value: cfloat): IRQueryCondition {.importc, cdecl, header: "ir_builder.h".}

# Breakpoint management functions
proc ir_add_breakpoint*(style: ptr IRStyle; conditions: ptr IRQueryCondition; condition_count: uint8) {.importc, cdecl, header: "ir_builder.h".}
proc ir_breakpoint_set_width*(style: ptr IRStyle; breakpoint_index: uint8; dim_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_breakpoint_set_height*(style: ptr IRStyle; breakpoint_index: uint8; dim_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_breakpoint_set_visible*(style: ptr IRStyle; breakpoint_index: uint8; visible: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_breakpoint_set_opacity*(style: ptr IRStyle; breakpoint_index: uint8; opacity: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_breakpoint_set_layout_mode*(style: ptr IRStyle; breakpoint_index: uint8; mode: IRLayoutMode) {.importc, cdecl, header: "ir_builder.h".}
