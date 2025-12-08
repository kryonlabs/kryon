## Kryon IR Runtime - Pure IR-based Runtime
## This is the NEW runtime that uses ONLY the IR system
## NO LEGACY CODE - everything uses IR

import ir_core, ir_desktop, ir_serialization, ir_logic, os, strutils, tables, math, parseutils, json
import style_vars

# Import reactive system for compatibility
import reactive_system
export reactive_system

# Export ir_logic for DSL access
export ir_logic

# Forward declare layout cache invalidation function (used in style setters)
proc ir_layout_invalidate_cache*(component: ptr IRComponent) {.importc, cdecl, header: "ir_core.h".}

# ============================================================================
# Runtime Types - IR-based
# ============================================================================

type
  KryonComponent* = ptr IRComponent  # Type alias for compatibility
  KryonRenderer* = DesktopIRRenderer  # Type alias for compatibility
  KryonAlignment* = IRAlignment  # Type alias for compatibility

  KryonApp* = ref object
    root*: ptr IRComponent
    renderer*: KryonRenderer  # Using IR renderer
    window*: KryonWindow
    running*: bool
    config*: DesktopRendererConfig

  KryonWindow* = ref object
    width*: int
    height*: int
    title*: string

# Alignment constants for compatibility
const
  kaStart* = IR_ALIGNMENT_START
  kaCenter* = IR_ALIGNMENT_CENTER
  kaEnd* = IR_ALIGNMENT_END
  kaStretch* = IR_ALIGNMENT_STRETCH
  # Space distribution alignment modes
  kaSpaceEvenly* = IR_ALIGNMENT_SPACE_EVENLY
  kaSpaceAround* = IR_ALIGNMENT_SPACE_AROUND
  kaSpaceBetween* = IR_ALIGNMENT_SPACE_BETWEEN

# ============================================================================
# Source Code Registry (for round-trip serialization)
# ============================================================================

var registeredSources*: Table[string, string] = initTable[string, string]()

proc registerSource*(lang: string, code: string) =
  ## Register source code for a language (for round-trip serialization)
  registeredSources[lang] = code

proc clearRegisteredSources*() =
  ## Clear all registered sources
  registeredSources.clear()

# ============================================================================
# Handler Registry (for round-trip logic block serialization)
# ============================================================================

type
  HandlerEntry* = object
    componentId*: uint32
    eventType*: string
    logicId*: string
    handlerName*: string

var registeredHandlers*: seq[HandlerEntry] = @[]

proc registerHandler*(componentId: uint32, eventType: string, logicId: string, handlerName: string) =
  ## Register a handler for round-trip serialization
  registeredHandlers.add(HandlerEntry(
    componentId: componentId,
    eventType: eventType,
    logicId: logicId,
    handlerName: handlerName
  ))

proc clearRegisteredHandlers*() =
  ## Clear all registered handlers
  registeredHandlers.setLen(0)

# ============================================================================
# Component Creation - IR-based
# ============================================================================

proc newKryonContainer*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CONTAINER)

proc newKryonButton*(text: string): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_BUTTON)
  ir_set_text_content(result, cstring(text))

proc newKryonText*(text: string): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_TEXT)
  ir_set_text_content(result, cstring(text))

proc newKryonRow*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_ROW)

proc newKryonColumn*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_COLUMN)

proc newKryonCenter*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CENTER)

proc newKryonInput*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_INPUT)

proc newKryonCheckbox*(label: string = ""): ptr IRComponent =
  result = ir_checkbox(cstring(label))

# ============================================================================
# Component Property Setters - IR-based
# ============================================================================

proc setWidth*(component: ptr IRComponent, width: int) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_width(newStyle, IR_DIMENSION_PX, cfloat(width))
    ir_set_style(component, newStyle)
  else:
    ir_set_width(style, IR_DIMENSION_PX, cfloat(width))

proc setHeight*(component: ptr IRComponent, height: int) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_height(newStyle, IR_DIMENSION_PX, cfloat(height))
    ir_set_style(component, newStyle)
  else:
    ir_set_height(style, IR_DIMENSION_PX, cfloat(height))

proc setBackgroundColor*(component: ptr IRComponent, color: uint32) =
  let style = ir_get_style(component)
  let r = uint8((color shr 24) and 0xFF)
  let g = uint8((color shr 16) and 0xFF)
  let b = uint8((color shr 8) and 0xFF)
  let a = uint8(color and 0xFF)

  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_background_color(newStyle, r, g, b, a)
    ir_set_style(component, newStyle)
  else:
    ir_set_background_color(style, r, g, b, a)

proc setTextColor*(component: ptr IRComponent, color: uint32) =
  let style = ir_get_style(component)
  let r = uint8((color shr 24) and 0xFF)
  let g = uint8((color shr 16) and 0xFF)
  let b = uint8((color shr 8) and 0xFF)
  let a = uint8(color and 0xFF)

  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_font(newStyle, 16.0, "sans-serif", r, g, b, a, false, false)
    ir_set_style(component, newStyle)
  else:
    ir_set_font(style, 16.0, "sans-serif", r, g, b, a, false, false)

proc setText*(component: ptr IRComponent, text: string) =
  ir_set_text_content(component, cstring(text))

proc setCustomData*(component: ptr IRComponent, data: string) =
  ## Store auxiliary data (e.g., input placeholder)
  ir_set_custom_data(component, cstring(data))

proc setFontSize*(component: ptr IRComponent, size: int) =
  ## Set font size for component
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_font(newStyle, cfloat(size), "sans-serif", 0, 0, 0, 255, false, false)
    ir_set_style(component, newStyle)
  else:
    # Get current font properties
    ir_set_font(style, cfloat(size), "sans-serif", 0, 0, 0, 255, false, false)

proc kryon_component_set_z_index*(component: ptr IRComponent, z: uint16) =
  ## Set z-index via IR style
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_z_index(newStyle, uint32(z))
    ir_set_style(component, newStyle)
  else:
    ir_set_z_index(style, uint32(z))

proc kryon_component_set_padding*(component: ptr IRComponent, top, right, bottom, left: uint8) =
  ## Set padding for component (space inside the component)
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_padding(newStyle, cfloat(top), cfloat(right), cfloat(bottom), cfloat(left))
    ir_set_style(component, newStyle)
  else:
    ir_set_padding(style, cfloat(top), cfloat(right), cfloat(bottom), cfloat(left))
    ir_layout_invalidate_cache(component)

proc kryon_component_set_margin*(component: ptr IRComponent, top, right, bottom, left: uint8) =
  ## Set margin for component (space outside the component)
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_margin(newStyle, cfloat(top), cfloat(right), cfloat(bottom), cfloat(left))
    ir_set_style(component, newStyle)
  else:
    ir_set_margin(style, cfloat(top), cfloat(right), cfloat(bottom), cfloat(left))
    ir_layout_invalidate_cache(component)

proc kryon_component_set_text_overflow*(component: ptr IRComponent, overflow: IRTextOverflowType) =
  ## Set text overflow behavior (clip, ellipsis, fade)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_text_overflow(newStyle, overflow)
    ir_set_style(component, newStyle)
  else:
    ir_set_text_overflow(style, overflow)

proc kryon_component_set_text_fade*(component: ptr IRComponent, fadeType: IRTextFadeType, fadeLength: float) =
  ## Set text fade effect (horizontal, vertical, radial)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_text_fade(newStyle, fadeType, cfloat(fadeLength))
    ir_set_style(component, newStyle)
  else:
    ir_set_text_fade(style, fadeType, cfloat(fadeLength))

proc kryon_component_set_text_shadow*(component: ptr IRComponent, offsetX, offsetY, blurRadius: float, r, g, b, a: uint8) =
  ## Set text shadow effect
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_text_shadow(newStyle, cfloat(offsetX), cfloat(offsetY), cfloat(blurRadius), r, g, b, a)
    ir_set_style(component, newStyle)
  else:
    ir_set_text_shadow(style, cfloat(offsetX), cfloat(offsetY), cfloat(blurRadius), r, g, b, a)

