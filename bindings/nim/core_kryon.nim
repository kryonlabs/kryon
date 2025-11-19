## Kryon C Core Direct Bindings
##
## Direct Nim bindings to the Kryon C99 ABI
## This is the glue layer that connects Nim to the C core

import strutils

# Import C malloc for memory management consistency with C code
proc c_malloc(size: csize_t): pointer {.importc: "malloc", header: "<stdlib.h>".}

when defined(KRYON_NO_FLOAT):
  {.warning: "KRYON_NO_FLOAT symbol detected at compile time".}

## Platform Configuration
const
  hostIsMcu = defined(mcu)
  hostIsWeb = defined(web)
  hostNoFloat = hostIsMcu or defined(KRYON_NO_FLOAT)  ## Use fixed-point on MCU or when explicitly defined

when hostIsMcu:
  const
    KRYON_TARGET_PLATFORM* = 1
    KRYON_NO_HEAP* = true
    KRYON_NO_FLOAT* = hostNoFloat
elif hostIsWeb:
  const
    KRYON_TARGET_PLATFORM* = 2
    KRYON_NO_HEAP* = false
    KRYON_NO_FLOAT* = hostNoFloat
else:
  const
    KRYON_TARGET_PLATFORM* = 0
    KRYON_NO_HEAP* = false
    KRYON_NO_FLOAT* = hostNoFloat

## Fixed-point Type Mapping
when KRYON_NO_FLOAT:
  type
    KryonFp* = distinct int32
else:
  type
    KryonFp* = float32

## C Types and Enums
type
  KryonEventType* {.importc: "kryon_event_type_t", header: "kryon.h".} = enum
    Click = 0, Touch = 1, Key = 2, Timer = 3, Hover = 4, Scroll = 5,
    Focus = 6, Blur = 7, Custom = 8

  KryonCommandType* {.importc: "kryon_command_type_t", header: "kryon.h".} = enum
    DrawRect = 0, DrawText = 1, DrawLine = 2, DrawArc = 3, DrawTexture = 4,
    SetClip = 5, PushClip = 6, PopClip = 7, SetTransform = 8,
    PushTransform = 9, PopTransform = 10, DrawPolygon = 11

  KryonAlignment* {.importc: "kryon_alignment_t", header: "kryon.h".} = enum
    kaStart = 0,
    kaCenter = 1,
    kaEnd = 2,
    kaStretch = 3,
    kaSpaceEvenly = 4,
    kaSpaceAround = 5,
    kaSpaceBetween = 6

  KryonComponentObj* {.importc: "kryon_component_t", nodecl, header: "kryon.h".} = object
  KryonComponent* = ptr KryonComponentObj

  KryonCmdBufObj* {.importc: "kryon_cmd_buf_t", nodecl, header: "kryon.h".} = object
  KryonCmdBuf* = ptr KryonCmdBufObj

  KryonRendererObj* {.importc: "kryon_renderer_t", nodecl, header: "kryon.h".} = object
  KryonRenderer* = ptr KryonRendererObj

  KryonRendererOps* {.importc: "kryon_renderer_ops_t", nodecl, header: "kryon.h".} = object

  KryonEvent* {.importc: "kryon_event_t", bycopy, header: "kryon.h".} = object
    `type`*: KryonEventType
    x*: int16
    y*: int16
    param*: uint32
    data*: pointer
    timestamp*: uint32

  KryonCommand* {.importc: "kryon_command_t", header: "kryon.h".} = object
    commandType*: KryonCommandType

  KryonComponentOps* {.importc: "kryon_component_ops_t", bycopy, header: "kryon.h".} = object
    render*: proc (self: KryonComponent; buf: KryonCmdBuf) {.cdecl.}
    onEvent* {.importc: "on_event".}: proc (self: KryonComponent; evt: ptr KryonEvent): bool {.cdecl.}
    destroy*: proc (self: KryonComponent) {.cdecl.}
    layout*: proc (self: KryonComponent; availableWidth: KryonFp; availableHeight: KryonFp) {.cdecl.}

