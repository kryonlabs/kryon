# Kryon Architecture - Final Correct Design

## Core Principle: KIR-Centric Pipeline

**Everything flows through KIR. All targets and codegens consume KIR.**

```
┌─────────────────────────────────────────────────────────────────┐
│                    FRONTENDS (Parsers)                          │
│                                                                 │
│  .kry   .tsx   .html   .md   .c   .nim   .lua   .rs           │
│    ↓     ↓      ↓      ↓     ↓     ↓      ↓      ↓            │
└─────────────────────────────────────────────────────────────────┘
                            ↓
                ┌───────────────────────┐
                │    KIR JSON           │
                │ (Universal IR Format) │
                └───────────────────────┘
                            ↓
        ┌───────────────────┼───────────────────┬──────────────┐
        ↓                   ↓                   ↓              ↓
   ┌─────────┐        ┌──────────┐       ┌──────────┐   ┌─────────┐
   │   VM    │        │  NATIVE  │       │ CODEGENS │   │  WASM   │
   │ TARGET  │        │  TARGET  │       │          │   │ TARGET  │
   └─────────┘        └──────────┘       └──────────┘   └─────────┘
        ↓                   ↓                   ↓              ↓
```

## Target 1: VM Target (.krb Bytecode)

```
KIR
   ↓
┌──────────────────┐
│ .krb Compiler    │
│ (KIR → Bytecode) │
└──────────────────┘
   ↓
.krb File
(Portable bytecode)
   ↓
┌──────────────────┐
│  KryonVM Runtime │
│  (Interpreter)   │
└──────────────────┘
   ↓
┌────────────────────────────────────┐
│     Runs on ANY Renderer           │
│                                    │
│  SDL3   Terminal   DOM   Embedded  │
└────────────────────────────────────┘
```

**Key Points:**
- Platform-independent bytecode
- Single .krb file runs everywhere
- Runtime chooses renderer
- Can use .krb with ANY renderer backend

**Commands:**
```bash
kryon build app.kry --target=krb -o app.krb

# Run on different renderers
kryon run app.krb --renderer=sdl3      # Desktop with SDL3
kryon run app.krb --renderer=terminal  # Terminal UI
kryon run app.krb --renderer=dom       # Web (via Node.js VM)
```

---

## Target 2: Native Target (Binary)

```
KIR
   ↓
┌──────────────────────┐
│  Source Codegen      │
│  (KIR → C/Nim/Rust)  │
│  (in-memory or file) │
└──────────────────────┘
   ↓
Generated Source Code
(C, Nim, or Rust)
   ↓
┌──────────────────────┐
│  Native Compiler     │
│  gcc / nim / cargo   │
└──────────────────────┘
   ↓
Platform-Specific Binary
(x86_64, ARM, etc.)
   ↓
┌──────────────────────┐
│  Embedded Renderer   │
│  SDL3 / Terminal /   │
│  Framebuffer         │
└──────────────────────┘
```

**Key Points:**
- Uses IR pipeline (KIR → Source)
- Source can be generated in-memory (no files)
- Or use existing KIR file directly
- Produces **platform-specific** native binary
- Binary is tied to specific CPU architecture
- Renderer is compiled into the binary

**Commands:**
```bash
# Compile to native binary
kryon build app.kry --target=native --lang=c --renderer=sdl3 -o app
# → KIR → C code (in-memory) → gcc → binary with SDL3

# For specific platform
kryon build app.kry --target=native --platform=arm64 --lang=c -o app_arm
# → Cross-compilation

# Embedded
kryon build app.kry --target=native --platform=stm32 --renderer=framebuffer
```

---

## Codegens (Source Output)

```
KIR
   ↓
┌──────────────────────────────────┐
│      Language Codegens           │
│                                  │
│  - Nim Codegen                   │
│  - Lua Codegen                   │
│  - C Codegen                     │
│  - Rust Codegen                  │
│  - HTML Codegen (with --flags)   │
│  - JSX/TSX Codegen               │
└──────────────────────────────────┘
   ↓
Generated Source Files
(.nim, .lua, .c, .rs, .html, .tsx)
   ↓
(User can compile/use these files)
```

**HTML Codegen is NOT Special:**
- HTML is **just another codegen** like Nim, Lua, C
- Generates static files (.html, .css, .js)
- Has extra flags for web deployment: `--minify`, `--bundle`, `--inline-css`
- Can round-trip: HTML → KIR → Lua → KIR → HTML

**Key Points:**
- All codegens consume KIR
- All produce source code or files
- HTML codegen has web-specific flags
- Output files can be used independently

**Commands:**
```bash
# Generate Nim source
kryon codegen nim app.kir -o app.nim

# Generate Lua source
kryon codegen lua app.kir -o app.lua

# Generate HTML (basic)
kryon codegen html app.kir -o dist/

# Generate HTML (web target with flags)
kryon codegen html app.kir -o dist/ --minify --inline-css --bundle
# These flags make it a "web target" for deployment

# Round-trip example
kryon compile app.html -o app.kir         # HTML → KIR
kryon codegen lua app.kir -o app.lua      # KIR → Lua
kryon compile app.lua -o app2.kir         # Lua → KIR
kryon codegen html app2.kir -o dist/      # KIR → HTML
```

---

## Target 3: WASM Target

