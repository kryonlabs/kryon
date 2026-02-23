Kryon Specification

A portable C library that implements a synthetic UI filesystem using the
9P2000 protocol.

OVERVIEW

Kryon operates in two modes:

    CPU Server Mode    - Linux/Android/BSD with TCP listener
    Native Mode         - 9front with kernel pipes

The same portable C library core works in both modes.  Any language that
speaks 9P can build UIs.

Architecture:

    ┌─────────────────────────────────────────────────────┐
    │         kryon — Portable C Library Core            │
    │  • 9P2000 State Machine  • Synthetic File Tree    │
    │  • TCP CPU Server Module  • Event Dispatcher      │
    └───────────────┬─────────────────────────┬───────────┘
                    │                         │
                    │ CPU Server Mode        │ Native Mode
                    │ (Linux/Android/BSD)    │ (9front)
                    ↓                         ↓
    ┌───────────────────────┐   ┌───────────────────────────┐
    │  TCP Listener (17019) │   │  Kernel Pipes + libdraw   │
    └───────────┬───────────┘   └───────────┬───────────────┘
                │                           │
                │ 9P/TCP                    │ 9P (kernel pipes)
                ↓                           ↓
        ┌───────────────┐           ┌───────────────────┐
        │   Drawterm    │           │   9front rio      │
        │  (Any Client) │           │  Native Windows   │
        └───────────────┘           └───────────────────┘

    Controller Layer (Any Language):
      C / Lua / Scheme / Python / rc

QUICK START

CPU Server Mode:

    gcc -o kryon kryon.c
    ./kryon --cpu-server --port 17019
    drawterm -h host -a 17019

Native Mode (9front):

    9kry -m /mnt/kryon &
    lua controller.lua

Direct 9P usage:

    echo create_window > /mnt/kryon/ctl
    echo "My App" > /mnt/kryon/windows/1/title
    echo "100 100 800 600" > /mnt/kryon/windows/1/rect
    echo "1" > /mnt/kryon/windows/1/visible
    echo create_widget 1 button 0 > /mnt/kryon/ctl
    echo "Click Me" > /mnt/kryon/windows/1/widgets/1/text

DOCUMENTATION

See doc/ directory for complete specification:

    doc/0 intro       - Overview and architecture
    doc/1 protocol    - 9P protocol and filesystem layout
    doc/2 widgets     - Widget catalog (89 widgets)
    doc/3 layout      - .kryon layout format syntax
    doc/4 examples    - Example applications
    doc/5 api         - Controller API (Lua, C, Python, rc)

EXAMPLES

examples/ directory contains:

    01-hello-world.kryon    - Simple window
    02-counter.kryon        - State and events
    03-form.kryon           - Form validation

WIDGETS

Kryon includes 89 widgets covering:

    Input (24)      - Text, selection, date/time, file inputs
    Buttons (8)     - Various button types
    Display (11)    - Labels, images, progress bars
    Containers (11) - Boxes, frames, dialogs, scroll areas
    Lists (8)       - List items, grids, trees
    Navigation (8)  - Menu bars, tool bars, nav rails
    Menus (4)       - Dropdown, context, popup menus
    Forms (4)       - Form groups, validation
    Windows (4)     - Window types
    Specialized (8) - Canvas, SVG, charts, color picker

CONTRIBUTING

Follow the specification in doc/.  Maintain compatibility with existing
controllers.  Document platform-specific differences.
