# Editing native TSX in VS Code

Open the Kryon repository folder (or a generated app's folder) in VS Code and
install **Ziran Language** (`kryonlabs.ziran-language`) from
[ziranlang/ziran-vscode](https://git.waozi.xyz/ziranlang/ziran-vscode).
The repository and the `app`, `pages`, `tui` and `web` starters carry the same
editor settings. An extension update requires reloading an already open window.

## What is declared

`.ts` uses VS Code's built-in **TypeScript** language and `.tsx` uses
**TypeScript React** (`typescriptreact`). This is the built-in TSX grammar;
Kryon does not use React and does not need a fork of a TypeScript extension.
The `tsconfig.json` selects preserved JSX, no JavaScript emit and no implicit
DOM or npm `@types` libraries. It describes the editor project, not the native
build. Use the ordinary `kryon`/`ziran` commands to build.

These files are Ziran's native TypeScript frontend, which differs from
JavaScript TypeScript. Its imports resolve through `ziran.toml` exports and
`ziran.lock`, including real checked Ziran records and functions. Native scalar
conversions such as `s32(32)`, pointers such as `*Native.State`, borrowed slices
and value-returning JSX are part of that frontend. JSX is a native function
call: `<Button .../>` can return a boolean and `<Text .../>` returns void.

An ambient `.d.ts` can name scalar types, but cannot teach TypeScript's parser
native pointer/slice syntax, resolve Ziran packages or preserve each JSX call's
result type. Handwritten copies of widget records would drift from the native
API, and a catch-all module declaration would hide mistakes. No such shim or
React declarations are installed here.

## What checks the files

The workspace setting `typescript.validate.enable: false` disables the
JavaScript TypeScript checker's incompatible diagnostics for this native
project. It does not change user settings or other workspaces. Built-in TSX
highlighting, brackets, indentation and editing remain available. TypeScript
IntelliSense only understands the standard TypeScript portion; native API
information comes from the Ziran extension.

The updated Ziran extension checks native `.zi`, `.ts` and `.tsx` with the
actual compiler on open/save. Problems from that checker have source `ziran`:
unknown imports, invalid fields, types and native lifetime rules remain errors.
The checks read saved source, so save changes before inspecting diagnostics.
If the extension is missing or the toolchain cannot run, native checking is
unavailable; turning off TypeScript diagnostics alone does not validate a file.
For a direct check from the repository root:

```sh
ziran check examples/hello.tsx
```

Use the project's pinned toolchain and package configuration; an optional local
checkout override belongs in ignored `ziran.local.toml` as usual. Do not install
React, `@types/react`, an npm package named `kryon`, or replace a native import
with a relative JavaScript import to satisfy TypeScript.

## Inspecting the pipeline

Use **Ziran: Run** or the editor's **▶** button on a `.zi`, `.ts` or `.tsx`
source. The Run pane or Kryon preview shows the available checked IR and target
stages. `.zir` is checked binary IR
and `.zx` is a binary portable executable/library; the Ziran extension supplies
binary inspection views. They are not source text and should not be associated
with TypeScript or given a text grammar. Unsupported backend operations still
report the real compiler error.