const
  KRYON_COMPONENT_FLAG_HAS_X* = uint8(0x01)
  KRYON_COMPONENT_FLAG_HAS_Y* = uint8(0x02)
  KRYON_COMPONENT_FLAG_HAS_WIDTH* = uint8(0x04)
  KRYON_COMPONENT_FLAG_HAS_HEIGHT* = uint8(0x08)
  KRYON_MAX_TEXT_LENGTH* = 64
  KRYON_MAX_DROPDOWN_OPTIONS* = 16

# Component State Types
type
  KryonTextState* {.importc: "kryon_text_state_t", bycopy, header: "kryon.h".} = object
    text*: cstring
    font_id*: uint16
    font_size*: uint8      # Font size in pixels (0 = use default)
    font_weight*: uint8    # 0=normal, 1=bold
    font_style*: uint8     # 0=normal, 1=italic
    max_length*: uint16
    word_wrap*: bool

  KryonCodeState* {.importc: "kryon_code_state_t", bycopy, header: "kryon.h".} = object
    bg_color*: uint32      # Background color (default: light gray)
    fg_color*: uint32      # Foreground color (default: dark gray)

  KryonButtonState* {.importc: "kryon_button_state_t", bycopy, header: "kryon.h".} = object
    text*: cstring
    font_id*: uint16
    font_size*: uint8
    pressed*: bool
    hovered*: bool
    center_text*: bool
    ellipsize*: bool
    on_click*: proc (component: KryonComponent; event: ptr KryonEvent) {.cdecl.}

  KryonCheckboxState* {.importc: "kryon_checkbox_state_t", bycopy, header: "kryon.h".} = object
    checked*: bool
    check_size*: uint16
    check_color*: uint32
    box_color*: uint32
    text_color*: uint32
    label*: array[KRYON_MAX_TEXT_LENGTH, char]
    font_id*: uint16
    on_change*: proc (checkbox: KryonComponent; checked: bool) {.cdecl.}

  KryonDropdownState* {.importc: "kryon_dropdown_state_t", bycopy, header: "kryon.h".} = object
    placeholder*: array[KRYON_MAX_TEXT_LENGTH, char]
    options*: array[KRYON_MAX_DROPDOWN_OPTIONS, cstring]
    option_count*: uint8
    selected_index*: int8
    is_open*: bool
    hovered_index*: int8
    font_id*: uint16
    font_size*: uint8
    text_color*: uint32
    background_color*: uint32
    border_color*: uint32
    hover_color*: uint32
    border_width*: uint8
    on_change*: proc (dropdown: KryonComponent; selected_index: int8) {.cdecl.}

## C API Functions - Component Lifecycle
{.passC: "-I../kryon/core/include".}
{.passC: "-DKRYON_CMD_BUF_SIZE=131072".}  ## Force 128KB buffer size for desktop
{.emit: "#include <kryon.h>".}

{.push importc, cdecl, header: "kryon.h", nodecl.}

proc kryon_component_create*(ops: ptr KryonComponentOps; initialState: pointer): KryonComponent
proc kryon_component_destroy*(component: KryonComponent)
proc kryon_component_add_child*(parent: KryonComponent; child: KryonComponent): bool
proc kryon_component_remove_child*(parent: KryonComponent; child: KryonComponent)
proc kryon_component_get_parent*(component: KryonComponent): KryonComponent
proc kryon_component_get_child*(component: KryonComponent; index: uint8): KryonComponent
proc kryon_component_get_child_count*(component: KryonComponent): uint8

