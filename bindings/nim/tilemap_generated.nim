# 0BSD
## tilemap Plugin Bindings for Nim
## Auto-generated from bindings.json - DO NOT EDIT MANUALLY
##
## 2D tile-based rendering for games
## Version: 1.0.0

import strutils

# Link plugin libraries
{.passL: "-lkryon_desktop -lkryon_ir".}

# ============================================================================
# C Function Imports
# ============================================================================

proc kryon_tilemap_create*(width: uint16, height: uint16, tile_width: uint16, tile_height: uint16, layer: uint8) {.
  importc: "kryon_tilemap_create",
  header: "../../plugins/tilemap/tilemap_plugin.h".}

proc kryon_tilemap_set_tile*(x: uint16, y: uint16, tile_id: uint16, flags: uint8, layer: uint8) {.
  importc: "kryon_tilemap_set_tile",
  header: "../../plugins/tilemap/tilemap_plugin.h".}

proc kryon_tilemap_render*(x: int32, y: int32, layer: uint8) {.
  importc: "kryon_tilemap_render",
  header: "../../plugins/tilemap/tilemap_plugin.h".}

# ============================================================================
# Helper Functions
# ============================================================================

proc parseColor*(hex: string): uint32 =
  ## Convert hex color string (#RRGGBB or #RRGGBBAA) to uint32 RGBA
  var h = hex
  if h.startsWith("#"):
    h = h[1..^1]

  if h.len == 6:
    # RGB - add full alpha
    let r = parseHexInt(h[0..1])
    let g = parseHexInt(h[2..3])
    let b = parseHexInt(h[4..5])
    result = (r.uint32 shl 24) or (g.uint32 shl 16) or (b.uint32 shl 8) or 0xFF'u32
  elif h.len == 8:
    # RGBA
    let r = parseHexInt(h[0..1])
    let g = parseHexInt(h[2..3])
    let b = parseHexInt(h[4..5])
    let a = parseHexInt(h[6..7])
    result = (r.uint32 shl 24) or (g.uint32 shl 16) or (b.uint32 shl 8) or a.uint32
  else:
    # Default to white
    result = 0xFFFFFFFF'u32

# ============================================================================
# High-Level Wrappers
# ============================================================================

proc createTilemap*(width: uint16, height: uint16, tile_width: uint16, tile_height: uint16, layer: uint8 = 0) =
  ## Create a new tilemap layer
  ## - width: Width in tiles
  ## - height: Height in tiles
  ## - tile_width: Width of each tile in pixels
  ## - tile_height: Height of each tile in pixels
  ## - layer: Layer index (0-3)
  kryon_tilemap_create(width, height, tile_width, tile_height, layer)

proc setTile*(x: uint16, y: uint16, tile_id: uint16, flags: uint8 = 0, layer: uint8 = 0) =
  ## Set a tile at a specific position
  ## - x: X position in tiles
  ## - y: Y position in tiles
  ## - tile_id: Tile ID (index into tileset)
  ## - flags: Flip flags (0 = normal, 1 = flip horizontal, 2 = flip vertical, 4 = flip diagonal)
  ## - layer: Layer index (0-3)
  kryon_tilemap_set_tile(x, y, tile_id, flags, layer)

proc renderTilemap*(x: int32, y: int32, layer: uint8 = 0) =
  ## Render the tilemap at a position
  ## - x: X position to render at (in pixels)
  ## - y: Y position to render at (in pixels)
  ## - layer: Layer index (0-3)
  kryon_tilemap_render(x, y, layer)

