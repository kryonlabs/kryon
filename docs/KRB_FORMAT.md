# Kryon Cartridge Format (krb)

A `.krb` file is Kryon's portable cartridge format. It is a little-endian,
mmapable image produced from KIR: a synthetic VFS node table, a string table, a
small program, host imports, controls, and room for state, source maps, assets,
and portable logic sections as the format grows.

The current v1 image is render-first: it can draw encoded UI nodes through a
`KryBackend`, mount host state, and call bound host imports. The target design is
a full cartridge: KIR-owned logic executes through the portable runtime, while
native C libraries enter through explicit capabilities or host imports.

## Layout

```
header          32 bytes (28 used, 4 pad)
nodes[]         28 bytes × node_count
strings[]       string_bytes, UTF-8, NUL-terminated, offset 0 is ""
prog[]          prog_bytes
imports[]       u32 string offset × import_count
controls[]      24 bytes × control_count
```

## Header

| Field | Type | Notes |
|---|---|---|
| magic | u32 | `0x0042524B` (`KRB\0`) |
| version | u16 | `1` |
| flags | u16 | reserved, 0 |
| node_count | u32 | |
| string_bytes | u32 | |
| prog_bytes | u32 | |
| import_count | u32 | host bind names (button handlers) |
| control_count | u32 | interactive controls (was `reserved`, always 0) |

## Node (28 bytes)

| Field | Type |
|---|---|
| id | u16 |
| parent | i16 (`-1` = root) |
| name_off | u16 (string table) |
| type | u8 (`BACKGROUND` 1, `TEXT` 2, `RECT` 3, `BUTTON` 4, `DATA` 5, `PICTURE` 6, `CHECKBOX` 7, `TOGGLE` 8, `CONTROL` 9; 0 reserved) |
| flags | u8 (`SCALE_X/Y/W/H` in bits 2–5; bits 0–1 reserved) |
| bind_slot | u16 (`0xffff` = none) |
| x y w h | i16 each |
| color | u32 (RGBA, or `0x80000000 \| theme_slot`) |
| text_off | u16 |
| font_size | u16 |
| style | u8 |
| pad | u8 |

Theme slots: 0 background, 1 text, 2 icon, 3 surface, 4 button.

PICTURE nodes carry the asset path in `text_off`, the tint in `color`, and the
`UIPictureFit` (0 stretch, 1 contain, 2 cover) in `style`. The walker draws
them through `KryBackend.texture`, which loads the asset via
`KryLoadPictureTexture` (runtime file or embedded asset) and fits it into the
node bounds. `DATA` nodes are state-field metadata and are not drawn.

CHECKBOX and TOGGLE are interactive: `name_off` is the bound state-field path
(mounted by the host), `text_off` the label, `bind_slot` the widget id. The
cartridge owns the toggle — each frame it reads the value via the mount, draws
the box/switch with `KryBackend` primitives, and on an in-bounds mouse press
writes the flipped value back through the mount. Only `state {}` fields can
bind (they are what the host mounts); a widget whose value isn't a state field
renders its default state.

## Control (24 bytes)

A `CONTROL` node's `bind_slot` indexes the `controls[]` table. Each record
carries the args that don't fit in a node:

| Field | Type | Notes |
|---|---|---|
| kind | u8 | `SLIDER` 1, `VSLIDER` 2, `SPINBOX` 3, `DROPDOWN` 4, `COMBOBOX` 5 |
| option_count | u8 | dropdown/combobox options (unused by range widgets) |
| id | u16 | widget id |
| min / max / step | i32 each | range |
| value_off | u16 | bound state-field path (string table) |
| label_off | u16 | label |
| options_off | u16 | first option string (unused by range widgets) |
| reserved | u16 | 0 |

The node's bounds are the widget bounds; `value_off` is the mount path. The
walker reads the value, renders with primitives (slider track+thumb, spinbox
field+`-`/`+`), and updates the value on interaction: a held drag sets a
slider across `[min,max]`; a click steps a spinbox by `step` (clamped). Slider,
VerticalSlider, and Spinbox are emitted today; Dropdown/Combobox (need a
state-array option parser) and by-value widgets (Radio/TabBar) are deferred.

## Program

A byte stream of opcodes. The current cartridge compiler writes `OP_DRAW_TREE`
for render-only cartridges. Future KRB versions should either reference KIR
logic functions directly or carry a lowered bytecode/WASM section derived from
KIR. Hosts may still bind native imports for platform services.

| Op | Byte | Args | Meaning |
|---|---|---|---|
| `OP_DRAW_TREE` | `0x01` | — | Draw every node through `KryBackend` |
| `OP_CALL_HOST` | `0x02` | u8 slot | Call the function bound to import slot |
| `OP_SET_I32` | `0x03` | u16 path_off, i32 value | Write a mounted C `int` at `path` |

`KrbExec` runs the program. `KrbDraw` runs `OP_DRAW_TREE` (or the whole
program if it is more than that one opcode).

## C memory as files

`KrbMount(img, "/app", ptr, fields)` maps live C fields onto paths.
`KrbBindMem(img, "score", &score, KRB_I32, 4)` is a one-field mount.

Read/write is a typed memcpy at a compile-time offset:

```c
KrbField fields[] = {
    { "score", offsetof(App, score), KRB_I32, 4 },
    { "label", offsetof(App, label), KRB_CSTR, 16 },
    { NULL }
};
KrbMount(&img, "/app", app, fields);
KrbReadI32(&img, "/app/score", &n);
```

A TEXT node whose name matches a mounted path draws the live value (the stored
string is a `printf` format when it contains `%`).

`k2b` is the intended `.kry`/`.kir` to `.krb` compiler. It should read `.kir`
directly or run the `.kry -> KIR` frontend internally, then write the cartridge
sections. Native apps that want readable generated C use `k2c`; portable
renderers load `.krb` and provide the standard capability/import table.
