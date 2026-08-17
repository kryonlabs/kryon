# Kryon Cartridge Format (krb) — exact specification

A `.krb` file is Kryon's compact portable cartridge: everything a renderer
needs to draw and run a UI, in one little-endian, mmapable binary. This
document is normative. An implementation that follows it can parse and
render any cartridge without kryon source code.

Reference implementation: `src/krb/krb.c` (runtime), `cmd/k2b` (compiler),
`include/krb.h` (constants). Reference renderers: `src/backend/kry_sw.c`
(software), `cmd/krb-run` (headless), `cmd/krb-web` (wasm), `cmd/krb-sdl`.

## 1. Encoding rules

- All multi-byte integers are **little-endian**, packed, no alignment
  padding inside records.
- Strings are UTF-8, NUL-terminated, stored once in the string table;
  every string reference elsewhere is a `u16` **byte offset** into it.
- Offsets into `prog[]` are `u32` **absolute** byte offsets from the start
  of the program section.
- Colors are `u32` packed `0xRRGGBBAA`. Theme references: see §4.

## 2. File layout

One linear image, sections in this exact order, no gaps:

```
offset  size                    section
0       32                      header (28 used + 4 reserved)
H       28 × node_count         nodes[]
N       string_bytes            strings[]
S       prog_bytes              prog[]
P       4 × import_count        imports[] (u32 string offsets each)
I       24 × control_count      controls[]
```

with `H = 32`, `N = H + 28 × node_count`, `S = N + string_bytes`,
`P = S + prog_bytes`, `I = P + 4 × import_count`. A loader MUST reject the
file when `file_size < I + 24 × control_count`.

## 3. Header (32 bytes)

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 4 | magic | `0x0042524B` — bytes `4B 52 42 00` (`"KRB\0"`) |
| 4 | 2 | version | `1` or `2` (see §7); a loader MUST reject other values |
| 6 | 2 | flags | reserved, MUST be 0 |
| 8 | 4 | node_count | number of node records |
| 12 | 4 | string_bytes | size of string table in bytes |
| 16 | 4 | prog_bytes | size of program section |
| 20 | 4 | import_count | number of host-import name offsets |
| 24 | 4 | control_count | number of control records |
| 28 | 4 | reserved | MUST be 0 |

Additional loader requirements: `string_bytes > 0` and the byte at string
offset `0` MUST be `0x00` (the empty string lives at offset 0).

## 4. Node record (28 bytes)

| Offset | Size | Field | Type | Meaning |
|---|---|---|---|---|
| 0 | 2 | id | u16 | node id (serial number; equals record index today) |
| 2 | 2 | parent | i16 | parent node index, `-1` = root |
| 4 | 2 | name_off | u16 | string offset (name / bound state path) |
| 6 | 1 | type | u8 | node type, §4.1 |
| 7 | 1 | flags | u8 | bit flags, §4.2 |
| 8 | 2 | bind_slot | u16 | import slot or control index; `0xFFFF` = none |
| 10 | 2 | x | i16 | x coordinate (meaning per type) |
| 12 | 2 | y | i16 | y coordinate |
| 14 | 2 | w | i16 | width / radius per type |
| 16 | 2 | h | i16 | height / inner radius per type |
| 18 | 4 | color | u32 | `0xRRGGBBAA` or theme reference |
| 22 | 2 | text_off | u16 | string offset (label / asset path / format) |
| 24 | 2 | font_size | u16 | pixels; `0` renders as 16 |
| 26 | 1 | style | u8 | per-type substyle |
| 27 | 1 | pad | u8 | reserved, MUST be 0 |

### 4.1 Node types

| Value | Name | x,y / w,h meaning | Drawn as |
|---|---|---|---|
| 0 | — | reserved | — |
| 1 | BACKGROUND | w,h = size (see §8) | filled rect, the screen backdrop |
| 2 | TEXT | x,y = top-left | text, `text_off` string, `font_size` |
| 3 | RECT | x,y,w,h = bounds | filled rect |
| 4 | BUTTON | bounds | filled rect + 1px border + centered label (`text_off`); import `bind_slot` fires on press-in-bounds. `style`: 0 primary (theme button color), 1 plain (theme surface), 2 danger (`0xB83B3BFF`) |
| 5 | DATA | — | not drawn; state-field metadata |
| 6 | PICTURE | bounds | texture; `text_off` = asset path, `color` = tint, `style` = fit (0 stretch, 1 contain, 2 cover) |
| 7 | CHECKBOX | box at bounds; label at `x+w+4` | cartridge-owned toggle on mount path `name_off`; flips value on press-in-bounds |
| 8 | TOGGLE | switch at bounds; label beside | same mount behavior as CHECKBOX |
| 9 | CONTROL | bounds = widget bounds | range widget; `bind_slot` indexes controls[] |
| 10 | CIRCLE | x,y = center; w = radius | filled circle |
| 11 | RING | x,y = center; w = outer, h = inner radius | annulus |
| 12 | SCROLL | bounds = viewport; `font_size` = content height; `name_off` = mount path of pixel offset | container: children (nodes with `parent` = this index) draw translated by `-offset` and clipped to the viewport; wheel over the viewport adjusts the mounted offset (clamped to `[0, content_h - h]`) |