# Extended Typography (Phase 1)
proc kryon_component_set_font_weight*(component: ptr IRComponent, weight: uint16) =
  ## Set font weight (100-900, where 400=normal, 700=bold)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_font_weight(newStyle, weight)
    ir_set_style(component, newStyle)
  else:
    ir_set_font_weight(style, weight)

proc kryon_component_set_letter_spacing*(component: ptr IRComponent, spacing: cfloat) =
  ## Set letter spacing in pixels
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_letter_spacing(newStyle, spacing)
    ir_set_style(component, newStyle)
  else:
    ir_set_letter_spacing(style, spacing)

proc kryon_component_set_word_spacing*(component: ptr IRComponent, spacing: cfloat) =
  ## Set word spacing in pixels
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_word_spacing(newStyle, spacing)
    ir_set_style(component, newStyle)
  else:
    ir_set_word_spacing(style, spacing)

proc kryon_component_set_line_height*(component: ptr IRComponent, line_height: cfloat) =
  ## Set line height multiplier (e.g., 1.5 = 1.5x font size)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_line_height(newStyle, line_height)
    ir_set_style(component, newStyle)
  else:
    ir_set_line_height(style, line_height)

proc kryon_component_set_max_text_width*(component: ptr IRComponent, max_width: cfloat) =
  ## Set maximum text width for wrapping (in pixels)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_text_max_width(newStyle, IR_DIMENSION_PX, max_width)
    ir_set_style(component, newStyle)
  else:
    ir_set_text_max_width(style, IR_DIMENSION_PX, max_width)

proc kryon_component_set_text_align*(component: ptr IRComponent, align: IRTextAlign) =
  ## Set text alignment (left, center, right, justify)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_text_align(newStyle, align)
    ir_set_style(component, newStyle)
  else:
    ir_set_text_align(style, align)

proc kryon_component_set_text_decoration*(component: ptr IRComponent, decoration: uint8) =
  ## Set text decoration flags (underline, overline, line-through)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_text_decoration(newStyle, decoration)
    ir_set_style(component, newStyle)
  else:
    ir_set_text_decoration(style, decoration)

# Box Shadow (Phase 2)
proc kryon_component_set_box_shadow*(component: ptr IRComponent, offset_x, offset_y, blur_radius, spread_radius: cfloat, color: uint32, inset: bool = false) =
  ## Set box shadow with offsets, blur, spread, color, and optional inset
  if component.isNil: return
  let style = ir_get_style(component)
  let r = uint8((color shr 24) and 0xFF)
  let g = uint8((color shr 16) and 0xFF)
  let b = uint8((color shr 8) and 0xFF)
  let a = uint8(color and 0xFF)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_box_shadow(newStyle, offset_x, offset_y, blur_radius, spread_radius, r, g, b, a, inset)
    ir_set_style(component, newStyle)
  else:
    ir_set_box_shadow(style, offset_x, offset_y, blur_radius, spread_radius, r, g, b, a, inset)

# CSS Filters (Phase 3)
proc kryon_component_add_filter*(component: ptr IRComponent, filter_type: IRFilterType, value: cfloat) =
  ## Add a CSS filter to the component
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_add_filter(newStyle, filter_type, value)
    ir_set_style(component, newStyle)
  else:
    ir_add_filter(style, filter_type, value)

proc kryon_component_set_blur*(component: ptr IRComponent, blur: cfloat) =
  ## Set blur filter (value in pixels)
  kryon_component_add_filter(component, IR_FILTER_BLUR, blur)

proc kryon_component_set_brightness*(component: ptr IRComponent, brightness: cfloat) =
  ## Set brightness filter (0 = black, 1 = normal, >1 = brighter)
  kryon_component_add_filter(component, IR_FILTER_BRIGHTNESS, brightness)

proc kryon_component_set_contrast*(component: ptr IRComponent, contrast: cfloat) =
  ## Set contrast filter (0 = gray, 1 = normal, >1 = more contrast)
  kryon_component_add_filter(component, IR_FILTER_CONTRAST, contrast)

proc kryon_component_set_grayscale*(component: ptr IRComponent, grayscale: cfloat) =
  ## Set grayscale filter (0 = color, 1 = fully grayscale)
  kryon_component_add_filter(component, IR_FILTER_GRAYSCALE, grayscale)

proc kryon_component_set_hue_rotate*(component: ptr IRComponent, degrees: cfloat) =
  ## Set hue rotation filter (value in degrees)
  kryon_component_add_filter(component, IR_FILTER_HUE_ROTATE, degrees)

proc kryon_component_set_invert*(component: ptr IRComponent, invert: cfloat) =
  ## Set invert filter (0 = normal, 1 = inverted)
  kryon_component_add_filter(component, IR_FILTER_INVERT, invert)

proc kryon_component_set_filter_opacity*(component: ptr IRComponent, opacity: cfloat) =
  ## Set opacity filter (0 = transparent, 1 = opaque)
  kryon_component_add_filter(component, IR_FILTER_OPACITY, opacity)

proc kryon_component_set_saturate*(component: ptr IRComponent, saturate: cfloat) =
  ## Set saturate filter (0 = desaturated, 1 = normal, >1 = more saturated)
  kryon_component_add_filter(component, IR_FILTER_SATURATE, saturate)

proc kryon_component_set_sepia*(component: ptr IRComponent, sepia: cfloat) =
  ## Set sepia filter (0 = normal, 1 = fully sepia)
  kryon_component_add_filter(component, IR_FILTER_SEPIA, sepia)

proc kryon_component_set_opacity*(component: ptr IRComponent, opacity: float) =
  ## Set component opacity (0.0 to 1.0)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_opacity(newStyle, cfloat(opacity))
    ir_set_style(component, newStyle)
  else:
    ir_set_opacity(style, cfloat(opacity))

# ============================================================================
# Grid Layout (Phase 4)
# ============================================================================

proc kryon_component_set_grid_template_rows*(component: ptr IRComponent, tracks: seq[IRGridTrack]) =
  ## Set grid template rows from a sequence of grid tracks
  if component.isNil: return
  let layout = ir_get_layout(component)
  if not layout.isNil and tracks.len > 0:
    var trackArray = tracks
    ir_set_grid_template_rows(layout, addr trackArray[0], uint8(tracks.len))

proc kryon_component_set_grid_template_columns*(component: ptr IRComponent, tracks: seq[IRGridTrack]) =
  ## Set grid template columns from a sequence of grid tracks
  if component.isNil: return
  let layout = ir_get_layout(component)
  if not layout.isNil and tracks.len > 0:
    var trackArray = tracks
    ir_set_grid_template_columns(layout, addr trackArray[0], uint8(tracks.len))

proc kryon_component_set_grid_gap*(component: ptr IRComponent, row_gap: cfloat, column_gap: cfloat) =
  ## Set grid row and column gaps
  if component.isNil: return
  let layout = ir_get_layout(component)
  if not layout.isNil:
    ir_set_grid_gap(layout, row_gap, column_gap)

proc kryon_component_set_grid_auto_flow*(component: ptr IRComponent, row_direction: bool, dense: bool) =
  ## Set grid auto-flow direction (true = row, false = column) and dense packing
  if component.isNil: return
  let layout = ir_get_layout(component)
  if not layout.isNil:
    ir_set_grid_auto_flow(layout, row_direction, dense)

proc kryon_component_set_grid_alignment*(component: ptr IRComponent,
                                          justify_items: IRAlignment,
                                          align_items: IRAlignment,
                                          justify_content: IRAlignment,
                                          align_content: IRAlignment) =
  ## Set grid alignment properties for items and content
  if component.isNil: return
  let layout = ir_get_layout(component)
  if not layout.isNil:
    ir_set_grid_alignment(layout, justify_items, align_items, justify_content, align_content)

proc kryon_component_set_grid_item_placement*(component: ptr IRComponent,
                                               row_start: int16,
                                               row_end: int16,
                                               column_start: int16,
                                               column_end: int16) =
  ## Set explicit grid item placement (grid-row and grid-column)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_grid_item_placement(newStyle, row_start, row_end, column_start, column_end)
    ir_set_style(component, newStyle)
  else:
    ir_set_grid_item_placement(style, row_start, row_end, column_start, column_end)

