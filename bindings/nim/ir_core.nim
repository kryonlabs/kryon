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

  IRColor* = object
    r*: uint8
    g*: uint8
    b*: uint8
    a*: uint8

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

proc ir_set_background_color*(style: ptr IRStyle; r: uint8; g: uint8; b: uint8; a: uint8) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_border*(style: ptr IRStyle; width: cfloat; r: uint8; g: uint8; b: uint8; a: uint8; radius: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_margin*(style: ptr IRStyle; top: cfloat; right: cfloat; bottom: cfloat; left: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_padding*(style: ptr IRStyle; top: cfloat; right: cfloat; bottom: cfloat; left: cfloat) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_font*(style: ptr IRStyle; size: cfloat; family: cstring; r: uint8; g: uint8; b: uint8; a: uint8; bold: bool; italic: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_visible*(style: ptr IRStyle; visible: bool) {.importc, cdecl, header: "ir_builder.h".}
proc ir_set_z_index*(style: ptr IRStyle; z_index: uint32) {.importc, cdecl, header: "ir_builder.h".}

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
