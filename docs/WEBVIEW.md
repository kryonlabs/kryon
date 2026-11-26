# Web view

The optional web view runs one approved staged page in an owned WebKit child
beside the application window. The application never touches URLs, scripts or
the network: it names a staged page, moves a rectangle, sends bounded JSON
commands and receives bounded JSON events. The child is a literal argv spawn
with parent-death supervision, an AF_UNIX SEQPACKET socket pair and a pidfd,
so a closed player always reaps it. There is no listening port; the island
serves its staged assets from a loopback server bound per process.

## Application API (`kryon/WebView`)

- `Open(content, x, y, width, height) -> bool` spawns the island for one
  staged page name (lowercase letters and dashes, at most 64 bytes) and asks
  for its first bounds.
- `Bounds(x, y, width, height, visible)` moves or hides the island rectangle
  in host window coordinates; unchanged rectangles are ignored.
- `Send(command) -> bool` delivers one JSON command packet (4 KiB limit).
- `Event() -> string` returns one queued event packet, or empty.
- `Close()` terminates and reaps the child.

The capability is Linux-only today; a program imports it as `kryon/WebView`
and the player's `Supported` check grants or refuses the foreign functions,
so other hosts report it as unsupported instead of failing.

## Island protocol (v1)

Every packet is a JSON object with a version field set to 1. Host commands
name a `type` in the shared vocabulary (`spin`, `state`, `focus`, `seek`,
`play`, `pause`, `select`, `next`, `previous`, `stop`, `volume`) and may
carry an `id`. Page events are `ready`, `ack` (echoing `id` and `command`
with the page's `state`), `state`, `click`, `error` and `frame`. `focus`
carries `active` and drives the shared visibility lifecycle, including the
page's `document.hidden`.

## Staged pages

Assets live beside the player executable in `webview-assets/` (or where the
`KRYON_WEBVIEW_ASSETS` environment variable names). `make zx-player` stages
`kryon-island.py` and `assets/webview/` (`bridge.js`, `island.css`, and the
demo `scene.html` with `scene.js`). A page loads `/bridge.js`, installs
`window.KryonPage` with `receive(command)` and `state(active)`, then reports
`window.KryonIsland.ready(state)` once its handlers exist; commands sent
before that wait in the island. The island serves only files inside the
staged directory, refuses dotfiles and directory listings, sends restrictive
content headers, and answers range requests so pages can stream media.

## Checks

`make webview-island-test` builds the probe bundle against this checkout and
runs it on a private Xvfb display: the island child spawns, maps its window
into the player, reports ready, answers a spin command with an ack reporting
rendered frames, and is reaped on close. It skips itself without WebKitGTK.