proc kryon_component_set_grid_item_alignment*(component: ptr IRComponent,
                                               justify_self: IRAlignment,
                                               align_self: IRAlignment) =
  ## Set grid item self-alignment (justify-self and align-self)
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_grid_item_alignment(newStyle, justify_self, align_self)
    ir_set_style(component, newStyle)
  else:
    ir_set_grid_item_alignment(style, justify_self, align_self)

# ============================================================================
# Gradients (Phase 5)
# ============================================================================

proc kryon_create_linear_gradient*(angle: cfloat): ptr IRGradient =
  ## Create a linear gradient with specified angle in degrees
  let gradient = ir_gradient_create(IR_GRADIENT_LINEAR)
  if not gradient.isNil:
    ir_gradient_set_angle(gradient, angle)
  return gradient

proc kryon_create_radial_gradient*(center_x: cfloat = 0.5, center_y: cfloat = 0.5): ptr IRGradient =
  ## Create a radial gradient with center position (0.0-1.0)
  let gradient = ir_gradient_create(IR_GRADIENT_RADIAL)
  if not gradient.isNil:
    ir_gradient_set_center(gradient, center_x, center_y)
  return gradient

proc kryon_create_conic_gradient*(center_x: cfloat = 0.5, center_y: cfloat = 0.5): ptr IRGradient =
  ## Create a conic gradient with center position (0.0-1.0)
  let gradient = ir_gradient_create(IR_GRADIENT_CONIC)
  if not gradient.isNil:
    ir_gradient_set_center(gradient, center_x, center_y)
  return gradient

proc kryon_gradient_add_color_stop*(gradient: ptr IRGradient, position: cfloat, color: uint32) =
  ## Add a color stop to a gradient at position (0.0-1.0)
  if gradient.isNil: return
  let r = uint8((color shr 24) and 0xFF)
  let g = uint8((color shr 16) and 0xFF)
  let b = uint8((color shr 8) and 0xFF)
  let a = uint8(color and 0xFF)
  ir_gradient_add_stop(gradient, position, r, g, b, a)

proc kryon_component_set_background_gradient*(component: ptr IRComponent, gradient: ptr IRGradient) =
  ## Set background gradient for a component
  if component.isNil or gradient.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_background_gradient(newStyle, gradient)
    ir_set_style(component, newStyle)
  else:
    ir_set_background_gradient(style, gradient)

# ============================================================================
# Transitions (Phase 6)
# ============================================================================

proc kryon_create_transition*(property: IRAnimationProperty, duration: cfloat,
                               easing: IREasingType = IR_EASING_EASE_IN_OUT,
                               delay: cfloat = 0.0): ptr IRTransition =
  ## Create a CSS-style transition for a property
  let transition = ir_transition_create(property, duration)
  if not transition.isNil:
    ir_transition_set_easing(transition, easing)
    if delay > 0.0:
      ir_transition_set_delay(transition, delay)
  return transition

proc kryon_component_add_opacity_transition*(component: ptr IRComponent, duration: cfloat,
                                              easing: IREasingType = IR_EASING_EASE_IN_OUT) =
  ## Add opacity transition to component
  if component.isNil: return
  let transition = kryon_create_transition(IR_ANIM_PROP_OPACITY, duration, easing)
  if not transition.isNil:
    ir_component_add_transition(component, transition)

proc kryon_component_add_transform_transition*(component: ptr IRComponent, duration: cfloat,
                                                easing: IREasingType = IR_EASING_EASE_IN_OUT) =
  ## Add transform (translate/scale/rotate) transition to component
  if component.isNil: return
  # Add transitions for all transform properties
  var transitions = [
    kryon_create_transition(IR_ANIM_PROP_TRANSLATE_X, duration, easing),
    kryon_create_transition(IR_ANIM_PROP_TRANSLATE_Y, duration, easing),
    kryon_create_transition(IR_ANIM_PROP_SCALE_X, duration, easing),
    kryon_create_transition(IR_ANIM_PROP_SCALE_Y, duration, easing),
    kryon_create_transition(IR_ANIM_PROP_ROTATE, duration, easing)
  ]
  for t in transitions:
    if not t.isNil:
      ir_component_add_transition(component, t)

proc kryon_component_add_size_transition*(component: ptr IRComponent, duration: cfloat,
                                          easing: IREasingType = IR_EASING_EASE_IN_OUT) =
  ## Add size (width/height) transition to component
  if component.isNil: return
  var transitions = [
    kryon_create_transition(IR_ANIM_PROP_WIDTH, duration, easing),
    kryon_create_transition(IR_ANIM_PROP_HEIGHT, duration, easing)
  ]
  for t in transitions:
    if not t.isNil:
      ir_component_add_transition(component, t)

proc kryon_component_add_color_transition*(component: ptr IRComponent, duration: cfloat,
                                           easing: IREasingType = IR_EASING_EASE_IN_OUT) =
  ## Add background color transition to component
  if component.isNil: return
  let transition = kryon_create_transition(IR_ANIM_PROP_BACKGROUND_COLOR, duration, easing)
  if not transition.isNil:
    ir_component_add_transition(component, transition)

# ============================================================================
# Container Queries (Phase 7)
# ============================================================================

proc kryon_component_set_container_type*(component: ptr IRComponent, container_type: string) =
  ## Set container query type ("size", "inline-size", or "normal")
  if component.isNil: return
  let containerEnum = case container_type.toLowerAscii()
    of "size": IR_CONTAINER_TYPE_SIZE
    of "inline-size", "inlinesize": IR_CONTAINER_TYPE_INLINE_SIZE
    else: IR_CONTAINER_TYPE_NORMAL

  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_container_type(newStyle, containerEnum)
    ir_set_style(component, newStyle)
  else:
    ir_set_container_type(style, containerEnum)

proc kryon_component_set_container_name*(component: ptr IRComponent, name: string) =
  ## Set container query name for targeting
  if component.isNil: return
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_container_name(newStyle, name.cstring)
    ir_set_style(component, newStyle)
  else:
    ir_set_container_name(style, name.cstring)

proc kryon_component_add_breakpoint*(component: ptr IRComponent,
                                      breakpoint_index: uint8,
                                      min_width: float = -1.0, max_width: float = -1.0,
                                      min_height: float = -1.0, max_height: float = -1.0,
                                      width: float = -1.0, height: float = -1.0,
                                      visible: bool = true, opacity: float = -1.0) =
  ## Add breakpoint with query conditions and style overrides
  ## Use negative values to skip that condition/override
  if component.isNil: return

  var style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_style(component, newStyle)
    style = newStyle

  # Build query conditions
  var conditions: array[IR_MAX_QUERY_CONDITIONS, IRQueryCondition]
  var condCount: uint8 = 0

  # Add width conditions
  if min_width >= 0.0:
    conditions[condCount] = ir_query_min_width(cfloat(min_width))
    condCount.inc
  if max_width >= 0.0 and condCount < IR_MAX_QUERY_CONDITIONS:
    conditions[condCount] = ir_query_max_width(cfloat(max_width))
    condCount.inc

  # Add height conditions (if room)
  if min_height >= 0.0 and condCount < IR_MAX_QUERY_CONDITIONS:
    conditions[condCount] = ir_query_min_height(cfloat(min_height))
    condCount.inc
  if max_height >= 0.0 and condCount < IR_MAX_QUERY_CONDITIONS:
    conditions[condCount] = ir_query_max_height(cfloat(max_height))
    condCount.inc

  # Add breakpoint if we have at least one condition
  if condCount > 0:
    ir_add_breakpoint(style, addr conditions[0], condCount)

    # Use the provided breakpoint_index to set overrides
    # The IR layer will have added it at this index
    let bpIndex = breakpoint_index

    # Set style overrides
    if width >= 0.0:
      ir_breakpoint_set_width(style, bpIndex, IR_DIMENSION_PX, cfloat(width))
    if height >= 0.0:
      ir_breakpoint_set_height(style, bpIndex, IR_DIMENSION_PX, cfloat(height))
    if opacity >= 0.0:
      ir_breakpoint_set_opacity(style, bpIndex, cfloat(opacity))
    ir_breakpoint_set_visible(style, bpIndex, visible)

