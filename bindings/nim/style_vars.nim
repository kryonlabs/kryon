## Style Variables System for Kryon
## Provides CSS-like variable references for theming support

import strutils
import ir_core

const kryonIrLib* {.strdefine.} = "build/libkryon_ir.so"

# Style Variable ID type
type IRStyleVarId* = uint16

# Builtin style variable IDs (match C enum)
const
  IR_VAR_NONE* = 0.IRStyleVarId
  IR_VAR_PRIMARY* = 1.IRStyleVarId
  IR_VAR_PRIMARY_HOVER* = 2.IRStyleVarId
  IR_VAR_SECONDARY* = 3.IRStyleVarId
  IR_VAR_ACCENT* = 4.IRStyleVarId
  IR_VAR_BACKGROUND* = 5.IRStyleVarId
  IR_VAR_SURFACE* = 6.IRStyleVarId
  IR_VAR_SURFACE_HOVER* = 7.IRStyleVarId
  IR_VAR_CARD* = 8.IRStyleVarId
  IR_VAR_TEXT* = 9.IRStyleVarId
  IR_VAR_TEXT_MUTED* = 10.IRStyleVarId
  IR_VAR_TEXT_ON_PRIMARY* = 11.IRStyleVarId
  IR_VAR_SUCCESS* = 12.IRStyleVarId
  IR_VAR_WARNING* = 13.IRStyleVarId
  IR_VAR_ERROR* = 14.IRStyleVarId
  IR_VAR_BORDER* = 15.IRStyleVarId
  IR_VAR_DIVIDER* = 16.IRStyleVarId
  IR_VAR_CUSTOM_START* = 64.IRStyleVarId

# Semantic name aliases for convenience
const
  varPrimary* = IR_VAR_PRIMARY
  varPrimaryHover* = IR_VAR_PRIMARY_HOVER
  varSecondary* = IR_VAR_SECONDARY
  varAccent* = IR_VAR_ACCENT
  varBackground* = IR_VAR_BACKGROUND
  varSurface* = IR_VAR_SURFACE
  varSurfaceHover* = IR_VAR_SURFACE_HOVER
  varCard* = IR_VAR_CARD
  varText* = IR_VAR_TEXT
  varTextMuted* = IR_VAR_TEXT_MUTED
  varTextOnPrimary* = IR_VAR_TEXT_ON_PRIMARY
  varSuccess* = IR_VAR_SUCCESS
  varWarning* = IR_VAR_WARNING
  varError* = IR_VAR_ERROR
  varBorder* = IR_VAR_BORDER
  varDivider* = IR_VAR_DIVIDER

# C API imports
proc ir_style_var_set*(id: IRStyleVarId; r, g, b, a: uint8) {.importc, cdecl, dynlib: kryonIrLib.}
proc ir_style_var_set_hex*(id: IRStyleVarId; hex_color: uint32) {.importc, cdecl, dynlib: kryonIrLib.}
proc ir_style_var_get*(id: IRStyleVarId; r, g, b, a: ptr uint8): bool {.importc, cdecl, dynlib: kryonIrLib.}
proc ir_style_vars_apply_theme*(colors: ptr uint32) {.importc, cdecl, dynlib: kryonIrLib.}
proc ir_style_vars_is_dirty*(): bool {.importc, cdecl, dynlib: kryonIrLib.}
proc ir_style_vars_clear_dirty*() {.importc, cdecl, dynlib: kryonIrLib.}

# Setter functions for component styles with variable references
proc ir_set_background_color_var*(style: ptr IRStyle; var_id: IRStyleVarId) {.importc, cdecl, dynlib: kryonIrLib.}
proc ir_set_text_color_var*(style: ptr IRStyle; var_id: IRStyleVarId) {.importc, cdecl, dynlib: kryonIrLib.}
proc ir_set_border_color_var*(style: ptr IRStyle; var_id: IRStyleVarId) {.importc, cdecl, dynlib: kryonIrLib.}

# Theme type - array of 16 RGBA colors (packed as 0xRRGGBBAA)
type Theme* = array[16, uint32]

# Helper to create color from hex string
proc hexToRgba*(hex: string): uint32 =
  ## Convert "#RRGGBB" or "#RRGGBBAA" to packed RGBA
  var h = hex
  if h.len > 0 and h[0] == '#':
    h = h[1..^1]
  if h.len == 6:
    h = h & "FF"  # Add full opacity
  result = uint32(parseHexInt("0x" & h))

# Helper to set a style variable from hex string
proc setStyleVar*(id: IRStyleVarId; hex: string) =
  let packed = hexToRgba(hex)
  ir_style_var_set_hex(id, packed)

# Helper to apply a theme (array of 16 hex colors)
proc applyTheme*(theme: Theme) =
  ## Apply a theme by setting all 16 builtin style variables
  var themeArr = theme  # Make mutable copy
  ir_style_vars_apply_theme(addr themeArr[0])

# Create an IRColor that references a style variable
proc colorVar*(varId: IRStyleVarId): IRColor =
  ## Create a color that references a style variable
  ## The actual color will be resolved at render time
  result.`type` = IR_COLOR_VAR_REF
  result.var_id = varId  # Uses the union field directly