## Component Properties
proc kryon_component_set_bounds_mask*(component: KryonComponent; x, y, width, height: KryonFp; explicitMask: uint8)
proc kryon_component_set_bounds*(component: KryonComponent; x, y, width, height: KryonFp)
proc kryon_component_set_z_index*(component: KryonComponent; zIndex: uint16)
proc kryon_component_get_z_index*(component: KryonComponent): uint16
proc kryon_component_get_layout_flags*(component: KryonComponent): uint8
proc kryon_component_set_padding*(component: KryonComponent; top, right, bottom, left: uint8)
proc kryon_component_set_margin*(component: KryonComponent; top, right, bottom, left: uint8)
proc kryon_component_set_background_color*(component: KryonComponent; color: uint32)
proc kryon_component_set_border_color*(component: KryonComponent; color: uint32)
proc kryon_component_set_border_width*(component: KryonComponent; width: uint8)
proc kryon_component_set_text_color*(component: KryonComponent; color: uint32)
proc kryon_component_set_text*(component: KryonComponent; text: cstring)
proc kryon_component_set_layout_alignment*(component: KryonComponent; justify, align: KryonAlignment)
proc kryon_component_set_layout_direction*(component: KryonComponent; direction: uint8)
proc kryon_component_set_gap*(component: KryonComponent; gap: uint8)
proc kryon_component_set_visible*(component: KryonComponent; visible: bool)
proc kryon_component_set_scrollable*(component: KryonComponent; scrollable: bool)
proc kryon_component_set_scroll_offset*(component: KryonComponent; offset_x, offset_y: KryonFp)
proc kryon_component_get_scroll_offset*(component: KryonComponent; offset_x, offset_y: ptr KryonFp)
proc kryon_component_get_absolute_bounds*(component: KryonComponent;
                                          abs_x, abs_y, width, height: ptr KryonFp)
proc kryon_component_set_flex*(component: KryonComponent; flexGrow, flexShrink: uint8)
proc kryon_component_mark_dirty*(component: KryonComponent)
proc kryon_component_mark_clean*(component: KryonComponent)
proc kryon_component_is_dirty*(component: KryonComponent): bool

## Event System
proc kryon_component_send_event*(component: KryonComponent; event: ptr KryonEvent)
proc kryon_component_add_event_handler*(component: KryonComponent; handler: proc (component: KryonComponent; event: ptr KryonEvent) {.cdecl.}): bool
proc kryon_component_is_button*(component: KryonComponent): bool
proc kryon_event_dispatch_to_point*(root: KryonComponent; x, y: int16; event: ptr KryonEvent)
proc kryon_event_find_target_at_point*(root: KryonComponent; x, y: int16): KryonComponent
proc kryon_button_set_center_text*(component: KryonComponent; center: bool)
proc kryon_button_set_ellipsize*(component: KryonComponent; ellipsize: bool)
proc kryon_button_set_font_size*(component: KryonComponent; fontSize: uint8)

## Command Buffer
proc kryon_cmd_buf_init*(buf: KryonCmdBuf)
proc kryon_cmd_buf_clear*(buf: KryonCmdBuf)
proc kryon_cmd_buf_push*(buf: KryonCmdBuf; cmd: ptr KryonCommand): bool
proc kryon_cmd_buf_pop*(buf: KryonCmdBuf; cmd: ptr KryonCommand): bool
proc kryon_cmd_buf_count*(buf: KryonCmdBuf): uint16
proc kryon_cmd_buf_is_full*(buf: KryonCmdBuf): bool
proc kryon_cmd_buf_is_empty*(buf: KryonCmdBuf): bool

## Drawing Commands
proc kryon_draw_rect*(buf: KryonCmdBuf; x, y: int16; w, h: uint16; color: uint32): bool
proc kryon_draw_text*(buf: KryonCmdBuf; text: cstring; x, y: int16; fontId: uint16; fontSize: uint8; fontWeight: uint8; fontStyle: uint8; color: uint32): bool
proc kryon_draw_line*(buf: KryonCmdBuf; x1, y1, x2, y2: int16; color: uint32): bool
proc kryon_draw_arc*(buf: KryonCmdBuf; cx, cy: int16; radius: uint16; startAngle, endAngle: int16; color: uint32): bool
proc kryon_draw_polygon*(buf: KryonCmdBuf; vertices: ptr KryonFp; vertexCount: uint16; color: uint32; filled: bool): bool

## Layout System
proc kryon_layout_tree*(root: KryonComponent; availableWidth, availableHeight: KryonFp)
proc kryon_layout_component*(component: KryonComponent; availableWidth, availableHeight: KryonFp)

## Renderer Management
proc kryon_renderer_create*(ops: ptr KryonRendererOps): KryonRenderer
proc kryon_renderer_destroy*(renderer: KryonRenderer)
proc kryon_renderer_init*(renderer: KryonRenderer; nativeWindow: pointer): bool
proc kryon_renderer_shutdown*(renderer: KryonRenderer)
proc kryon_renderer_get_dimensions*(renderer: KryonRenderer; width, height: ptr uint16)
proc kryon_renderer_set_clear_color*(renderer: KryonRenderer; color: uint32)

