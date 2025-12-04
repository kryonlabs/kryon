# 0BSD
## Universal IR Core Bindings for Nim
## Direct bindings to the new IR system - no legacy compatibility layers

import os

# Link IR libraries (desktop backend linked separately when needed)
{.passL: "-Lbuild -lkryon_ir -lm".}

# Type definitions from ir_core.h
# IMPORTANT: Import enums from C to avoid hardcoded values getting out of sync
type
  IRComponentType* {.size: sizeof(cint), importc: "IRComponentType", header: "ir_core.h".} = enum
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

  IRDimensionType* {.size: sizeof(cint), importc: "IRDimensionType", header: "ir_core.h".} = enum
    IR_DIMENSION_PX = 0      # Pixels
    IR_DIMENSION_PERCENT   # Percentage (0-100)
    IR_DIMENSION_AUTO      # Auto-calculated
    IR_DIMENSION_FLEX      # Flex units

  IRAlignment* {.size: sizeof(cint), importc: "IRAlignment", header: "ir_core.h".} = enum
    IR_ALIGNMENT_START = 0
    IR_ALIGNMENT_CENTER
    IR_ALIGNMENT_END
    IR_ALIGNMENT_STRETCH
    IR_ALIGNMENT_SPACE_BETWEEN
    IR_ALIGNMENT_SPACE_AROUND
    IR_ALIGNMENT_SPACE_EVENLY

  IRTextAlign* {.size: sizeof(cint), importc: "IRTextAlign", header: "ir_core.h".} = enum
    IR_TEXT_ALIGN_LEFT = 0
    IR_TEXT_ALIGN_RIGHT
    IR_TEXT_ALIGN_CENTER
    IR_TEXT_ALIGN_JUSTIFY

  # BiDi (Bidirectional Text) Support - NEW!
  IRDirection* {.size: sizeof(cint), importc: "IRDirection", header: "ir_core.h".} = enum
    IR_DIRECTION_AUTO = 0       # Auto-detect from content (default)
    IR_DIRECTION_LTR = 1        # Left-to-right
    IR_DIRECTION_RTL = 2        # Right-to-left
    IR_DIRECTION_INHERIT = 3    # Inherit from parent

  IRUnicodeBidi* {.size: sizeof(cint), importc: "IRUnicodeBidi", header: "ir_core.h".} = enum
    IR_UNICODE_BIDI_NORMAL = 0     # Default behavior
    IR_UNICODE_BIDI_EMBED = 1      # Embed level
    IR_UNICODE_BIDI_ISOLATE = 2    # Isolate from surroundings
    IR_UNICODE_BIDI_PLAINTEXT = 3  # Treat as plain text

  IREventType* {.size: sizeof(cint), importc: "IREventType", header: "ir_core.h".} = enum
    IR_EVENT_CLICK = 0
    IR_EVENT_HOVER
    IR_EVENT_FOCUS
    IR_EVENT_BLUR
    IR_EVENT_KEY
    IR_EVENT_SCROLL
    IR_EVENT_TIMER
    IR_EVENT_CUSTOM

  LogicSourceType* {.size: sizeof(cint), importc: "LogicSourceType", header: "ir_core.h".} = enum
    LOGIC_NONE = 0
    LOGIC_JAVASCRIPT
    LOGIC_WEBASSEMBLY
    LOGIC_LUA
    LOGIC_PYTHON