# ============================================================================
# Animations (Phase 8)
# ============================================================================

proc kryon_animation_pulse*(duration: cfloat, iterations: int32 = -1): ptr IRAnimation =
  ## Create pulse animation (scale in/out effect)
  ## iterations: -1 = infinite, 1 = once, etc.
  let anim = ir_animation_pulse(duration)
  if not anim.isNil:
    ir_animation_set_iterations(anim, iterations)
  return anim

proc kryon_animation_fade_in_out*(duration: cfloat, iterations: int32 = 1): ptr IRAnimation =
  ## Create fade in/out animation (opacity 0 -> 1 -> 0)
  ## iterations: -1 = infinite, 1 = once, etc.
  let anim = ir_animation_fade_in_out(duration)
  if not anim.isNil:
    ir_animation_set_iterations(anim, iterations)
  return anim

proc kryon_animation_slide_in_left*(duration: cfloat): ptr IRAnimation =
  ## Create slide in from left animation
  let anim = ir_animation_slide_in_left(duration)
  return anim

proc kryon_component_add_animation*(component: ptr IRComponent, animation: ptr IRAnimation) =
  ## Attach animation to component
  if component.isNil or animation.isNil: return
  ir_component_add_animation(component, animation)

proc kryon_apply_animation_from_string*(component: ptr IRComponent, animSpec: string) =
  ## Parse and apply animation from string specification at runtime
  ## Format: "functionName(param1, param2, ...)"
  ## Examples:
  ##   "pulse(2.0, -1)" -> pulse animation, 2.0s duration, infinite iterations
  ##   "fadeInOut(3.0, -1)" -> fade in/out, 3.0s duration, infinite iterations
  ##   "slideInLeft(2.0)" -> slide in from left, 2.0s duration

  if component.isNil or animSpec.len == 0:
    return

  # Find function name and parameters
  let parenPos = animSpec.find('(')
  if parenPos < 0:
    echo "Warning: Invalid animation spec (missing parenthesis): ", animSpec
    return

  let funcName = animSpec[0..<parenPos].strip()
  let paramsEnd = animSpec.rfind(')')
  if paramsEnd < 0:
    echo "Warning: Invalid animation spec (missing closing parenthesis): ", animSpec
    return

  let paramsStr = animSpec[parenPos+1..<paramsEnd].strip()
  var params: seq[string] = @[]
  if paramsStr.len > 0:
    params = paramsStr.split(',')
    for i in 0..<params.len:
      params[i] = params[i].strip()

  # Create animation based on function name
  var anim: ptr IRAnimation = nil

  case funcName.toLowerAscii()
  of "pulse":
    if params.len >= 1:
      let duration = parseFloat(params[0])
      let iterations = if params.len >= 2: parseInt(params[1]) else: -1
      anim = kryon_animation_pulse(cfloat(duration), int32(iterations))
    else:
      echo "Warning: pulse() requires at least 1 parameter (duration)"

  of "fadeinout", "fade_in_out":
    if params.len >= 1:
      let duration = parseFloat(params[0])
      let iterations = if params.len >= 2: parseInt(params[1]) else: 1
      anim = kryon_animation_fade_in_out(cfloat(duration), int32(iterations))
    else:
      echo "Warning: fadeInOut() requires at least 1 parameter (duration)"

  of "slideinleft", "slide_in_left":
    if params.len >= 1:
      let duration = parseFloat(params[0])
      anim = kryon_animation_slide_in_left(cfloat(duration))
    else:
      echo "Warning: slideInLeft() requires at least 1 parameter (duration)"

  else:
    echo "Warning: Unknown animation function: ", funcName
    return

  # Apply animation to component
  if not anim.isNil:
    kryon_component_add_animation(component, anim)

# ============================================================================
# Application Runtime - IR-based
# ============================================================================

proc newKryonApp*(): KryonApp =
  ## Create new Kryon application (IR-based)
  result = KryonApp()
  result.running = false

proc setWindow*(app: KryonApp, window: KryonWindow) =
  ## Set the window configuration for the app
  app.window = window

proc setRenderer*(app: KryonApp, renderer: KryonRenderer) =
  ## Set the renderer for the app (not used in IR architecture)
  app.renderer = renderer

proc setRoot*(app: KryonApp, root: ptr IRComponent) =
  ## Set the root component for the app
  app.root = root
  ir_set_root(root)  # Update C context too - needed for visibility checks

proc initRenderer*(width, height: int; title: string): KryonRenderer =
  ## Initialize IR desktop renderer with window properties
  echo "Initializing IR desktop renderer..."
  echo "  Window: ", width, "x", height
  echo "  Title: ", title

  # Create renderer config
  var config = desktop_renderer_config_sdl3(int32(width), int32(height), cstring(title))
  config.resizable = true
  config.vsync_enabled = true
  config.target_fps = 60

  # Note: Renderer will be created during run()
  # For now, return nil as placeholder (not used in IR architecture)
  result = nil

proc addWindowPropertiesToKir*(filename: string, window: KryonWindow) =
  ## Post-process a serialized KIR file to add window properties
  try:
    let content = readFile(filename)
    var kirJson = parseJson(content)
    # Add window properties at root level
    kirJson["windowTitle"] = %window.title
    kirJson["windowWidth"] = %($window.width & ".0px")
    kirJson["windowHeight"] = %($window.height & ".0px")
    writeFile(filename, pretty(kirJson))
  except:
    echo "[IR] Warning: Failed to add window properties to KIR file"