## Global renderer for text measurement
proc kryon_set_global_renderer*(renderer: KryonRenderer)
proc kryon_get_global_renderer*(): KryonRenderer

## Main Rendering Loop
proc kryon_render_frame*(renderer: KryonRenderer; rootComponent: KryonComponent)

## Utility Functions
proc kryon_color_rgba*(r, g, b, a: uint8): uint32
proc kryon_color_rgb*(r, g, b: uint8): uint32
proc kryon_color_get_components*(color: uint32; r, g, b, a: ptr uint8)

## Fixed-point utilities
proc kryon_fp_from_float*(f: cfloat): KryonFp
proc kryon_fp_to_float*(fp: KryonFp): cfloat
proc kryon_fp_from_int*(i: int32): KryonFp
proc kryon_fp_to_int_round*(fp: KryonFp): int32

## Built-in Component Operations
var kryon_container_ops* {.importc: "kryon_container_ops", nodecl.}: KryonComponentOps
var kryon_text_ops* {.importc: "kryon_text_ops", nodecl.}: KryonComponentOps
var kryon_button_ops* {.importc: "kryon_button_ops", nodecl.}: KryonComponentOps
var kryon_dropdown_ops* {.importc: "kryon_dropdown_ops", nodecl.}: KryonComponentOps
var kryon_checkbox_ops* {.importc: "kryon_checkbox_ops", nodecl.}: KryonComponentOps

## Heading Component Operations (H1-H6) - Native first-class components
var kryon_h1_ops* {.importc: "kryon_h1_ops", nodecl.}: KryonComponentOps
var kryon_h2_ops* {.importc: "kryon_h2_ops", nodecl.}: KryonComponentOps
var kryon_h3_ops* {.importc: "kryon_h3_ops", nodecl.}: KryonComponentOps
var kryon_h4_ops* {.importc: "kryon_h4_ops", nodecl.}: KryonComponentOps
var kryon_h5_ops* {.importc: "kryon_h5_ops", nodecl.}: KryonComponentOps
var kryon_h6_ops* {.importc: "kryon_h6_ops", nodecl.}: KryonComponentOps

## Inline component ops (native styled text)
var kryon_span_ops* {.importc: "kryon_span_ops", nodecl.}: KryonComponentOps
var kryon_bold_ops* {.importc: "kryon_bold_ops", nodecl.}: KryonComponentOps
var kryon_italic_ops* {.importc: "kryon_italic_ops", nodecl.}: KryonComponentOps
var kryon_underline_ops* {.importc: "kryon_underline_ops", nodecl.}: KryonComponentOps
var kryon_strikethrough_ops* {.importc: "kryon_strikethrough_ops", nodecl.}: KryonComponentOps
var kryon_code_ops* {.importc: "kryon_code_ops", nodecl.}: KryonComponentOps
var kryon_link_ops* {.importc: "kryon_link_ops", nodecl.}: KryonComponentOps
var kryon_highlight_ops* {.importc: "kryon_highlight_ops", nodecl.}: KryonComponentOps

## Checkbox creation function
proc kryon_checkbox_create*(label: cstring; initial_checked: bool;
                           check_size: uint16;
                           on_change: proc (checkbox: KryonComponent; checked: bool) {.cdecl.}): KryonComponent

{.pop.}

## Framebuffer Renderer Functions (internal fallback)
{.push importc, cdecl, header: "kryon.h", nodecl.}
proc kryon_get_framebuffer_renderer_ops*(): ptr KryonRendererOps
proc kryon_framebuffer_renderer_create*(width: uint16; height: uint16; bytesPerPixel: uint8): KryonRenderer
{.pop.}

