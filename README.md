# Kryon

Kryon is a native UI library for [Ziran](https://ziran-lang.org/). Application
screens use typed TSX, backed by native Ziran widgets and application logic,
on the desktop, in a terminal, in a browser or on Android. Ziran owns the
language frontends, compiler, `.zir`, `.zx` bundles and portable runtime. Kryon
is an ordinary imported package, so a program carries only the widgets it uses.

## Start an application

Install the `ziran` command (`make install-user` in the Ziran checkout) and
the `kryon` command (`make install-user` here), then:

```sh
kryon new hello        # desktop app; --template tui|pages|web picks another
cd hello
kryon run              # default profile; kryon run web selects another
kryon run desktop --png build/shot.png --width 390 --height 844   # offscreen, any size
```

New to Kryon? [docs/CHEATSHEET.md](docs/CHEATSHEET.md) is one page with the app
shell, every widget, the layout recipe, theme tokens and a driver test.

`kryon build portable` exports one `build/<name>-portable.zx` that opens with
`ziran run` on any supported platform. Manifests, profiles and every `kryon`
command are documented in [docs/PROJECTS.md](docs/PROJECTS.md).

## Build and test this repository

```sh
make          # check modules, generate C/Go, build libkryon.a
make test     # headless behavior tests
```

The build uses the Ziran revision locked in `ziran.lock`. `make test-focus
TEST=name` gives a quick edit loop; experiments go in `build/scratch/`. The
widget gallery lives in `examples/gallery/`
([examples/README.md](examples/README.md)); `make site` packages the website
and gallery into `docs/site/`.

## Documentation

| Question | File |
| --- | --- |
| First app: shell, widgets, layout, tokens, screenshot, driver test | docs/CHEATSHEET.md |
| Widget APIs and drawing | docs/API.md |
| Architecture and boundaries | docs/ARCHITECTURE.md, docs/BOUNDARIES.md |
| Projects, hosts, web canvas | docs/PROJECTS.md, docs/BACKENDS.md, docs/canvas.md |
| Supported coverage | docs/FEATURE_MATRIX.md, docs/WIDGET_CONFORMANCE.md |
| Input components, overlays, navigation | docs/INPUT_COMPONENTS.md, docs/OVERLAYS.md, docs/navigation.md |
| Portable components; accessibility | docs/COMPONENTS.md, docs/ACCESSIBILITY.md |
| Theming, zoom, cursor, clipboard, focus, IME | docs/THEMING.md, docs/ZOOM.md, docs/CURSOR.md, docs/CLIPBOARD.md, docs/window-focus.md, docs/COMPOSITION_INPUT.md |
| Frame capture/replay; threading; performance | docs/FRAME_REPLAY.md, docs/THREADING.md, docs/PERFORMANCE.md |
| Open work | plans/open-work.md |

Charts, tables, trees, source coloring and style sheets are separate packages:
`ziran add https://github.com/kryonlabs/plot.git`,
`https://github.com/kryonlabs/data-views.git`,
`https://github.com/kryonlabs/css.git` or
`https://github.com/ziranlang/syntax.git`, imported as `plot/Plot`,
`data_views/TableView`, `data_views/TreeView`, `css/Css` or `syntax/Syntax`.
Kryon exports the building blocks they draw with; see
[widget packages](docs/WIDGET_PACKAGES.md#widget-packages). Ziran language and runtime gaps
are listed in Ziran's [implementation
status](https://github.com/ziranlang/ziran/blob/master/docs/IMPLEMENTATION_STATUS.md).

The optional GStreamer service (`kryon/Video`, `kryon/VideoLinux`) decodes
local video with synchronized audio; draw frames with `Image.Draw`. The app
owns playback controls and access checks, and an expiring lease revokes both
audio and decoded frames when authorization stops being renewed;
`make test-focus TEST=ziran_video_linux_test` checks it.

Commit Kryon changes upstream here first; applications pick them up by
updating their `ziran.lock`.