```
KIR
   ↓
┌──────────────────────────────────┐
│  WASM Compilation Pipeline       │
│                                  │
│  Option A: Via Native Source     │
│  KIR → C → emscripten → .wasm    │
│                                  │
│  Option B: Via VM                │
│  KIR → .krb → wasm-vm → .wasm    │
└──────────────────────────────────┘
   ↓
.wasm Module
   ↓
┌──────────────────────────────────┐
│  Browser WebAssembly Runtime     │
│  or Native WASM Runtime          │
└──────────────────────────────────┘
   ↓
┌──────────────────────────────────┐
│  Renderer                        │
│  Canvas / WebGL / DOM Bindings   │
└──────────────────────────────────┘
```

**Key Points:**
- WASM is another target option
- Can use native pipeline (C → WASM) OR VM pipeline (.krb → WASM)
- Runs in browser OR standalone WASM runtime
- Can target Canvas/WebGL for graphics

**Commands:**
```bash
# Via native pipeline
kryon build app.kry --target=wasm --lang=c -o app.wasm
# → KIR → C → emscripten → .wasm

# Via VM pipeline
kryon build app.kry --target=wasm --via-vm -o app.wasm
# → KIR → .krb → wasm-compiler → .wasm

# Run
# In browser: <script src="app.wasm"></script>
# Or: wasmtime app.wasm
```

---

## Complete Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    FRONTENDS (Parsers)                          │
│  .kry  .tsx  .html  .md  .c  .nim  .lua  .rs                   │
└─────────────────────────────────────────────────────────────────┘
                            ↓
                ┌───────────────────────┐
                │    KIR JSON           │
                │ (Intermediate Repr.)  │
                └───────────────────────┘
                            ↓
        ┌───────────────────┼───────────────────┬──────────────┐
        ↓                   ↓                   ↓              ↓
   ┌─────────┐        ┌──────────┐       ┌──────────┐   ┌─────────┐
   │VM TARGET│        │NATIVE    │       │CODEGENS  │   │  WASM   │
   │         │        │TARGET    │       │          │   │ TARGET  │
   └─────────┘        └──────────┘       └──────────┘   └─────────┘
        ↓                   ↓                   ↓              ↓
   .krb File          C/Nim/Rust         .nim .lua        .wasm
        ↓              Source Code        .c .html         Module
        ↓                   ↓             .tsx .rs            ↓
  VM Runtime          Native Compiler         │         WASM Runtime
        ↓                   ↓                  ↓              ↓
  ┌─────────┐         Platform Binary    Source Files   Browser/
  │ANY      │              ↓              (for users)    Native
  │RENDERER │         ┌─────────┐                           ↓
  │         │         │Renderer │                      Canvas/
  │SDL3     │         │Embedded │                      WebGL/
  │Terminal │         │in Binary│                      DOM
  │DOM      │         └─────────┘
  │Embedded │
  └─────────┘
```

---

## Summary Table

| Target | Input | Output | Renderer | Platform |
|--------|-------|--------|----------|----------|
| **VM** | KIR | .krb bytecode | Any (SDL3, Terminal, DOM, Embedded) | All |
| **Native** | KIR | Platform binary | Compiled in (SDL3, Terminal, FB) | Specific |
| **Codegen** | KIR | Source files (.nim, .lua, .html) | N/A (files for user) | N/A |
| **WASM** | KIR | .wasm module | Canvas/WebGL/DOM | Browser/Native |

---

## Key Clarifications

### 1. HTML is a Codegen, NOT a Renderer
```bash
# HTML codegen (like any other)
kryon codegen html app.kir -o dist/

# With web-specific flags
kryon codegen html app.kir -o dist/ --minify --bundle
```
- Generates .html, .css, .js files
- Browser's DOM engine is the actual renderer
- HTML codegen just has more flags for web deployment

### 2. Native Target Uses IR Pipeline
```bash
kryon build app.kry --target=native --lang=c
# Internally:
# 1. app.kry → KIR (parser)
# 2. KIR → C source (codegen, possibly in-memory)
# 3. C source → binary (gcc)
```
- Always goes through KIR
- Can generate source in-memory (no file needed)
- Can use existing .kir file

### 3. .krb is Portable, Binary is Not
- **.krb**: Runs on any platform, any renderer
- **Binary**: Platform-specific (x86_64, ARM), renderer compiled in

### 4. WASM is Another Target
- Not part of "native" or "web codegen"
- Separate target with two compilation paths
- Can use native pipeline OR VM pipeline

---

## Build Command Examples

```bash
# VM target (portable)
kryon build app.tsx --target=krb -o app.krb
kryon run app.krb --renderer=sdl3

# Native target (platform-specific)
kryon build app.tsx --target=native --lang=c --renderer=sdl3 -o app
./app  # Runs directly

# Codegen (source files)
kryon codegen nim app.kir -o app.nim

# HTML codegen (web deployment)
kryon codegen html app.kir -o dist/ --minify --bundle

# WASM target
kryon build app.tsx --target=wasm --lang=c -o app.wasm
```

---

## Does This Match Your Vision?

**Key points confirmed:**
1. ✅ All frontends → KIR (universal pipeline)
2. ✅ VM target (.krb) runs on any renderer
3. ✅ Native target produces platform-specific binary
4. ✅ Still uses IR pipeline (KIR → Source → Binary)
5. ✅ HTML is just a codegen with extra flags
6. ✅ WASM is a separate target (via native or VM pipeline)
7. ✅ No special treatment for any codegen

**Is this correct? Should I update all plans with this architecture?**