## SDL3 Renderer Integration
when defined(KRYON_SDL3):
  {.push importc, cdecl, header: "sdl3.h", nodecl.}
  proc kryon_sdl3_renderer_create*(width: uint16, height: uint16, title: cstring): KryonRenderer
  proc kryon_sdl3_renderer_destroy*(renderer: KryonRenderer)
  proc kryon_sdl3_poll_event*(event: ptr KryonEvent): bool
  proc kryon_sdl3_apply_cursor_shape*(shape: uint8)
  proc kryon_sdl3_is_mouse_button_down*(button: uint8): bool

  ## Font management functions
  proc kryon_sdl3_fonts_init*()
  proc kryon_sdl3_fonts_shutdown*()
  proc kryon_sdl3_load_font*(name: cstring, size: uint16): uint16
  proc kryon_sdl3_unload_font*(font_id: uint16)
  proc kryon_sdl3_add_font_search_path*(path: cstring)
  proc kryon_sdl3_get_font_name*(font_id: uint16): cstring
  proc kryon_sdl3_get_font_size*(font_id: uint16): uint16
  proc kryon_sdl3_get_font_height*(font_id: uint16): uint16
  {.pop.}

## Terminal Renderer Integration
when defined(KRYON_TERMINAL):
  {.push importc, cdecl, header: "renderers/terminal/terminal_backend.h", nodecl.}
  proc kryon_terminal_renderer_create*(): KryonRenderer
  proc kryon_terminal_renderer_destroy*(renderer: KryonRenderer)
  {.pop.}

# Canvas ops helper
proc kryon_component_set_canvas_ops*(component: KryonComponent) {.importc: "kryon_component_set_canvas_ops", header: "kryon.h".}
proc kryon_canvas_set_draw_callback*(component: KryonComponent; onDraw: proc(component: KryonComponent; buf: KryonCmdBuf): bool {.cdecl.}) {.importc: "kryon_canvas_set_draw_callback", header: "kryon.h".}
proc kryon_canvas_set_size*(component: KryonComponent; width, height: uint16) {.importc: "kryon_canvas_set_size", header: "kryon.h".}
proc kryon_canvas_component_set_background_color*(component: KryonComponent; color: uint32) {.importc: "kryon_canvas_component_set_background_color", header: "kryon.h".}

# End of C API imports

## ============================================================================
## Nim Convenience Wrappers
## ============================================================================

proc newKryonContainer*(): KryonComponent =
  ## Create a new container component
  kryon_component_create(addr kryon_container_ops, nil)

proc newKryonText*(text: string, fontSize: uint8 = 0, fontWeight: uint8 = 0, fontStyle: uint8 = 0, wordWrap: bool = false): KryonComponent =
  ## Create a new text component with optional font styling
  ## fontSize: Font size in pixels (0 = use default, typically 16)
  ## fontWeight: Font weight (0 = normal, 1 = bold)
  ## fontStyle: Font style (0 = normal, 1 = italic)
  ## wordWrap: Enable word wrapping (default = false)
  var textState: KryonTextState
  let len = text.len
  # Use malloc instead of alloc to match C's free() in kryon_component_set_text
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = fontSize
  textState.font_weight = fontWeight
  textState.font_style = fontStyle
  textState.max_length = 255
  textState.word_wrap = wordWrap

  # Use malloc for state too, for consistency
  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))

  result = kryon_component_create(addr kryon_text_ops, statePtr)

proc newKryonButton*(text: string; onClickCallback: proc (component: KryonComponent; event: ptr KryonEvent) {.cdecl.} = nil): KryonComponent =
  ## Create a new button component
  var buttonState = KryonButtonState(
    font_id: 0,
    font_size: 16,
    pressed: false,
    hovered: false,
    center_text: true,
    ellipsize: true,
    on_click: onClickCallback
  )
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  buttonState.text = cast[cstring](buffer)

  let statePtr = c_malloc(csize_t(sizeof(KryonButtonState)))
  copyMem(statePtr, addr buttonState, sizeof(KryonButtonState))

  result = kryon_component_create(addr kryon_button_ops, statePtr)

# ============================================================================
# Heading Components (H1-H6) - Native first-class components
# ============================================================================

proc newKryonH1*(text: string): KryonComponent =
  ## Create a native H1 heading component (32px font)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = 32  # H1 font size
  textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
  textState.font_style = 0   # KRYON_FONT_STYLE_NORMAL
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_h1_ops, statePtr)