# Core structures
type
  IRDimension* {.importc: "IRDimension", header: "ir_core.h".} = object
    `type`*: IRDimensionType
    value*: cfloat

  IRColorType* {.size: sizeof(cint), importc: "IRColorType", header: "ir_core.h".} = enum
    IR_COLOR_SOLID = 0
    IR_COLOR_TRANSPARENT
    IR_COLOR_GRADIENT
    IR_COLOR_VAR_REF

  # Gradient types
  IRGradientType* {.size: sizeof(cint), importc: "IRGradientType", header: "ir_core.h".} = enum
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
  IRSpacing* {.importc: "IRSpacing", header: "ir_core.h".} = object
    top*: cfloat
    right*: cfloat
    bottom*: cfloat
    left*: cfloat

  IRFont* {.importc: "IRFont", header: "ir_core.h".} = object
    family*: cstring
    size*: cfloat
    color*: IRColor
    bold*: bool
    italic*: bool

  IRBorder* {.importc: "IRBorder", header: "ir_core.h".} = object
    width*: cfloat
    color*: IRColor
    radius*: uint8

  IRTypography* {.importc: "IRTypography", header: "ir_core.h".} = object
    size*: cfloat
    color*: IRColor
    bold*: bool
    italic*: bool
    family*: cstring

  IRFlexbox* {.importc: "IRFlexbox", header: "ir_core.h".} = object
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

  # Text Direction for BiDi support
  IRTextDirection* {.size: sizeof(cint).} = enum
    IR_TEXT_DIR_LTR = 0   # Left-to-right (English, Spanish, etc.)
    IR_TEXT_DIR_RTL       # Right-to-left (Arabic, Hebrew, etc.)
    IR_TEXT_DIR_AUTO      # Auto-detect from first strong character

  # Text Layout Result (multi-line text)
  IRTextLayout* {.importc: "IRTextLayout", header: "ir_core.h".} = object
    line_count*: uint32              # Number of lines
    line_widths*: ptr cfloat         # Width of each line
    line_heights*: ptr cfloat        # Height of each line
    line_start_indices*: ptr uint32  # Character offset where each line starts
    line_end_indices*: ptr uint32    # Character offset where each line ends
    total_height*: cfloat            # Total height of all lines
    max_line_width*: cfloat          # Width of widest line
    needs_reshape*: bool             # True if text shaping needed
    direction*: IRTextDirection      # Text direction (LTR/RTL/AUTO)

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
    text_expression*: cstring  # For reactive text (e.g., "{{value}}")
    custom_data*: cstring
    component_ref*: cstring    # For component references (e.g., "Counter")
    component_props*: cstring  # JSON string of props passed to component instance
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
proc ir_set_text_max_width*(style: ptr IRStyle; dimension_type: IRDimensionType; value: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_opacity*(style: ptr IRStyle; opacity: cfloat) {.importc, cdecl, header: "ir_builder.h".}

# Layout Management
proc ir_create_layout*(): ptr IRLayout {.importc, cdecl, header: "ir_builder.h".}
proc ir_destroy_layout*(layout: ptr IRLayout) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_layout*(component: ptr IRComponent; layout: ptr IRLayout) {.importc, cdecl, header: "ir_builder.h".}
proc ir_get_layout*(component: ptr IRComponent): ptr IRLayout {.importc, cdecl, header: "ir_builder.h".}

# Text Layout API (multi-line text wrapping)
proc ir_text_layout_create*(max_lines: uint32): ptr IRTextLayout {.importc, cdecl, header: "ir_core.h".}
proc ir_text_layout_destroy*(layout: ptr IRTextLayout) {.importc, cdecl, header: "ir_core.h".}
proc ir_text_layout_compute*(component: ptr IRComponent; max_width: cfloat; font_size: cfloat) {.importc, cdecl, header: "ir_core.h".}
proc ir_text_layout_get_width*(layout: ptr IRTextLayout): cfloat {.importc, cdecl, header: "ir_core.h".}
proc ir_text_layout_get_height*(layout: ptr IRTextLayout): cfloat {.importc, cdecl, header: "ir_core.h".}

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

# Layout setters - direct field access
proc ir_set_gap*(layout: ptr IRLayout; gap: uint32) =
  ## Set the flex gap between children
  if layout != nil:
    layout.flex.gap = gap

# Serialization
proc ir_serialize_binary*(root: ptr IRComponent; filename: cstring): bool {.importc, cdecl, header: "ir_serialization.h".}
proc ir_deserialize_binary*(filename: cstring): ptr IRComponent {.importc, cdecl, header: "ir_serialization.h".}

# IR Serialization
{.passC: "-I" & currentSourcePath().parentDir() & "/../../ir".}


# Content Management
proc ir_set_text_content*(component: ptr IRComponent; text: cstring) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_custom_data*(component: ptr IRComponent; data: cstring) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_tag*(component: ptr IRComponent; tag: cstring) {.importc, cdecl, header: "ir_builder.h".}

# Text expression support for reactive bindings
proc ir_set_text_expression*(component: ptr IRComponent; expr: cstring) =
  ## Set the reactive text expression (e.g., "{{value}}")
  ## This is serialized alongside text_content for .kir files
  component.text_expression = expr

# C library strdup for copying strings to C-allocated memory
proc strdup(s: cstring): cstring {.importc, header: "<string.h>".}

# Component reference support for custom components
proc ir_set_component_ref*(component: ptr IRComponent; refName: cstring) =
  ## Set the component reference name (e.g., "Counter")
  ## Used when serializing component instances instead of expanded trees
  ## Note: Must use strdup to copy the string because Nim's cstring may be temporary
  if refName != nil and refName[0] != '\0':
    component.component_ref = strdup(refName)
  else:
    component.component_ref = nil

proc ir_set_component_props*(component: ptr IRComponent; propsJson: cstring) =
  ## Set the component instance props as JSON string
  ## Used when serializing component instances with their props
  ## Note: Must use strdup to copy the string because Nim's cstring may be temporary
  if propsJson != nil and propsJson[0] != '\0':
    component.component_props = strdup(propsJson)
  else:
    component.component_props = nil

# Event Management
proc ir_create_event*(event_type: IREventType; logic_id: cstring; handler_data: cstring): ptr IREvent {.importc, cdecl, header: "ir_builder.h".}
proc ir_destroy_event*(event: ptr IREvent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_add_event*(component: ptr IRComponent; event: ptr IREvent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_remove_event*(component: ptr IRComponent; event: ptr IREvent) {.importc, cdecl, header: "ir_builder.h".}
proc ir_find_event*(component: ptr IRComponent; event_type: IREventType): ptr IREvent {.importc, cdecl, header: "ir_builder.h".}

# Event Bytecode Support (IR v2.1)
proc ir_event_set_bytecode_function_id*(event: ptr IREvent; function_id: uint32) {.importc, cdecl, header: "ir_builder.h".}
proc ir_event_get_bytecode_function_id*(event: ptr IREvent): uint32 {.importc, cdecl, header: "ir_builder.h".}

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

# ============================================================================
# Text Shaping API (HarfBuzz integration)
# ============================================================================

type
  IRShapeDirection* {.size: sizeof(cint).} = enum
    IR_SHAPE_DIRECTION_LTR = 0    # Left-to-right
    IR_SHAPE_DIRECTION_RTL        # Right-to-left
    IR_SHAPE_DIRECTION_TTB        # Top-to-bottom
    IR_SHAPE_DIRECTION_BTT        # Bottom-to-top
    IR_SHAPE_DIRECTION_AUTO       # Auto-detect from script

  IRGlyphInfo* {.importc, header: "ir_text_shaping.h".} = object
    glyph_id*: uint32
    cluster*: uint32
    x_advance*: cfloat
    y_advance*: cfloat
    x_offset*: cfloat
    y_offset*: cfloat

  IRShapedText* {.importc, header: "ir_text_shaping.h".} = object
    glyphs*: ptr IRGlyphInfo
    glyph_count*: uint32
    total_width*: cfloat
    total_height*: cfloat
    hb_buffer*: pointer

  IRShapingFont* {.importc: "IRFont", header: "ir_text_shaping.h".} = object
    hb_font*: pointer
    hb_face*: pointer
    hb_blob*: pointer
    font_path*: cstring
    size*: cfloat
    scale*: cfloat

  IRShapeOptions* {.importc, header: "ir_text_shaping.h".} = object
    direction*: IRShapeDirection
    language*: cstring
    script*: cstring
    features*: cstringArray
    feature_count*: uint32

# Text shaping system initialization
proc ir_text_shaping_init*(): bool {.importc, cdecl, header: "ir_text_shaping.h".}
proc ir_text_shaping_shutdown*() {.importc, cdecl, header: "ir_text_shaping.h".}
proc ir_text_shaping_available*(): bool {.importc, cdecl, header: "ir_text_shaping.h".}

# Font management
proc ir_font_load*(font_path: cstring; size: cfloat): ptr IRShapingFont {.importc, cdecl, header: "ir_text_shaping.h".}
proc ir_font_destroy*(font: ptr IRShapingFont) {.importc, cdecl, header: "ir_text_shaping.h".}
proc ir_font_set_size*(font: ptr IRShapingFont; size: cfloat) {.importc, cdecl, header: "ir_text_shaping.h".}

# Text shaping
proc ir_shape_text*(
  font: ptr IRShapingFont;
  text: cstring;
  text_length: uint32;
  options: ptr IRShapeOptions
): ptr IRShapedText {.importc, cdecl, header: "ir_text_shaping.h".}

proc ir_shaped_text_destroy*(shaped_text: ptr IRShapedText) {.importc, cdecl, header: "ir_text_shaping.h".}
proc ir_shaped_text_get_width*(shaped_text: ptr IRShapedText): cfloat {.importc, cdecl, header: "ir_text_shaping.h".}
proc ir_shaped_text_get_height*(shaped_text: ptr IRShapedText): cfloat {.importc, cdecl, header: "ir_text_shaping.h".}

# ============================================================================
# Bidirectional Text (BiDi) API
# ============================================================================

type
  IRBidiDirection* {.size: sizeof(cint).} = enum
    IR_BIDI_DIR_LTR = 0      # Left-to-right
    IR_BIDI_DIR_RTL          # Right-to-left
    IR_BIDI_DIR_WEAK_LTR     # Weak LTR (auto-detect, prefer LTR)
    IR_BIDI_DIR_WEAK_RTL     # Weak RTL (auto-detect, prefer RTL)
    IR_BIDI_DIR_NEUTRAL      # Neutral (auto-detect from content)

  IRBidiResult* {.importc, header: "ir_text_shaping.h".} = object
    visual_to_logical*: ptr uint32
    logical_to_visual*: ptr uint32
    embedding_levels*: ptr uint8
    base_direction*: IRBidiDirection
    length*: uint32

# BiDi functions
proc ir_bidi_available*(): bool {.importc, cdecl, header: "ir_text_shaping.h".}

proc ir_bidi_detect_direction*(text: cstring; length: uint32): IRBidiDirection {.importc, cdecl, header: "ir_text_shaping.h".}

proc ir_bidi_reorder*(
  text: ptr uint32;
  length: uint32;
  base_dir: IRBidiDirection
): ptr IRBidiResult {.importc, cdecl, header: "ir_text_shaping.h".}

proc ir_bidi_utf8_to_utf32*(
  utf8_text: cstring;
  utf8_length: uint32;
  out_length: ptr uint32
): ptr uint32 {.importc, cdecl, header: "ir_text_shaping.h".}

proc ir_bidi_result_destroy*(result: ptr IRBidiResult) {.importc, cdecl, header: "ir_text_shaping.h".}

# ============================================================================
# Hot Reload System API
# ============================================================================

type
  IRFileEventType* {.size: sizeof(cint).} = enum
    IR_FILE_EVENT_CREATED = 0
    IR_FILE_EVENT_MODIFIED
    IR_FILE_EVENT_DELETED
    IR_FILE_EVENT_MOVED

  IRReloadResult* {.size: sizeof(cint).} = enum
    IR_RELOAD_SUCCESS = 0
    IR_RELOAD_BUILD_FAILED
    IR_RELOAD_NO_CHANGES
    IR_RELOAD_ERROR

  IRFileEvent* {.importc, header: "ir_hot_reload.h".} = object
    `type`*: IRFileEventType
    path*: cstring
    old_path*: cstring
    timestamp*: uint64

  IRFileWatcher* {.importc, header: "ir_hot_reload.h".} = object
  IRStateSnapshot* {.importc, header: "ir_hot_reload.h".} = object
  IRHotReloadContext* {.importc, header: "ir_hot_reload.h".} = object

  IRFileWatchCallback* = proc(event: ptr IRFileEvent; user_data: pointer) {.cdecl.}
  IRRebuildCallback* = proc(user_data: pointer): ptr IRComponent {.cdecl.}

# File watcher functions
proc ir_file_watcher_create*(): ptr IRFileWatcher {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_file_watcher_add_path*(
  watcher: ptr IRFileWatcher;
  path: cstring;
  recursive: bool
): bool {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_file_watcher_remove_path*(
  watcher: ptr IRFileWatcher;
  path: cstring
): bool {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_file_watcher_set_callback*(
  watcher: ptr IRFileWatcher;
  callback: IRFileWatchCallback;
  user_data: pointer
) {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_file_watcher_poll*(
  watcher: ptr IRFileWatcher;
  timeout_ms: cint
): cint {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_file_watcher_is_active*(watcher: ptr IRFileWatcher): bool {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_file_watcher_destroy*(watcher: ptr IRFileWatcher) {.importc, cdecl, header: "ir_hot_reload.h".}

# State preservation functions
proc ir_capture_state*(root: ptr IRComponent): ptr IRStateSnapshot {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_restore_state*(
  root: ptr IRComponent;
  snapshot: ptr IRStateSnapshot
) {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_state_snapshot_destroy*(snapshot: ptr IRStateSnapshot) {.importc, cdecl, header: "ir_hot_reload.h".}

# Hot reload context functions
proc ir_hot_reload_create*(): ptr IRHotReloadContext {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_hot_reload_set_rebuild_callback*(
  ctx: ptr IRHotReloadContext;
  callback: IRRebuildCallback;
  user_data: pointer
) {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_hot_reload_watch_file*(
  ctx: ptr IRHotReloadContext;
  path: cstring
): bool {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_hot_reload_watch_directory*(
  ctx: ptr IRHotReloadContext;
  path: cstring
): bool {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_hot_reload_poll*(
  ctx: ptr IRHotReloadContext;
  current_root: ptr ptr IRComponent
): IRReloadResult {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_hot_reload_set_enabled*(
  ctx: ptr IRHotReloadContext;
  enabled: bool
) {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_hot_reload_is_enabled*(ctx: ptr IRHotReloadContext): bool {.importc, cdecl, header: "ir_hot_reload.h".}

proc ir_hot_reload_destroy*(ctx: ptr IRHotReloadContext) {.importc, cdecl, header: "ir_hot_reload.h".}

# ============================================================================
# IR Buffer for Serialization
# ============================================================================

type
  IRBuffer* {.importc: "IRBuffer", header: "ir_core.h".} = object
    data*: ptr uint8
    base*: ptr uint8
    size*: csize_t
    capacity*: csize_t

# ============================================================================
# Reactive Manifest Types and Functions
# ============================================================================

type
  IRReactiveVarType* {.size: sizeof(cint).} = enum
    IR_REACTIVE_TYPE_INT = 0
    IR_REACTIVE_TYPE_FLOAT = 1
    IR_REACTIVE_TYPE_STRING = 2
    IR_REACTIVE_TYPE_BOOL = 3
    IR_REACTIVE_TYPE_CUSTOM = 4

  IRBindingType* {.size: sizeof(cint).} = enum
    IR_BINDING_TEXT = 0
    IR_BINDING_CONDITIONAL = 1
    IR_BINDING_ATTRIBUTE = 2
    IR_BINDING_FOR_LOOP = 3
    IR_BINDING_CUSTOM = 4

  IRReactiveValue* {.importc: "IRReactiveValue", header: "ir_core.h", union.} = object
    as_int*: int32
    as_float*: float64
    as_string*: cstring
    as_bool*: bool

  IRReactiveVarDescriptor* {.importc: "IRReactiveVarDescriptor", header: "ir_core.h".} = object
    id*: uint32
    name*: cstring
    `type`*: IRReactiveVarType
    value*: IRReactiveValue
    version*: uint32
    source_location*: cstring
    binding_count*: uint32

  IRReactiveBinding* {.importc: "IRReactiveBinding", header: "ir_core.h".} = object
    component_id*: uint32
    reactive_var_id*: uint32
    binding_type*: IRBindingType
    expression*: cstring
    update_code*: cstring

  IRReactiveConditional* {.importc: "IRReactiveConditional", header: "ir_core.h".} = object
    component_id*: uint32
    condition*: cstring
    last_eval_result*: bool
    suspended*: bool
    dependent_var_ids*: ptr uint32
    dependent_var_count*: uint32

  IRReactiveForLoop* {.importc: "IRReactiveForLoop", header: "ir_core.h".} = object
    parent_component_id*: uint32
    collection_expr*: cstring
    collection_var_id*: uint32
    item_template*: cstring
    child_component_ids*: ptr uint32
    child_count*: uint32

  # Component definition types (for custom components) - must be before IRReactiveManifest
  IRComponentProp* {.importc: "IRComponentProp", header: "ir_core.h".} = object
    name*: cstring
    `type`*: cstring
    default_value*: cstring

  IRComponentStateVar* {.importc: "IRComponentStateVar", header: "ir_core.h".} = object
    name*: cstring
    `type`*: cstring
    initial_expr*: cstring

  IRComponentDefinition* {.importc: "IRComponentDefinition", header: "ir_core.h".} = object
    name*: cstring
    props*: ptr IRComponentProp
    prop_count*: uint32
    state_vars*: ptr IRComponentStateVar
    state_var_count*: uint32
    template_root*: ptr IRComponent

  IRSourceEntry* {.importc: "IRSourceEntry", header: "ir_core.h".} = object
    lang*: cstring
    code*: cstring

  IRReactiveManifest* {.importc: "IRReactiveManifest", header: "ir_core.h".} = object
    variables*: ptr IRReactiveVarDescriptor
    variable_count*: uint32
    variable_capacity*: uint32
    bindings*: ptr IRReactiveBinding
    binding_count*: uint32
    binding_capacity*: uint32
    conditionals*: ptr IRReactiveConditional
    conditional_count*: uint32
    conditional_capacity*: uint32
    for_loops*: ptr IRReactiveForLoop
    for_loop_count*: uint32
    for_loop_capacity*: uint32
    # Component definitions (custom components)
    component_defs*: ptr IRComponentDefinition
    component_def_count*: uint32
    component_def_capacity*: uint32
    # Source code entries (for round-trip preservation)
    sources*: ptr IRSourceEntry
    source_count*: uint32
    source_capacity*: uint32
    # Metadata
    next_var_id*: uint32

# Reactive manifest management functions
proc ir_reactive_manifest_create*(): ptr IRReactiveManifest {.importc, cdecl, header: "ir_core.h".}
proc ir_reactive_manifest_destroy*(manifest: ptr IRReactiveManifest) {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_add_var*(
  manifest: ptr IRReactiveManifest;
  name: cstring;
  `type`: IRReactiveVarType;
  value: IRReactiveValue
): uint32 {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_set_var_metadata*(
  manifest: ptr IRReactiveManifest;
  var_id: uint32;
  type_string: cstring;
  initial_value_json: cstring;
  scope: cstring
) {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_add_binding*(
  manifest: ptr IRReactiveManifest;
  component_id: uint32;
  reactive_var_id: uint32;
  binding_type: IRBindingType;
  expression: cstring
) {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_add_conditional*(
  manifest: ptr IRReactiveManifest;
  component_id: uint32;
  condition: cstring;
  dependent_var_ids: ptr uint32;
  dependent_var_count: uint32
) {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_set_conditional_branches*(
  manifest: ptr IRReactiveManifest;
  component_id: uint32;
  then_children_ids: ptr uint32;
  then_children_count: uint32;
  else_children_ids: ptr uint32;
  else_children_count: uint32
) {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_add_for_loop*(
  manifest: ptr IRReactiveManifest;
  parent_component_id: uint32;
  collection_expr: cstring;
  collection_var_id: uint32
) {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_find_var*(
  manifest: ptr IRReactiveManifest;
  name: cstring
): ptr IRReactiveVarDescriptor {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_get_var*(
  manifest: ptr IRReactiveManifest;
  var_id: uint32
): ptr IRReactiveVarDescriptor {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_update_var*(
  manifest: ptr IRReactiveManifest;
  var_id: uint32;
  new_value: IRReactiveValue
): bool {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_print*(manifest: ptr IRReactiveManifest) {.importc, cdecl, header: "ir_core.h".}

# Component definition functions
proc ir_reactive_manifest_add_component_def*(
  manifest: ptr IRReactiveManifest;
  name: cstring;
  props: ptr IRComponentProp;
  prop_count: uint32;
  state_vars: ptr IRComponentStateVar;
  state_var_count: uint32;
  template_root: ptr IRComponent
) {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_find_component_def*(
  manifest: ptr IRReactiveManifest;
  name: cstring
): ptr IRComponentDefinition {.importc, cdecl, header: "ir_core.h".}

# Source code management (for round-trip preservation)
proc ir_reactive_manifest_add_source*(
  manifest: ptr IRReactiveManifest;
  lang: cstring;
  code: cstring
) {.importc, cdecl, header: "ir_core.h".}

proc ir_reactive_manifest_get_source*(
  manifest: ptr IRReactiveManifest;
  lang: cstring
): cstring {.importc, cdecl, header: "ir_core.h".}