### 4.2 Flags

| Bit | Mask | Name | Meaning |
|---|---|---|---|
| 0–1 | `0x03` | reserved | MUST be 0 |
| 2 | `0x04` | SCALE_X | x is in UI-scale units (multiply by backend scale) |
| 3 | `0x08` | SCALE_Y | same for y |
| 4 | `0x10` | SCALE_W | same for w |
| 5 | `0x20` | SCALE_H | same for h |

### 4.3 Colors and theme slots

A node `color` whose **top bit is set** (`color & 0x80000000 != 0`) is a
theme reference: `slot = color & 0x7FFFFFFF`; the renderer asks the host
backend for the slot color. Slots: `0` background, `1` text, `2` icon,
`3` surface, `4` button; other slot values resolve to the backend default.

**Consequence (deliberate v1 constraint):** any *literal* color with a red
channel ≥ `0x80` also has the top bit set and reads as a theme reference.
Literal cartridge colors MUST keep R < `0x80`; use a theme slot for bright
reds. (A future version may move the sentinel; until then this rule is
normative.)

## 5. String table

`string_bytes` bytes of concatenated NUL-terminated UTF-8 strings. Offset 0
is the empty string (the table starts with a `0x00` byte). A string
reference is valid iff `offset < string_bytes - 1` and the byte at
`offset` begins a complete string. Paths used as mount references (e.g.
`"counter"`, `"app/score"`) are plain strings compared exactly.

## 6. Imports

`import_count` × `u32` string offsets naming host functions (today: button
handlers). A node's `bind_slot` (when `< 0xFFFF`) selects the slot; the
host binds each slot with `KrbBind`/`KrbBindSlot`. A slot with no bound
function is skipped silently.

## 7. Control record (24 bytes)

Referenced by a CONTROL node's `bind_slot`.

| Offset | Size | Field | Type | Meaning |
|---|---|---|---|---|
| 0 | 1 | kind | u8 | 1 slider, 2 vslider, 3 spinbox (4/5 reserved) |
| 1 | 1 | option_count | u8 | reserved for dropdown options, 0 |
| 2 | 2 | id | u16 | widget id |
| 4 | 4 | min | i32 | range minimum |
| 8 | 4 | max | i32 | range maximum |
| 12 | 4 | step | i32 | increment |
| 16 | 2 | value_off | u16 | string offset: mount path of the value |
| 18 | 2 | label_off | u16 | string offset: label |
| 20 | 2 | options_off | u16 | reserved, 0 |
| 22 | 2 | reserved | u16 | MUST be 0 |

Slider: held-drag maps the pointer position across the track to
`[min, max]`. Spinbox: click left/right half decrements/increments by
`step`, clamped to `[min, max]`. Dropdown (`kind 4`): `options_off`
points at `option_count` consecutive NUL-terminated option strings;
the bound `int` holds the selected index. Click toggles the popup;
clicking a row writes the index and closes.

## 8. Program and execution semantics

`prog[]` is a byte stream executed in order by `KrbExec` once per frame.
Opcodes:

| Op | Byte | Operands (in order) | Semantics |
|---|---|---|---|
| `OP_DRAW_TREE` | `0x01` | — | draw every node, in record order |
| `OP_CALL_HOST` | `0x02` | u8 slot | call host function bound to slot |
| `OP_SET_I32` | `0x03` | u16 path_off, i32 value | write mounted int |
| `OP_PUSH_CONST` | `0x10` | i32 | push immediate |
| `OP_PUSH_PATH` | `0x11` | u16 path_off | push mounted int at path (unmounted → 0) |
| `OP_POP_STORE` | `0x12` | u16 path_off | pop → write mounted int at path |
| `OP_ADD` | `0x13` | — | pop b, pop a, push `a + b` |
| `OP_SUB` | `0x14` | — | push `a - b` |
| `OP_MUL` | `0x15` | — | push `a * b` |
| `OP_DIV` | `0x16` | — | push `b == 0 ? 0 : a / b` (C division) |
| `OP_EQ`/`NE`/`LT`/`LE`/`GT`/`GE` | `0x17`–`0x1C` | — | pop b, pop a, push 1 or 0 |
| `OP_JMP` | `0x1D` | u32 addr | set program counter to `addr` |
| `OP_JZ` | `0x1E` | u32 addr | pop; if 0, set program counter to `addr` |
| `OP_TIME` | `0x1F` | — | push `(int)(host time in seconds × 1000)` |
| `OP_DRAW_NODE` | `0x20` | u16 node index | draw one node |