proc run*(app: KryonApp) =
  ## Run the IR-based Kryon application
  if app.root.isNil:
    echo "ERROR: No root component set"
    quit(1)

  let isDebug = getEnv("KRYON_DEBUG") != ""

  if isDebug:
    echo "Running Kryon IR application..."

  # Create renderer config if not set
  if app.window.isNil:
    app.window = KryonWindow(width: 800, height: 600, title: "Kryon App")

  app.config = desktop_renderer_config_sdl3(
    int32(app.window.width),
    int32(app.window.height),
    cstring(app.window.title)
  )
  app.config.resizable = true
  app.config.vsync_enabled = true
  app.config.target_fps = 60

  if isDebug:
    echo "  Window: ", app.window.width, "x", app.window.height
    echo "  Title: ", app.window.title

  # Debug: Print component tree if KRYON_DEBUG_TREE is set
  if getEnv("KRYON_DEBUG_TREE") != "":
    echo "\n=== DEBUG: Initial Component Tree ==="
    debug_print_tree(app.root)
    echo "=== END DEBUG ===\n"

  # Serialize to IR file if KRYON_SERIALIZE_IR is set
  let serializeTarget = getEnv("KRYON_SERIALIZE_IR")
  if serializeTarget != "":
    echo "Serializing IR to: ", serializeTarget
    # Evaluate all reactive conditionals before serialization
    # This ensures conditional children are in the tree when we serialize
    updateAllReactiveConditionals()
    # Export reactive manifest and sync component definitions
    var manifest = exportReactiveManifest()
    if manifest == nil:
      # Create a manifest if none exists (we need it for sources)
      manifest = ir_reactive_manifest_create()
    if manifest != nil:
      syncComponentDefinitionsToManifest(manifest)
      # Add registered sources to manifest (for round-trip preservation)
      for lang, code in registeredSources.pairs:
        ir_reactive_manifest_add_source(manifest, cstring(lang), cstring(code))

    # Use globalLogicBlock which has proper universal logic functions
    # (populated by registerLogicFunctionWithSource and registerEventBinding)
    var logicBlock: IRLogicBlockPtr = nil
    let glb = getGlobalLogicBlock()
    if glb.handle != nil and (glb.functionCount > 0 or glb.bindingCount > 0):
      logicBlock = glb.handle
      echo "[kryon] Using global logic block with ", glb.functionCount, " functions, ", glb.bindingCount, " bindings"

    # Write JSON v3.0 if we have handlers, otherwise v2.1
    if logicBlock != nil:
      if ir_write_json_v3_file(app.root, manifest, logicBlock, cstring(serializeTarget)):
        addWindowPropertiesToKir(serializeTarget, app.window)
        echo "✓ IR serialized successfully (JSON v3.0 with logic block, ", manifest.source_count, " sources)"
        ir_logic_block_free(logicBlock)
        ir_reactive_manifest_destroy(manifest)
        quit(0)
      else:
        echo "✗ Failed to serialize IR"
        ir_logic_block_free(logicBlock)
        ir_reactive_manifest_destroy(manifest)
        quit(1)
    elif manifest != nil and (manifest.variable_count > 0 or manifest.component_def_count > 0 or
                            manifest.conditional_count > 0 or manifest.for_loop_count > 0 or
                            manifest.source_count > 0):
      if ir_write_json_v2_with_manifest_file(app.root, manifest, cstring(serializeTarget)):
        addWindowPropertiesToKir(serializeTarget, app.window)
        echo "✓ IR serialized successfully (JSON v2.1 with ", manifest.component_def_count, " components, ", manifest.variable_count, " reactive vars, ", manifest.source_count, " sources)"
        ir_reactive_manifest_destroy(manifest)
        quit(0)
      else:
        echo "✗ Failed to serialize IR"
        ir_reactive_manifest_destroy(manifest)
        quit(1)
    else:
      # Fallback to v2.0 (no reactive state)
      if manifest != nil:
        ir_reactive_manifest_destroy(manifest)
      if ir_write_json_file(app.root, cstring(serializeTarget)):
        addWindowPropertiesToKir(serializeTarget, app.window)
        echo "✓ IR serialized successfully (JSON v2.0)"
        quit(0)
      else:
        echo "✗ Failed to serialize IR"
        quit(1)

  # Auto-save IR files to build/ir/ directory
  let exeName = extractFilename(getAppFilename()).changeFileExt("")
  let irDir = "build/ir"
  if not dirExists(irDir):
    createDir(irDir)

  let kirPath = irDir / exeName & ".kir"

  # Evaluate all reactive conditionals before serialization
  # This ensures conditional children are in the tree when we serialize
  updateAllReactiveConditionals()

  # Check if reactive manifest exists (from kryonComponent macros)
  let manifest = exportReactiveManifest()
  if manifest != nil:
    # Sync component definitions from Nim registry to C manifest
    syncComponentDefinitionsToManifest(manifest)

  # Get the global logic block (may be empty)
  let logicBlock = getGlobalLogicBlock()

  # Check if we have logic block with handlers (always use v3 for handlers)
  let hasLogic = logicBlock.handle != nil and logicBlock.functionCount > 0
  let hasReactiveState = manifest != nil and (manifest.variable_count > 0 or manifest.component_def_count > 0 or
                          manifest.conditional_count > 0 or manifest.for_loop_count > 0)

  if hasLogic:
    # Use JSON v3.0 with logic block (includes handlers for .kir execution)
    if ir_write_json_v3_file(app.root, manifest, logicBlock.handle, cstring(kirPath)):
      addWindowPropertiesToKir(kirPath, app.window)
      echo "[IR] Saved JSON v3.0 IR with logic block (", logicBlock.functionCount, " handlers): ", kirPath
    else:
      echo "[IR] Warning: Failed to save JSON v3.0 IR"
    if manifest != nil:
      ir_reactive_manifest_destroy(manifest)
  elif hasReactiveState:
    # Save JSON v2.1 with reactive manifest and component definitions
    if ir_write_json_v2_with_manifest_file(app.root, manifest, cstring(kirPath)):
      addWindowPropertiesToKir(kirPath, app.window)
      echo "[IR] Saved JSON v2.1 IR with ", manifest.component_def_count, " components, ", manifest.variable_count, " reactive variables: ", kirPath
    else:
      echo "[IR] Warning: Failed to save JSON v2.1 IR"
    ir_reactive_manifest_destroy(manifest)
  else:
    # Fallback to v2.0 (no reactive state, no handlers)
    if manifest != nil:
      ir_reactive_manifest_destroy(manifest)
    if ir_write_json_file(app.root, cstring(kirPath)):
      addWindowPropertiesToKir(kirPath, app.window)
      echo "[IR] Saved JSON v2.0 IR: ", kirPath
    else:
      echo "[IR] Warning: Failed to save JSON IR"

  # Render using IR desktop renderer
  if not desktop_render_ir_component(app.root, addr app.config):
    echo "ERROR: Failed to render IR component"
    quit(1)

  echo "Application closed"

# ============================================================================
# ============================================================================
# Helper Functions for DSL Compatibility
# ============================================================================

proc parseColor*(colorStr: string): uint32 =
  ## Parse hex color string or named color; supports #RGB, #RRGGBB, optional alpha
  if colorStr.len == 0:
    return 0xFFFFFFFF'u32

  # Named colors fall back to helpers.parseNamedColor if no '#'
  if colorStr[0] != '#':
    let c = ir_color_named(cstring(colorStr))
    return (uint32(c.r) shl 24) or (uint32(c.g) shl 16) or (uint32(c.b) shl 8) or uint32(c.a)

  var hex = colorStr[1..^1]

  # Expand short forms
  if hex.len == 3: # RGB
    hex = "" & hex[0] & hex[0] & hex[1] & hex[1] & hex[2] & hex[2] & "FF"
  elif hex.len == 4: # RGBA
    hex = "" & hex[0] & hex[0] & hex[1] & hex[1] & hex[2] & hex[2] & hex[3] & hex[3]
  elif hex.len == 6: # RRGGBB
    hex = hex & "FF"

  if hex.len != 8:
    return 0xFFFFFFFF'u32

  try:
    let r = fromHex[uint8](hex[0..1])
    let g = fromHex[uint8](hex[2..3])
    let b = fromHex[uint8](hex[4..5])
    let a = fromHex[uint8](hex[6..7])
    result = (uint32(r) shl 24) or (uint32(g) shl 16) or (uint32(b) shl 8) or uint32(a)
  except:
    result = 0xFFFFFFFF'u32

proc parseColor*(color: uint32): uint32 =
  ## Passthrough overload for already-parsed colors
  color

# IR uses floats directly - toFixed is for DSL compatibility with old fixed-point system
proc toFixed*(value: int): cfloat {.inline.} = cfloat(value)
proc toFixed*(value: float): cfloat {.inline.} = cfloat(value)

# ============================================================================
# Compatibility Layer - Map old API to IR
# ============================================================================

# Component tree management (compatibility with old API)
proc kryon_component_add_child*(parent, child: ptr IRComponent): bool =
  ir_add_child(parent, child)
  return true  # IR always succeeds unless nil pointers

# Component property setters (compatibility with old API)
proc kryon_component_set_background_color*(component: ptr IRComponent, color: uint32) =
  setBackgroundColor(component, color)

proc kryon_component_set_text_color*(component: ptr IRComponent, color: uint32) =
  setTextColor(component, color)

# Overloads for IRColor (supports style variable references)
proc kryon_component_set_background_color*(component: ptr IRComponent, color: IRColor) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    if color.`type` == IR_COLOR_VAR_REF:
      ir_set_background_color_var(newStyle, color.var_id)
    else:
      ir_set_background_color(newStyle, color.r, color.g, color.b, color.a)
    ir_set_style(component, newStyle)
  else:
    if color.`type` == IR_COLOR_VAR_REF:
      ir_set_background_color_var(style, color.var_id)
    else:
      ir_set_background_color(style, color.r, color.g, color.b, color.a)

proc kryon_component_set_text_color*(component: ptr IRComponent, color: IRColor) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    if color.`type` == IR_COLOR_VAR_REF:
      ir_set_text_color_var(newStyle, color.var_id)
    else:
      ir_set_font(newStyle, 16.0, nil, color.r, color.g, color.b, color.a, false, false)
    ir_set_style(component, newStyle)
  else:
    if color.`type` == IR_COLOR_VAR_REF:
      ir_set_text_color_var(style, color.var_id)
    else:
      ir_set_font(style, 16.0, nil, color.r, color.g, color.b, color.a, false, false)

proc kryon_component_set_border_color*(component: ptr IRComponent, color: IRColor) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    if color.`type` == IR_COLOR_VAR_REF:
      ir_set_border_color_var(newStyle, color.var_id)
    else:
      ir_set_border(newStyle, 1.0, color.r, color.g, color.b, color.a, 0.0)
    ir_set_style(component, newStyle)
  else:
    if color.`type` == IR_COLOR_VAR_REF:
      ir_set_border_color_var(style, color.var_id)
    else:
      ir_set_border(style, 1.0, color.r, color.g, color.b, color.a, 0.0)

