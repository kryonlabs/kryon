# Kryon: Universal 9P UI Blueprint

Kryon is a platform-agnostic UI framework where the **View** (Pixels), the **Model** (State), and the **Controller** (Logic) are decoupled via the 9P protocol. This allows the same application logic to run natively on 9front or as a standalone MirageOS unikernel.

---

## 1. Core Architecture: The 9P Bridge

The "Source of Truth" for any Kryon app is a 9P filesystem tree. Whether hosted in OCaml (MirageOS) or C (9front), the hierarchy is identical.

### File Schema Specification
- `/ctl`: Write-only command pipe (e.g., `add button submit`).
- `/events`: Read-only blocking pipe for global events.
- `/[widget_id]/`:
    - `type`: (Read-only) e.g., `button`, `label`, `textinput`.
    - `rect`: (Read/Write) Coordinates as `x0 y0 x1 y1`.
    - `text`: (Read/Write) The string content of the widget.
    - `value`: (Read/Write) Numeric or boolean state (0/1).
    - `event`: (Read-only) Blocking pipe for widget-specific events (e.g., `clicked`).

---

## 2. Platform Implementations

### A. The 9front Target (Native/Desktop)
* **The Service (`kryonsrv`):** A C program that parses a `.kryon` layout file and hosts the 9P server at `/srv/kryon`.
* **The Renderer (`kryonrender`):** A C program using `libdraw` that mounts the 9P server and translates the file states into pixels on `/dev/draw`.
* **The Logic:** Any language available on 9front (RC, C, Go, Lua) interacts with the UI by reading/writing to `/mnt/kryon`.

### B. The MirageOS Target (Appliance/Cloud)
* **The Core:** An OCaml unikernel using `ocaml-9p` to host the widget tree in-memory.
* **The Renderer:** A module using `mirage-framebuffer` to draw pixels directly to a VESA/VGA buffer (KVM/Xen).
* **The Logic:** An embedded Lua or Scheme interpreter linked into the unikernel binary.

---

## 3. Portable Logic Engines

To achieve "Write Once, Run Anywhere," Kryon utilizes embedded scripting.

### Lua (Primary)
- **9front:** Uses the native Lua port.
- **MirageOS:** Uses `lua-ml` (OCaml-based Lua) or C-bindings.
- **Role:** Handles event loops and state changes by interacting with the virtual 9P tree.

### Scheme (Alternative)
- **Engine:** Chibi-Scheme or Gambit.
- **Role:** Provides a functional approach to UI state management, mapping Lisp lists to the 9P widget tree.

---

## 4. The Unified Workflow

1.  **Layout (`app.kryon`):**
    ```text
    window "Main" 800 600
    button btn_sync "Sync Now" 10 10 100 40
    label lbl_status "Ready" 10 60 200 80
    ```

2.  **Logic (`logic.lua`):**
    ```lua
    while true do
        local ev = kryon.wait_event("btn_sync")
        if ev == "clicked" then
            kryon.set("lbl_status", "text", "Syncing...")
            -- App logic here
            kryon.set("lbl_status", "text", "Done!")
        end
    end
    ```

3.  **Deployment:**
    - **On 9front:** `kryonsrv app.kryon && lua logic.lua`
    - **On MirageOS:** `mirage configure -t virtio && make` (Logic and Layout are bundled).

---

## 5. Implementation Milestones

1.  **Spec:** Finalize the 9P file protocol (permissions, message types).
2.  **Renderer (C):** Build the 9front `libdraw` client that can "auto-sync" when files change.
3.  **Server (OCaml):** Build the MirageOS 9P provider.
4.  **Embed:** Integrate Lua/Scheme interpreters into the MirageOS unikernel.
5.  **Bridge:** Write the "Standard Library" for Lua/Scheme to abstract the file I/O into simple objects.
