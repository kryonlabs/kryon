## Kryon C Core Direct Bindings
##
## Direct Nim bindings to the Kryon C99 ABI
## This is the glue layer that connects Nim to the C core

import strutils

# Import C malloc/free for memory management consistency with C code
proc c_malloc(size: csize_t): pointer {.importc: "malloc", header: "<stdlib.h>".}
proc c_free(p: pointer) {.importc: "free", header: "<stdlib.h>".}

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
    PushTransform = 9, PopTransform = 10

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
    onEvent*: proc (self: KryonComponent; evt: ptr KryonEvent): bool {.cdecl.}
    destroy*: proc (self: KryonComponent) {.cdecl.}
    layout*: proc (self: KryonComponent; availableWidth: KryonFp; availableHeight: KryonFp) {.cdecl.}

const
  KRYON_COMPONENT_FLAG_HAS_X* = uint8(0x01)
  KRYON_COMPONENT_FLAG_HAS_Y* = uint8(0x02)
  KRYON_COMPONENT_FLAG_HAS_WIDTH* = uint8(0x04)
  KRYON_COMPONENT_FLAG_HAS_HEIGHT* = uint8(0x08)
  KRYON_MAX_TEXT_LENGTH* = 64

# Component State Types
type
  KryonTextState* {.importc: "kryon_text_state_t", bycopy, header: "kryon.h".} = object
    text*: cstring
    font_id*: uint16
    max_length*: uint16
    word_wrap*: bool

  KryonButtonState* {.importc: "kryon_button_state_t", bycopy, header: "kryon.h".} = object
    text*: cstring
    font_id*: uint16
    pressed*: bool
    hovered*: bool
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

## C API Functions - Component Lifecycle
{.passC: "-I../../core/include".}
{.passC: "-DKRYON_CMD_BUF_SIZE=32768".}  ## Force large buffer size for desktop
{.emit: "#include <kryon.h>".}

{.push importc, cdecl, header: "kryon.h", nodecl.}

proc kryon_component_create*(ops: ptr KryonComponentOps; initialState: pointer): KryonComponent
proc kryon_component_destroy*(component: KryonComponent)
proc kryon_component_add_child*(parent: KryonComponent; child: KryonComponent): bool
proc kryon_component_remove_child*(parent: KryonComponent; child: KryonComponent): bool
proc kryon_component_get_parent*(component: KryonComponent): KryonComponent
proc kryon_component_get_child*(component: KryonComponent; index: uint8): KryonComponent
proc kryon_component_get_child_count*(component: KryonComponent): uint8

## Component Properties
proc kryon_component_set_bounds_mask*(component: KryonComponent; x, y, width, height: KryonFp; explicitMask: uint8)
proc kryon_component_set_bounds*(component: KryonComponent; x, y, width, height: KryonFp)
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
proc kryon_draw_text*(buf: KryonCmdBuf; text: cstring; x, y: int16; fontId: uint16; color: uint32): bool
proc kryon_draw_line*(buf: KryonCmdBuf; x1, y1, x2, y2: int16; color: uint32): bool
proc kryon_draw_arc*(buf: KryonCmdBuf; cx, cy: int16; radius: uint16; startAngle, endAngle: int16; color: uint32): bool

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

## Main Rendering Loop
proc kryon_render_frame*(renderer: KryonRenderer; rootComponent: KryonComponent)

## Utility Functions
proc kryon_color_rgba*(r, g, b, a: uint8): uint32
proc kryon_color_rgb*(r, g, b: uint8): uint32
proc kryon_color_get_components*(color: uint32; r, g, b, a: ptr uint8)

## Fixed-point utilities
proc kryon_fp_from_float*(f: float32): KryonFp
proc kryon_fp_to_float*(fp: KryonFp): float32
proc kryon_fp_from_int*(i: int32): KryonFp
proc kryon_fp_to_int_round*(fp: KryonFp): int32

## Built-in Component Operations
var kryon_container_ops* {.importc: "kryon_container_ops", nodecl.}: KryonComponentOps
var kryon_text_ops* {.importc: "kryon_text_ops", nodecl.}: KryonComponentOps
var kryon_button_ops* {.importc: "kryon_button_ops", nodecl.}: KryonComponentOps
var kryon_checkbox_ops* {.importc: "kryon_checkbox_ops", nodecl.}: KryonComponentOps

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
  {.push importc, cdecl, header: "renderers/sdl3/sdl3.h", nodecl.}
  proc kryon_sdl3_renderer_create*(width: uint16, height: uint16, title: cstring): KryonRenderer
  proc kryon_sdl3_renderer_destroy*(renderer: KryonRenderer)
  proc kryon_sdl3_poll_event*(event: ptr KryonEvent): bool
  proc kryon_sdl3_apply_cursor_shape*(shape: uint8)
  {.pop.}

## Terminal Renderer Integration
when defined(KRYON_TERMINAL):
  {.push importc, cdecl, header: "renderers/terminal/terminal_backend.h", nodecl.}
  proc kryon_terminal_renderer_create*(): KryonRenderer
  proc kryon_terminal_renderer_destroy*(renderer: KryonRenderer)
  {.pop.}

# End of C API imports

## ============================================================================
## Nim Convenience Wrappers
## ============================================================================

proc newKryonContainer*(): KryonComponent =
  ## Create a new container component
  kryon_component_create(addr kryon_container_ops, nil)

proc newKryonText*(text: string): KryonComponent =
  ## Create a new text component
  var textState: KryonTextState
  let len = text.len
  # Use malloc instead of alloc to match C's free() in kryon_component_set_text
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  textState.text = cast[cstring](buffer)
  textState.font_id = 0
  textState.max_length = 255
  textState.word_wrap = false

  # Use malloc for state too, for consistency
  let statePtr = c_malloc(csize_t(sizeof(KryonTextState)))
  copyMem(statePtr, addr textState, sizeof(KryonTextState))

  result = kryon_component_create(addr kryon_text_ops, statePtr)

proc newKryonButton*(text: string; onClickCallback: proc (component: KryonComponent; event: ptr KryonEvent) {.cdecl.} = nil): KryonComponent =
  ## Create a new button component
  var buttonState: KryonButtonState
  let len = text.len
  # Use malloc instead of alloc to match C's free() in kryon_component_set_text
  let buffer = cast[ptr UncheckedArray[char]](c_malloc(csize_t(len + 1)))
  if len > 0:
    copyMem(addr buffer[0], unsafeAddr text[0], len)
  buffer[len] = '\0'
  buttonState.text = cast[cstring](buffer)
  buttonState.font_id = 0
  buttonState.pressed = false
  buttonState.hovered = false
  buttonState.on_click = onClickCallback

  # Use malloc for state too, for consistency
  let statePtr = c_malloc(csize_t(sizeof(KryonButtonState)))
  copyMem(statePtr, addr buttonState, sizeof(KryonButtonState))

  result = kryon_component_create(addr kryon_button_ops, statePtr)

proc newKryonCheckbox*(label: string; initialChecked: bool = false;
                      onChangeCallback: proc (checkbox: KryonComponent; checked: bool) {.cdecl.} = nil): KryonComponent =
  ## Create a new checkbox component
  let labelCStr = if label.len > 0: cstring(label) else: nil
  result = kryon_checkbox_create(labelCStr, initialChecked, 16, onChangeCallback)

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