proc kryon_component_set_bounds*(component: ptr IRComponent, x, y, w, h: int) =
  let style = ir_get_style(component)
  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_width(newStyle, IR_DIMENSION_PX, cfloat(w))
    ir_set_height(newStyle, IR_DIMENSION_PX, cfloat(h))
    ir_set_style(component, newStyle)
  else:
    ir_set_width(style, IR_DIMENSION_PX, cfloat(w))
    ir_set_height(style, IR_DIMENSION_PX, cfloat(h))

# Canvas/Spacer - actual IR components
proc newKryonCanvas*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CANVAS)

proc newKryonSpacer*(): ptr IRComponent =
  result = ir_create_component(IR_COMPONENT_CONTAINER)

# Forward declaration for dropdown handler registration
proc registerDropdownHandler*(component: ptr IRComponent, handler: proc(index: int))

# Dropdown Handler System - declare before use
var dropdownHandlers = initTable[uint32, proc(index: int)]()

# Dropdown - full IR implementation
proc newKryonDropdown*(
  placeholder: string = "Select...",
  options: seq[string] = @[],
  selectedIndex: int = -1,
  fontSize: uint8 = 14,
  textColor: uint32 = 0xFF000000'u32,
  backgroundColor: uint32 = 0xFFFFFFFF'u32,
  borderColor: uint32 = 0xFFD1D5DB'u32,
  borderWidth: uint8 = 1,
  hoverColor: uint32 = 0xFFE0E0E0'u32,
  width: int = 0,  # Added for DSL support
  height: int = 0,  # Added for DSL support
  onChangeCallback: proc(index: int) = nil
): ptr IRComponent =
  # Convert Nim seq[string] to C string array
  var cOptions: seq[cstring] = @[]
  for opt in options:
    cOptions.add(cstring(opt))

  # Create the dropdown component using IR core function
  result = if cOptions.len > 0:
    ir_dropdown(cstring(placeholder), addr cOptions[0], uint32(cOptions.len))
  else:
    ir_dropdown(cstring(placeholder), nil, 0)

  # Set initial selected index if valid
  if selectedIndex >= 0 and selectedIndex < options.len:
    ir_set_dropdown_selected_index(result, int32(selectedIndex))

  # Create and apply style
  let style = ir_create_style()

  # Set dimensions if specified
  if width > 0:
    ir_set_width(style, IR_DIMENSION_PX, cfloat(width))
  if height > 0:
    ir_set_height(style, IR_DIMENSION_PX, cfloat(height))

  # Apply colors
  let textR = uint8((textColor shr 24) and 0xFF)
  let textG = uint8((textColor shr 16) and 0xFF)
  let textB = uint8((textColor shr 8) and 0xFF)
  let textA = uint8(textColor and 0xFF)

  let bgR = uint8((backgroundColor shr 24) and 0xFF)
  let bgG = uint8((backgroundColor shr 16) and 0xFF)
  let bgB = uint8((backgroundColor shr 8) and 0xFF)
  let bgA = uint8(backgroundColor and 0xFF)

  let borderR = uint8((borderColor shr 24) and 0xFF)
  let borderG = uint8((borderColor shr 16) and 0xFF)
  let borderB = uint8((borderColor shr 8) and 0xFF)
  let borderA = uint8(borderColor and 0xFF)

  ir_set_font(style, cfloat(fontSize), nil, textR, textG, textB, textA, false, false)
  ir_set_background_color(style, bgR, bgG, bgB, bgA)
  ir_set_border(style, cfloat(borderWidth), borderR, borderG, borderB, borderA, 4)

  ir_set_style(result, style)

  # Always register dropdown event (for open/close interaction)
  # even if there's no callback - the renderer needs this to detect clicks
  let event = ir_create_event(IR_EVENT_CLICK, cstring("nim_dropdown_" & $result.id), nil)
  ir_add_event(result, event)

  # Register onChange handler if provided
  if onChangeCallback != nil:
    dropdownHandlers[result.id] = onChangeCallback

# Legacy compatibility for markdown and other modules
type
  KryonCmdBuf* = pointer

proc kryon_component_set_layout_direction*(component: ptr IRComponent, direction: int) =
  ## Set layout direction: 0=column, 1=row
  if component.isNil: return
  var layout = ir_get_layout(component)
  if layout.isNil:
    layout = ir_create_layout()
    ir_set_layout(component, layout)
  ir_set_flex_properties(layout, layout.flex.grow, layout.flex.shrink, uint8(direction))

proc kryon_component_set_layout_alignment*(component: ptr IRComponent, justify: KryonAlignment, align: KryonAlignment) =
  # Layout alignment for containers - create or get layout and set alignment
  var layout = ir_get_layout(component)
  if layout.isNil:
    layout = ir_create_layout()
    ir_set_layout(component, layout)
  ir_set_justify_content(layout, justify)
  ir_set_align_items(layout, align)

proc kryon_component_set_gap*(component: ptr IRComponent, gap: uint8) =
  # Gap is part of flexbox layout
  var layout = ir_get_layout(component)
  if layout.isNil:
    layout = ir_create_layout()
    ir_set_layout(component, layout)
  ir_set_gap(layout, uint32(gap))

proc kryon_component_set_bounds_mask*(component: ptr IRComponent, x, y, w, h: cfloat, mask: uint8) =
  # Set component bounds using IR style system
  # Mask bits: 0x01 = X, 0x02 = Y, 0x04 = width, 0x08 = height
  # If X or Y are set, this is ABSOLUTE positioning
  let style = ir_get_style(component)
  let targetStyle = if style.isNil:
    let newStyle = ir_create_style()
    ir_set_style(component, newStyle)
    newStyle
  else:
    style

  # Check if absolute positioning is being set (X or Y specified)
  let hasAbsoluteX = (mask and 0x01) != 0
  let hasAbsoluteY = (mask and 0x02) != 0

  if hasAbsoluteX or hasAbsoluteY:
    # Enable absolute positioning mode
    targetStyle.position_mode = IR_POSITION_ABSOLUTE

    if hasAbsoluteX:
      targetStyle.absolute_x = x

    if hasAbsoluteY:
      targetStyle.absolute_y = y

    echo "[IR] Set absolute positioning: x=", x, ", y=", y, " (mask=0x", mask.toHex, ")"

  # Only set width if mask bit 0x04 is set
  if (mask and 0x04) != 0:
    ir_set_width(targetStyle, IR_DIMENSION_PX, w)

  # Only set height if mask bit 0x08 is set
  if (mask and 0x08) != 0:
    ir_set_height(targetStyle, IR_DIMENSION_PX, h)

proc kryon_component_set_bounds_with_types*(component: ptr IRComponent, x, y, w, h: cfloat,
                                            widthType, heightType: IRDimensionType, mask: uint8) =
  ## Set component bounds with explicit dimension types for width and height
  ## This allows percentage-based sizing (e.g., width = 50.percent)
  ## Mask bits: 0x01 = X, 0x02 = Y, 0x04 = width, 0x08 = height
  let style = ir_get_style(component)
  let targetStyle = if style.isNil:
    let newStyle = ir_create_style()
    ir_set_style(component, newStyle)
    newStyle
  else:
    style

  # Check if absolute positioning is being set (X or Y specified)
  let hasAbsoluteX = (mask and 0x01) != 0
  let hasAbsoluteY = (mask and 0x02) != 0

  if hasAbsoluteX or hasAbsoluteY:
    # Enable absolute positioning mode
    targetStyle.position_mode = IR_POSITION_ABSOLUTE

    if hasAbsoluteX:
      targetStyle.absolute_x = x

    if hasAbsoluteY:
      targetStyle.absolute_y = y

  # Only set width if mask bit 0x04 is set
  if (mask and 0x04) != 0:
    ir_set_width(targetStyle, widthType, w)

  # Only set height if mask bit 0x08 is set
  if (mask and 0x08) != 0:
    ir_set_height(targetStyle, heightType, h)