proc newKryonH2*(text: string): KryonComponent =
  ## Create a native H2 heading component (28px font)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = 28  # H2 font size
  textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
  textState.font_style = 0   # KRYON_FONT_STYLE_NORMAL
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_h2_ops, statePtr)

proc newKryonH3*(text: string): KryonComponent =
  ## Create a native H3 heading component (24px font)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = 24  # H3 font size
  textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
  textState.font_style = 0   # KRYON_FONT_STYLE_NORMAL
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_h3_ops, statePtr)

proc newKryonH4*(text: string): KryonComponent =
  ## Create a native H4 heading component (20px font)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = 20  # H4 font size
  textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
  textState.font_style = 0   # KRYON_FONT_STYLE_NORMAL
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_h4_ops, statePtr)

proc newKryonH5*(text: string): KryonComponent =
  ## Create a native H5 heading component (18px font)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = 18  # H5 font size
  textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
  textState.font_style = 0   # KRYON_FONT_STYLE_NORMAL
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_h5_ops, statePtr)

proc newKryonH6*(text: string): KryonComponent =
  ## Create a native H6 heading component (16px font)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = 16  # H6 font size
  textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
  textState.font_style = 0   # KRYON_FONT_STYLE_NORMAL
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_h6_ops, statePtr)

proc newKryonBold*(text: string; fontSize: uint8 = 0): KryonComponent =
  ## Create NATIVE bold component (uses kryon_bold_ops for rich text)
  ## fontSize: Font size in pixels (0 = use default, typically 16)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = fontSize
  textState.font_weight = 0  # Will be applied by Bold ops
  textState.font_style = 0
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_bold_ops, statePtr)

proc newKryonItalic*(text: string; fontSize: uint8 = 0): KryonComponent =
  ## Create NATIVE italic component (uses kryon_italic_ops for rich text)
  ## fontSize: Font size in pixels (0 = use default, typically 16)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = fontSize
  textState.font_weight = 0
  textState.font_style = 0  # Will be applied by Italic ops
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_italic_ops, statePtr)

proc newKryonCode*(text: string; fontSize: uint8 = 0): KryonComponent =
  ## Create NATIVE inline code component (uses kryon_code_ops for rich text)
  ## fontSize: Font size in pixels (0 = use default, typically 16)
  ## Note: Code uses monospace font with gray background and dark text
  ## Code is a container - the text is stored in a child Text component

  # Create Code container with colors
  var codeState: KryonCodeState
  # Standard markdown code colors:
  # Light gray background: #F0F0F0 = RGB(240,240,240)
  # Dark gray text: #333333 = RGB(51,51,51)
  codeState.bg_color = 0xF0F0F0FF'u32  # Light gray background
  codeState.fg_color = 0x333333FF'u32  # Dark gray text

  let statePtr = c_malloc(csize_t(sizeof(KryonCodeState)))
  copyMem(statePtr, addr codeState, sizeof(KryonCodeState))
  result = kryon_component_create(addr kryon_code_ops, statePtr)

  # Create Text child with the actual text
  if text.len > 0:
    var textState: KryonTextState
    let len = text.len
    let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
    copyMem(addr buffer[0], unsafeAddr text[0], len)
    buffer[len] = '\0'
    textState.text = cast[cstring](buffer)
    textState.font_id = 1  # Font ID 1 reserved for monospace
    textState.font_size = fontSize
    textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
    textState.font_style = 0   # KRYON_FONT_STYLE_NORMAL
    textState.max_length = 255
    textState.word_wrap = false

    let textStatePtr = c_malloc(csize_t(sizeof(KryonTextState)))
    copyMem(textStatePtr, addr textState, sizeof(KryonTextState))
    let textComp = kryon_component_create(addr kryon_text_ops, textStatePtr)

    # Add Text as child of Code
    discard kryon_component_add_child(result, textComp)

proc newKryonCodeBlock*(text: string; fontSize: uint8 = 14): KryonComponent =
  ## Create code block (multiline monospace text, for ```code```)
  ## fontSize: Font size in pixels (default 14 for code blocks)
  ## Note: Backend renderers should use monospace font and add background
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 1  # Font ID 1 reserved for monospace
  textState.font_size = fontSize
  textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
  textState.font_style = 0   # KRYON_FONT_STYLE_NORMAL
  textState.max_length = 255
  textState.word_wrap = true  # Code blocks can wrap

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_text_ops, statePtr)