Rules:

- The VM stack is 16-deep int32 and persists across opcodes within one
  `KrbExec` pass. Push on overflow or pop on underflow is an error
  (execution fails).
- Jump targets are absolute offsets into `prog[]`; a target outside
  `[0, prog_bytes]` makes the jump a no-op.
- Any other opcode byte is an error; execution stops.
- Drawing: node coordinates are relative to the current draw origin
  (0,0 for a full-frame pass). A SCALE_* flag replaces the raw i16 with
  `backend.scale_px(value)`. When `OP_DRAW_TREE` (or `OP_DRAW_NODE` on a
  BACKGROUND node) executes, a BACKGROUND node's w/h are overridden with the
  host screen size and its SCALE_W/H flags cleared.
- Interaction (BUTTON, CHECKBOX, TOGGLE, CONTROL) is evaluated at draw
  time: "pointer pressed this frame" AND "pointer inside the node bounds"
  (post-transform). Bounds are exclusive on the right/bottom edges.

### Versions

- **v1** — opcodes `0x01`–`0x03`, node types 1–9. Render-first.
- **v2** — adds opcodes `0x10`–`0x20` and node types 10–11. Loaders accept
  both; compilers should emit 2.

### Asset section (v2)

When header offset 28 (`asset_bytes`) is nonzero, an asset section follows
`controls[]`: a `u32 asset_count`, then `asset_count` directory entries of
20 bytes, then the blobs. Entry: `path_off u32@0` (string table),
`data_off u32@4` (absolute file offset), `size u32@8`, `kind u16@12`
(0 = raw RGBA8 pixels, 1 = glyph atlas), `w u16@14`, `h u16@16`,
`reserved u16@18`. `asset_bytes` counts the whole section. A PICTURE node
whose path matches an embedded raw-RGBA asset is rendered from cartridge
pixels through the backend's optional `texture_rgba` (scaled, tinted);
non-embedded paths fall back to host texture loading. The conventional
name "@atlas" carries a KFA1 glyph atlas: `u32 "KFA1" | u16 size_count`,
then per size a 16-byte record (`px u16 | glyphs u16 | w u16 | h u16 |
table_off u32 | pixels_off u32`), then per-glyph 18-byte records
(`cp u32 | x,y,w,h u16 | xoff,yoff i16 | advance u16` in 1/256-px fixed point,
offsets into the
size's RGBA8 white-on-alpha bitmap). Engines with atlas support render
antialiased text with true advances; without it they fall back to their
built-in font.

## 9. Limits (current implementation)

256 nodes, 8 KiB strings, 32 imports, 128 controls, 32 host binds,
16 mount roots, 64 fields per mount, 16 MiB cartridge (raised from
64 KiB to fit embedded atlases and images).

## 10. Host surface (what a renderer must provide)

A renderer implements the `KryBackend` table (see `include/kry_backend.h`):
`clear`, `rect`, `text`, `measure_text`, `clip_push/pop`, `mouse`,
`mouse_down`, `mouse_pressed`, `width`, `height`, `time`, `scale_px`,
`theme_color`, `texture`, and optional `circle`/`ring` (v2 geometry; a
renderer may leave them NULL, in which case CIRCLE/RING nodes are no-ops).
Colors passed to the backend are already resolved literals. Assets are
referenced by path; loading and caching are host-owned.

## 11. C memory as files (mounts)

Hosts map live memory onto string paths:

```c
KrbField fields[] = {
    { "score", offsetof(App, score), KRB_I32, 4 },
    { "label", offsetof(App, label), KRB_CSTR, 16 },
    { NULL }
};
KrbMount(&img, "/app", app, fields);
KrbReadI32(&img, "/app/score", &n);
/* or a single field: */
KrbBindMem(&img, "counter", &counter, KRB_I32, 4);
```

Field kinds: `KRB_I32 1`, `KRB_U32 2`, `KRB_F32 3`, `KRB_BOOL 4`,
`KRB_CSTR 5`. Read/write is a typed memcpy. A TEXT node whose `name_off`
string equals a mounted path renders the live value; its `text_off` string
acts as a `printf` format when it contains `%`.

## 12. Tooling

- `k2b` compiles `.kry` → `.krb` (+ optional generated C host; `--no-main`
  omits the host `main`). Unsupported `.kry` calls are dropped and reported.
- `krb-run` renders headless to PNG / a recorded call stream.
- `krb-web` / `krb-sdl` are full hosts; `tests/golden` + `tests/krb_engine_test.sh`
  pin cross-engine byte-identical output.