proc kryon_component_set_flex*(component: ptr IRComponent, flexGrow, flexShrink: uint8) =
  ## Set flex grow and shrink properties on the IR component
  if component.isNil: return
  var layout = ir_get_layout(component)
  if layout.isNil:
    layout = ir_create_layout()
    ir_set_layout(component, layout)
  ir_set_flex_properties(layout, flexGrow, flexShrink, layout.flex.direction)

proc kryon_component_mark_dirty*(component: ptr IRComponent) =
  # Use the C function which properly invalidates cache and marks layout dirty
  ir_layout_invalidate_cache(component)

# ============================================================================
# Nim-to-C Event Bridge - Actual Implementation
# ============================================================================

# Button event handling using IR event system
var buttonHandlers = initTable[uint32, proc()]()

proc nimButtonBridge*(componentId: uint32) {.exportc: "nimButtonBridge", cdecl, dynlib.} =
  ## Bridge function for button clicks - called from C IR event system
  ## This is exported to C so it can be called when buttons are clicked
  if buttonHandlers.hasKey(componentId):
    buttonHandlers[componentId]()

proc registerButtonHandler*(component: ptr IRComponent, handler: proc()) =
  ## Register a button click handler using IR event system
  ## Replaces any existing handler for this component
  if component.isNil:
    return

  # Check if event needs to be created (before we update the handler table)
  let needsEvent = not buttonHandlers.hasKey(component.id) or component.events == nil

  # Replace existing handler (don't accumulate)
  buttonHandlers[component.id] = handler

  # Only create event if not already registered
  # The event persists on the component, we just update the Nim handler
  if needsEvent:
    let event = ir_create_event(IR_EVENT_CLICK, cstring("nim_button_" & $component.id), nil)
    ir_add_event(component, event)

# Checkbox Handler System
var checkboxHandlers = initTable[uint32, proc()]()

proc nimCheckboxBridge*(componentId: uint32) {.exportc: "nimCheckboxBridge", cdecl, dynlib.} =
  ## Bridge function for checkbox clicks - called from C IR event system
  if checkboxHandlers.hasKey(componentId):
    checkboxHandlers[componentId]()

proc registerCheckboxHandler*(component: ptr IRComponent, handler: proc()) =
  ## Register a checkbox click handler using IR event system
  if component.isNil:
    return

  checkboxHandlers[component.id] = handler

  # Create IR event and attach to component (use component ID for default logic_id)
  let event = ir_create_event(IR_EVENT_CLICK, cstring("nim_checkbox_" & $component.id), nil)
  ir_add_event(component, event)

proc registerCheckboxHandlerWithLogicId*(component: ptr IRComponent, handler: proc(), logicId: string) =
  ## Register a checkbox click handler with a specific logic_id for IR serialization
  if component.isNil:
    return

  checkboxHandlers[component.id] = handler

  # Create IR event with the specified logic_id
  let event = ir_create_event(IR_EVENT_CLICK, cstring(logicId), nil)
  ir_add_event(component, event)

proc setCheckboxState*(component: ptr IRComponent, checked: bool) =
  ## Set checkbox checked state using custom_data
  if component.isNil:
    return
  ir_set_custom_data(component, if checked: cstring("checked") else: cstring("unchecked"))

# Dropdown Handler System (variable declared at top of file)
proc nimDropdownBridge*(componentId: uint32, selectedIndex: int32) {.exportc: "nimDropdownBridge", cdecl, dynlib.} =
  ## Bridge function for dropdown selection changes - called from C IR event system
  if dropdownHandlers.hasKey(componentId):
    dropdownHandlers[componentId](int(selectedIndex))

proc registerDropdownHandler*(component: ptr IRComponent, handler: proc(index: int)) =
  ## Register a dropdown change handler
  ## Note: The event is already registered in newKryonDropdown, this just adds the callback
  if component.isNil:
    return
  dropdownHandlers[component.id] = handler

proc kryon_component_set_border_color*(component: ptr IRComponent, color: uint32) =
  let style = ir_get_style(component)
  let r = uint8((color shr 24) and 0xFF)
  let g = uint8((color shr 16) and 0xFF)
  let b = uint8((color shr 8) and 0xFF)
  let a = uint8(color and 0xFF)

  if style.isNil:
    let newStyle = ir_create_style()
    ir_set_border(newStyle, 1.0, r, g, b, a, 0)
    ir_set_style(component, newStyle)
  else:
    ir_set_border(style, 1.0, r, g, b, a, 0)

proc kryon_component_set_border_width*(component: ptr IRComponent, width: uint32) =
  let style = ir_get_style(component)

  if style.isNil:
    let newStyle = ir_create_style()
    # No existing border color, use default black
    ir_set_border(newStyle, cfloat(width), 0, 0, 0, 255, 0)
    ir_set_style(component, newStyle)
  else:
    # Preserve existing border color when updating width
    ir_set_border(style, cfloat(width), style.border.color.r, style.border.color.g, style.border.color.b, style.border.color.a, cfloat(style.border.radius))

proc kryon_component_set_border_radius*(component: ptr IRComponent, radius: uint8) =
  let style = ir_get_style(component)

  if style.isNil:
    let newStyle = ir_create_style()
    # No existing border, use default width and color
    ir_set_border(newStyle, 1.0, 0, 0, 0, 255, cfloat(radius))
    ir_set_style(component, newStyle)
  else:
    # Preserve existing border color and width when updating radius
    ir_set_border(style, style.border.width, style.border.color.r, style.border.color.g, style.border.color.b, style.border.color.a, cfloat(radius))

proc kryon_button_set_center_text*(button: ptr IRComponent, centered: bool) =
  # Text centering is handled automatically in button rendering
  discard

proc kryon_button_set_ellipsize*(button: ptr IRComponent, ellipsize: bool) =
  # Ellipsize not yet implemented in IR - would need font metrics
  discard

# Tab/Checkbox/Input stubs (not yet implemented in IR)
type
  TabVisualState* = object  # Tab visual state for Nim DSL
    backgroundColor*: uint32
    activeBackgroundColor*: uint32
    textColor*: uint32
    activeTextColor*: uint32

  CheckboxState* = ref object  # Stub
  InputState* = ref object  # Stub

# ============================================================================
# TabGroup State Registry
# ============================================================================

# Store TabGroup states by TabGroup component pointer for runtime access
var tabGroupStates = initTable[pointer, ptr TabGroupState]()

proc registerTabGroupState*(group: ptr IRComponent, state: ptr TabGroupState) =
  ## Register a TabGroup state for runtime access by reactive system
  tabGroupStates[cast[pointer](group)] = state

proc getTabGroupState*(group: ptr IRComponent): ptr TabGroupState =
  ## Get a TabGroup state by its component pointer
  tabGroupStates.getOrDefault(cast[pointer](group), nil)

# Forward declaration
proc resyncTabGroupChildren*(state: ptr TabGroupState)

proc handleReactiveForLoopParentChanged(parent: ptr IRComponent) =
  ## Called by reactive system when a for-loop's parent children change
  ## Checks if parent is a TabBar or TabContent and resyncs the TabGroup
  if parent == nil: return

  # Walk up to find potential TabGroup parent
  var current = parent.parent
  while current != nil:
    let state = getTabGroupState(current)
    if state != nil:
      # We found a TabGroup - resync if parent is inside it
      # (We can't check specific pointers since TabGroupState is incomplete,
      #  so we resync on any child change within a TabGroup)
      echo "[ReactiveForLoop] Resyncing TabGroup after children changed"
      resyncTabGroupChildren(state)
      break  # Found it, no need to continue
    current = current.parent

# Register the hook with reactive system
onReactiveForLoopParentChanged = handleReactiveForLoopParentChanged

# ============================================================================
# TabGroup Core Functions
# ============================================================================

proc createTabGroupState*(group, tabBar, tabContent: ptr IRComponent, selectedIndex: int, reorderable: bool): ptr TabGroupState =
  ## Create tab group state - delegates entirely to C
  result = ir_tabgroup_create_state(group, tabBar, tabContent, cint(selectedIndex), reorderable)