proc newKryonBlockquote*(text: string; fontSize: uint8 = 0): KryonComponent =
  ## Create blockquote text (for > blockquote in markdown)
  ## fontSize: Font size in pixels (0 = use default, typically 16)
  ## Note: Backend renderers should add indentation and left border styling
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = fontSize
  textState.font_weight = 0  # KRYON_FONT_WEIGHT_NORMAL
  textState.font_style = 1   # KRYON_FONT_STYLE_ITALIC (blockquotes often italic)
  textState.max_length = 255
  textState.word_wrap = true  # Blockquotes can wrap

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_text_ops, statePtr)

proc newKryonUnderline*(text: string; fontSize: uint8 = 0): KryonComponent =
  ## Create NATIVE underline component (uses kryon_underline_ops for rich text)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = fontSize
  textState.font_weight = 0
  textState.font_style = 0
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_underline_ops, statePtr)

proc newKryonStrikethrough*(text: string; fontSize: uint8 = 0): KryonComponent =
  ## Create NATIVE strikethrough component (uses kryon_strikethrough_ops for rich text)
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = fontSize
  textState.font_weight = 0
  textState.font_style = 0
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_strikethrough_ops, statePtr)

proc newKryonSpan*(text: string; fontSize: uint8 = 0; fgColor: uint32 = 0; bgColor: uint32 = 0): KryonComponent =
  ## Create NATIVE span component (uses kryon_span_ops for rich text)
  ## Generic inline text with custom styling
  var textState: KryonTextState
  let len = text.len
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.font_size = fontSize
  textState.font_weight = 0
  textState.font_style = 0
  textState.max_length = 255
  textState.word_wrap = false

  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))
  result = kryon_component_create(addr kryon_span_ops, statePtr)

proc newKryonCheckbox*(label: string; initialChecked: bool = false;
                      onChangeCallback: proc (checkbox: KryonComponent; checked: bool) {.cdecl.} = nil): KryonComponent =
  ## Create a new checkbox component
  let labelCStr = if label.len > 0: cstring(label) else: nil
  result = kryon_checkbox_create(labelCStr, initialChecked, 16, onChangeCallback)

proc newKryonDropdown*(placeholder: string = "Select...";
                      options: seq[string] = @[];
                      selectedIndex: int = -1;
                      fontSize: uint8 = 14;
                      textColor: uint32 = 0xFF000000'u32;
                      backgroundColor: uint32 = 0xFFFFFFFF'u32;
                      borderColor: uint32 = 0xFFCCCCCC'u32;
                      borderWidth: uint8 = 1;
                      hoverColor: uint32 = 0xFFEEEEFF'u32;
                      onChangeCallback: proc (dropdown: KryonComponent; selected_index: int8) {.cdecl.} = nil): KryonComponent =
  ## Create a new dropdown component
  var dropdownState: KryonDropdownState

  # Copy placeholder text
  let placeholderLen = min(placeholder.len, KRYON_MAX_TEXT_LENGTH - 1)
  if placeholderLen > 0:
    copyMem(addr dropdownState.placeholder[0], unsafeAddr placeholder[0], placeholderLen)
  dropdownState.placeholder[placeholderLen] = '\0'

  # Copy options - allocate each string with malloc
  dropdownState.option_count = uint8(min(options.len, KRYON_MAX_DROPDOWN_OPTIONS))
  for i in 0 ..< dropdownState.option_count.int:
    let optLen = options[i].len
    let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(optLen + 1)))
    if optLen > 0:
      copyMem(addr buffer[0], unsafeAddr options[i][0], optLen)
    buffer[optLen] = '\0'
    dropdownState.options[i] = cast[cstring](buffer)

  # Initialize state with provided parameters
  dropdownState.selected_index = int8(selectedIndex)
  dropdownState.is_open = false
  dropdownState.hovered_index = -1
  dropdownState.font_id = 0
  dropdownState.font_size = fontSize
  dropdownState.text_color = textColor
  dropdownState.background_color = backgroundColor
  dropdownState.border_color = borderColor
  dropdownState.hover_color = hoverColor
  dropdownState.border_width = borderWidth
  dropdownState.on_change = onChangeCallback

  # Use malloc for state
  let statePtr = c_malloc(csize_t(sizeof(KryonDropdownState)))
  copyMem(statePtr, addr dropdownState, sizeof(KryonDropdownState))

  result = kryon_component_create(addr kryon_dropdown_ops, statePtr)

