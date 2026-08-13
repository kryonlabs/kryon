# Kryon Cartridge Format (krb)

A `.krb` file is a little-endian, mmapable image: a synthetic VFS node table,
a string table, a tiny program, and a host-import name list. Behavior stays
in C. `prog[]` starts as “draw this tree”; later opcodes and
`kry_bind("/app", ptr, layout)` extend the same file.

## Layout

```
header          32 bytes (28 used, 4 pad)
nodes[]         28 bytes × node_count
strings[]       string_bytes, UTF-8, NUL-terminated, offset 0 is ""
prog[]          prog_bytes
imports[]       u32 string offset × import_count
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
| import_count | u32 | host bind names (`#extern` / `#export` later) |
| reserved | u32 | 0 |

## Node (28 bytes)

| Field | Type |
|---|---|
| id | u16 |
| parent | i16 (`-1` = root) |
| name_off | u16 (string table) |
| type | u8 (`GROUP` 0, `BACKGROUND` 1, `TEXT` 2, `RECT` 3, `BUTTON` 4) |
| flags | u8 (export in bits 0–1; `SCALE_X/Y/W/H` in bits 2–5) |
| bind_slot | u16 (`0xffff` = none) |
| x y w h | i16 each |
| color | u32 (RGBA, or `0x80000000 \| theme_slot`) |
| text_off | u16 |
| font_size | u16 |
| style | u8 |
| pad | u8 |

Theme slots: 0 background, 1 text, 2 icon, 3 surface, 4 button.

## Program

A byte stream of opcodes. `kc --emit-krb` writes `OP_DRAW_TREE` only. Hosts
may append more.

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

`kc --emit-krb` also writes `foo.krb.c` / `foo.krb.h`. That host embeds the
image, mounts `state { }` fields, and turns each `if Button { ... }` body into
a C bind. Existing C calls `Name_krb_draw` / `Name_krb_press`. Compile with
`KRYON_KRB_NO_MAIN` to skip the generated `main`.