proc registerTabBar*(tabBar: ptr IRComponent, state: ptr TabGroupState, reorderable: bool) =
  ## Register tab bar - delegates to C
  if state.isNil: return
  ir_tabgroup_set_reorderable(state, reorderable)
  ir_tabgroup_register_bar(state, tabBar)

proc registerTabComponent*(tab: ptr IRComponent, state: ptr TabGroupState, visual: TabVisualState, index: int) =
  ## Register a tab component with the tab group
  ## C handles tab selection; user's onClick is in buttonHandlers (set by DSL)
  if state.isNil: return

  # Check if this tab is already registered via C
  let existingCount = ir_tabgroup_get_tab_count(state)
  for i in 0..<existingCount:
    if ir_tabgroup_get_tab(state, i) == tab:
      echo "[kryon][tabs] tab id=", tab.id, " already registered"
      return

  # Register tab with C (adds to tabs array)
  ir_tabgroup_register_tab(state, tab)
  let tabIndex = ir_tabgroup_get_tab_count(state) - 1
  echo "[kryon][tabs] register tab id=", tab.id, " index=", tabIndex

  # Store visual state in C core for automatic application
  let cVisual = CTabVisualState(
    background_color: visual.backgroundColor,
    active_background_color: visual.activeBackgroundColor,
    text_color: visual.textColor,
    active_text_color: visual.activeTextColor
  )
  ir_tabgroup_set_tab_visual(state, cint(tabIndex), cVisual)

  # User's onClick handler is already in buttonHandlers (set by DSL)
  # C renderer calls nimButtonBridge after ir_tabgroup_handle_tab_click
  # No duplicate tracking needed - tabs are buttons

proc registerTabContent*(content: ptr IRComponent, state: ptr TabGroupState) =
  ## Register tab content container - delegates to C
  if state.isNil: return
  echo "[kryon][tabs] register content id=", content.id
  ir_tabgroup_register_content(state, content)

proc registerTabPanel*(panel: ptr IRComponent, state: ptr TabGroupState, index: int) =
  ## Register a tab panel - delegates to C
  if state.isNil: return
  echo "[kryon][tabs] register panel id=", panel.id, " idx=", index
  ir_tabgroup_register_panel(state, panel)

proc finalizeTabGroup*(state: ptr TabGroupState) =
  ## Finalize tab group - delegates to C
  if state.isNil: return
  echo "[kryon][tabs] finalize state=", cast[int](state)
  ir_tabgroup_finalize(state)
  # C handles initial visual application via ir_tabgroup_apply_visuals

proc resyncTabGroupChildren*(state: ptr TabGroupState) =
  ## Re-synchronize TabGroup state with current TabBar/TabContent children
  ## Called when a reactive for-loop rebuilds tabs or panels
  if state.isNil: return

  echo "[TabGroup] Resyncing children..."

  # Since TabGroupState is incomplete, we can't directly access fields
  # The C functions handle the internal state management
  # We just need to call finalize again to re-count tabs/panels
  ir_tabgroup_finalize(state)

  echo "[TabGroup] Resync complete"

var canvasHandlers = initTable[uint32, proc()]()

proc registerCanvasHandler*(canvas: ptr IRComponent, handler: proc()) =
  ## Register a canvas onDraw handler - called each frame when canvas is rendered
  if canvas.isNil:
    return
  canvasHandlers[canvas.id] = handler

proc nimCanvasBridge*(componentId: uint32) {.exportc: "nimCanvasBridge", cdecl, dynlib.} =
  ## Bridge function for canvas rendering - called from C desktop renderer
  ## This is exported to C so it can be called when canvas components are rendered
  if canvasHandlers.hasKey(componentId):
    canvasHandlers[componentId]()

var inputHandlers = initTable[uint32, proc(text: string)]()

proc nimInputBridge*(component: ptr IRComponent, text: cstring): bool {.exportc: "nimInputBridge", cdecl, dynlib.} =
  ## Bridge from C: notify Nim input change
  if component.isNil:
    return false
  if inputHandlers.hasKey(component.id):
    let s = if text != nil: $text else: ""
    inputHandlers[component.id](s)
    return true
  result = false

proc nimCheckboxBridge*(component: ptr IRComponent): bool =
  # Stub for checkbox handling
  result = false

proc registerCheckboxHandler*(component: ptr IRComponent, handler: proc(checked: bool)) =
  # Stub for checkbox handler registration
  discard

proc registerInputHandler*(component: ptr IRComponent, onChange: proc(text: string), onSubmit: proc(text: string) = nil) =
  if component.isNil or onChange.isNil:
    return
  inputHandlers[component.id] = onChange

# Font management (resources)
var fontSearchDirs: seq[string] = @[]
var fontRegistry = initTable[string, string]()  # name -> resolved path

proc addFontSearchDir*(path: string) =
  if path.len > 0 and not fontSearchDirs.contains(path):
    fontSearchDirs.add(path)

proc resolveFontPath(pathOrName: string): string =
  ## Resolve a font path using search dirs; returns empty if not found
  if pathOrName.len == 0: return ""
  if fileExists(pathOrName):
    return absolutePath(pathOrName)
  for dir in fontSearchDirs:
    let candidate = absolutePath(joinPath(dir, pathOrName))
    if fileExists(candidate): return candidate
  result = ""

proc registerFont*(name: string, source: string, size: int = 16): bool =
  let resolved = resolveFontPath(source)
  if resolved.len == 0: return false
  fontRegistry[name] = resolved
  desktop_ir_register_font(cstring(name), cstring(resolved))
  if fontRegistry.len == 1:
    desktop_ir_set_default_font(cstring(name))
  result = true

proc loadFont*(path: string, size: int): int =
  ## Compatibility helper: try to load a font given a name or path
  var name = path
  let resolved = resolveFontPath(path)
  if resolved.len > 0:
    name = splitFile(path).name
    discard registerFont(name, resolved, size)
    return 1
  # If path not found, still register name (renderer may find via fontconfig)
  discard registerFont(name, path, size)
  result = 1

proc getFontId*(path: string, size: int): int =
  ## Return non-zero if font is known/registered
  if fontRegistry.hasKey(path): return 1
  if registerFont(path, path, size): return 1
  result = 0

# ============================================================================
# Handler Cleanup Functions
# ============================================================================

proc cleanupButtonHandlerFor*(component: ptr IRComponent) =
  ## Remove button handler for a specific component
  if component.isNil:
    return
  if buttonHandlers.hasKey(component.id):
    buttonHandlers.del(component.id)
    echo "[kryon][handlers] Removed button handler for id=", component.id

proc cleanupHandlersForSubtree*(root: ptr IRComponent) =
  ## Clean up all handlers for components in a subtree
  ## Called when a component tree is being removed
  if root.isNil:
    return

  # Clean up handler for this component
  if buttonHandlers.hasKey(root.id):
    buttonHandlers.del(root.id)
  if checkboxHandlers.hasKey(root.id):
    checkboxHandlers.del(root.id)
  if canvasHandlers.hasKey(root.id):
    canvasHandlers.del(root.id)
  if inputHandlers.hasKey(root.id):
    inputHandlers.del(root.id)
  if dropdownHandlers.hasKey(root.id):
    dropdownHandlers.del(root.id)

  # Recursively clean up children
  if root.children != nil and root.child_count > 0:
    let childArray = cast[ptr UncheckedArray[ptr IRComponent]](root.children)
    for i in 0..<root.child_count:
      let child = childArray[i]
      if child != nil:
        cleanupHandlersForSubtree(child)

proc nimCleanupHandlersForComponent*(component: ptr IRComponent) {.exportc: "nimCleanupHandlersForComponent", cdecl, dynlib.} =
  ## C-callable bridge to clean up handlers when a component is removed
  cleanupHandlersForSubtree(component)

# Export IR functions for DSL use
export ir_add_child, ir_create_component, ir_set_text_content
export IRComponentType, IRAlignment  # Export the enum types

# Export compatibility types
export KryonComponent, KryonRenderer, KryonApp, KryonAlignment

# Export alignment constants
export kaStart, kaCenter, kaEnd, kaStretch, kaSpaceEvenly, kaSpaceAround, kaSpaceBetween