proc newKryonRenderer*(): KryonRenderer =
  ## Create a new framebuffer renderer
  kryon_renderer_create(kryon_get_framebuffer_renderer_ops())

proc createKryonCmdBuf*(): KryonCmdBuf =
  ## Create new command buffer
  result = cast[KryonCmdBuf](alloc(sizeof(KryonCmdBuf)))
  kryon_cmd_buf_init(result)

## ============================================================================
## Type Converters and Helpers
## ============================================================================

when KRYON_NO_FLOAT:
  proc fixedInt*(value: int): KryonFp {.inline.} =
    KryonFp(value.int32 shl 16)

  proc fixedFloat*(value: float): KryonFp {.inline.} =
    KryonFp(kryon_fp_from_float(value.float32))

  converter toKryonFp*(i: int): KryonFp = fixedInt(i)
  converter toKryonFp*(f: float): KryonFp = fixedFloat(f)

  converter fromKryonFp*(fp: KryonFp): float =
    float(kryon_fp_to_int_round(fp))
else:
  converter toKryonFp*(i: int): KryonFp = kryon_fp_from_int(i.int32)
  converter toKryonFp*(f: float): KryonFp = kryon_fp_from_float(f)
  converter fromKryonFp*(fp: KryonFp): float = kryon_fp_to_float(fp)

proc toFixed*(value: int): KryonFp {.inline.} =
  toKryonFp(value)

proc toFixed*(value: float): KryonFp {.inline.} =
  toKryonFp(value)

## ============================================================================
## Property Helpers
## ============================================================================

proc `<<`*(parent: KryonComponent; child: KryonComponent): KryonComponent {.discardable.} =
  ## Add child component to parent (operator syntax)
  discard kryon_component_add_child(parent, child)
  result = parent

proc setBounds*(component: KryonComponent; x, y, width, height: auto) =
  ## Set component bounds with type conversion
  kryon_component_set_bounds(component, toFixed(x), toFixed(y), toFixed(width), toFixed(height))

proc setBackgroundColor*(component: KryonComponent; r, g, b, a: int) =
  ## Set background color with RGBA values
  kryon_component_set_background_color(component, kryon_color_rgba(uint8(r), uint8(g), uint8(b), uint8(a)))

proc setBackgroundColor*(component: KryonComponent; color: uint32) =
  ## Set background color with packed RGBA value
  kryon_component_set_background_color(component, color)

proc setVisible*(component: KryonComponent; visible: bool) =
  ## Set component visibility
  kryon_component_set_visible(component, visible)

proc setText*(component: KryonComponent; text: string) =
  ## Update text content for Text and Button components
  if component != nil:
    kryon_component_set_text(component, cstring(text))

proc layout*(root: KryonComponent; width, height: auto) =
  ## Perform layout on component tree
  kryon_layout_tree(root, toFixed(width), toFixed(height))

proc render*(renderer: KryonRenderer; root: KryonComponent) =
  ## Render component tree
  kryon_render_frame(renderer, root)

## ============================================================================
## Color Utilities (Nim-style)
## ============================================================================

proc rgba*(r, g, b, a: int): uint32 =
  ## Create RGBA color
  kryon_color_rgba(uint8(r), uint8(g), uint8(b), uint8(a))

proc rgb*(r, g, b: int): uint32 =
  ## Create RGB color (full alpha)
  kryon_color_rgb(uint8(r), uint8(g), uint8(b))

proc parseColor*(color: string): uint32 =
  ## Parse color from hex string or CSS name (simplified)
  if color.startsWith("#"):
    let hex = color[1..^1]
    if hex.len == 6:
      let r = parseHexInt(hex[0..1])
      let g = parseHexInt(hex[2..3])
      let b = parseHexInt(hex[4..5])
      return rgb(r, g, b)
  return rgb(0, 0, 0) # Default to black
