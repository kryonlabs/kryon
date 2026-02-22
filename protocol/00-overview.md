# Kryon Protocol Overview

## Table of Contents

1. [Design Philosophy](#design-philosophy)
2. [Architecture](#architecture)
3. [Multi-Window Support](#multi-window-support)
4. [Concurrency Model](#concurrency-model)
5. [Security Model](#security-model)
6. [Platform Targets](#platform-targets)

---

## Design Philosophy

Kryon is built on the principle that **UI state is filesystem state**. By exposing widgets through the 9P protocol, we achieve:

- **Language Agnostic**: Any language that can speak 9P can build UIs
- **Platform Agnostic**: Same protocol works on 9front, MirageOS, and beyond
- **Transparent State**: All widget state visible and manipulatable as files
- **Compositional**: Widgets nest naturally like directories
- **Debuggable**: Inspect UI state with standard filesystem tools (`ls`, `cat`, `echo`)

### The 9P Advantage

9P is fundamentally simple:
- **Messages**: Read, write, create, remove, walk (navigate), stat (metadata)
- **Resources**: Represented as a hierarchical filesystem
- **Transport**: Agnostic (works over pipes, network, shared memory)
- **State**: Either in kernel (files) or userspace (synthetic filesystems)

This simplicity makes 9P the perfect abstraction for UI state.

---

## Architecture

### View/Model/Controller Separation

```
┌─────────────────────────────────────────────────────────────┐
│                        Controller                            │
│                   (Application Logic)                        │
│                                                              │
│  - Lua/Scheme scripts on 9front                             │
│  - Embedded Lua/Scheme in MirageOS                          │
│  - Reads/writes to 9P filesystem to manipulate UI state     │
└────────────────────┬────────────────────────────────────────┘
                     │ 9P Operations
                     │ (read, write, walk)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                          Model                               │
│                     (9P Filesystem)                          │
│                                                              │
│  /windows/1/widgets/5/text  →  "Hello, World"              │
│  /windows/1/widgets/5/value →  "42"                        │
│  /windows/1/widgets/5/event →  "clicked x=150 y=32"        │
│                                                              │
│  - Hosted by kryonsrv (C/9front) or mirage-9p (OCaml)       │
│  - Represents complete UI state                             │
└────────────────────┬────────────────────────────────────────┘
                     │ State Changes
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                          View                                │
│                    (Renderer/Display)                        │
│                                                              │
│  - kryonrender on 9front (libdraw)                          │
│  - mirage-framebuffer on MirageOS                           │
│  - Mounts 9P server, watches for changes, renders widgets   │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

1. **User Interaction** → View generates event → written to event file
2. **Controller** reads event file → processes logic → writes to property files
3. **Model** (9P server) validates writes → updates state → notifies watchers
4. **View** sees state change → re-renders affected widgets

### Event Loop

```
┌────────────────┐
│  Controller    │
│  Event Loop    │
└───────┬────────┘
        │
        │ 1. Open /events (blocking read)
        │
        ▼
┌─────────────────────┐     ┌──────────────────┐
│  Wait for Event     │────▶│  Event Received  │
└─────────────────────┘     └────────┬─────────┘
                                     │
        ┌────────────────────────────┘
        │
        ▼
┌─────────────────────┐
│  Parse Event        │  "clicked widget=5 button=1"
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│  Execute Handler    │  Update state, call functions
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│  Write Properties   │  echo "new value" > /windows/1/widgets/5/text
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│  Loop Back          │
└─────────────────────┘
```

---

## Multi-Window Support

Kryon supports unlimited windows from the ground up. Each window is an independent widget tree rooted under `/windows/<id>/`.

### Window Isolation

- **Independent State**: Each window has separate widget trees
- **Independent Events**: Each window has its own event pipe
- **Shared Resources**: Themes, fonts, and assets can be shared

### Window Hierarchy

```
/windows/
├── 1/                    # Main window
│   ├── title → "Main App"
│   ├── rect → "0 0 800 600"
│   └── widgets/
│       ├── 1/           # Button
│       └── 2/           # Text field
│
├── 2/                    # Preferences dialog
│   ├── title → "Preferences"
│   ├── state → "normal|minimized|maximized|fullscreen"
│   ├── modal → "1"
│   └── widgets/
│
└── 3/                    # About dialog
    └── ...
```

---

## Concurrency Model

### Thread Safety

- **9P Server**: Single-threaded event loop (simplifies synchronization)
- **Renderer**: May run in separate process (9front) or thread (MirageOS)
- **Controller**: Multiple controllers can connect simultaneously

### FID Lifecycle

9P uses FIDs (file IDs) to track open resources:

```
1. Tattach     →  Client connects to root
2. Twalk       →  Client navigates to path
3. Topen/Tcreate →  Client opens file or directory
4. Tread/Twrite →  Client reads/writes
5. Tclunk      →  Client releases FID
```

### Lock Ordering

To prevent deadlocks:

1. Always acquire locks in order: root → window → widget → property
2. Never hold a property lock while making 9P call
3. Event pipes use non-blocking writes with backpressure handling

### Atomic Operations

- **Property writes**: Atomic (single write syscall)
- **Widget creation**: Atomic (create_widget command creates entire widget)
- **Multi-property updates**: Not atomic; use transactions (future)

---

## Security Model

### Access Control

9P supports Unix-style permissions:

```
Mode bits:  rwxrwxrwx
           ───┬──┬┬┬
              │ │ └── Others (all clients)
              │ └──── Group (optional groups)
              └────── User (owner)
```

### Default Permissions

| Path              | Permissions | Description                    |
|-------------------|-------------|--------------------------------|
| `/version`        | `0444`      | Read-only version string       |
| `/ctl`            | `0222`      | Write-only command pipe        |
| `/events`         | `0444`      | Read-only global events        |
| `/windows/`       | `0755`      | Listable window directory      |
| `/windows/<id>/`  | `0755`      | Window directory               |
| `/windows/*/rect` | `0644`      | Read/write properties          |
| `/windows/*/event`| `0444`      | Read-only event pipes          |

### Threat Model

**Trusted Environment**: Kryon assumes:
- Single-user system (no untrusted users)
- Controller and renderer run as same user
- 9P server runs locally (not exposed to network)

**Future Hardening**:
- Authentication via Tauth
- Per-widget permissions
- Sandboxed controllers

---

## Platform Targets

### 9front (Native/Desktop)

- **9P Server**: `lib9p` (C)
- **Rendering**: `libdraw` → `/dev/draw`
- **Scripting**: Lua (native port), Scheme (scsh, chibi-scheme)
- **Concurrency**: Processes with 9P communication
- **Windowing**: Multiple `libdraw` Image structures

### MirageOS (Unikernel/Cloud)

- **9P Server**: `mirage-9p` (OCaml)
- **Rendering**: `mirage-framebuffer` → VESA/VGA buffer
- **Scripting**: `lua-ml` (OCaml Lua VM) or embedded Scheme
- **Concurrency**: Lwt cooperative threads
- **Windowing**: Virtual screens or multiple framebuffer contexts

### Future Targets

- **Web**: 9P over WebSockets, Canvas/WebGL rendering
- **Wayland**: 9P server as Wayland client
- **Windows**: 9P server with GDI/Direct2D rendering
- **macOS**: 9P server with Core Graphics rendering

---

## Protocol Documents

- [9P Specification](01-9p-specification.md) - Complete wire protocol
- [Filesystem Namespace](02-filesystem-namespace.md) - Directory structure
- [Permissions](03-permissions.md) - Security model
- [Error Handling](04-error-handling.md) - Error codes and handling
- [Multi-Window Architecture](05-multi-window.md) - Window management
