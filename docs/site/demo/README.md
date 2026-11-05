# Kryon live editor

`../demo.html` offers Source, Preview, and Both views. Both is the default on
wide screens; narrow screens and `?mini` embeds start with the interactive
preview. Opening source loads the browser compiler. Edits compile after a
350 ms pause, and Ctrl/Command + Enter compiles immediately. Compilation errors
keep the last working preview. Reset restores `src/app.zi`.

The compiler runs in a worker and uses Ziran's ordinary checker and portable
bundle writer. The compiled app runs in a sandboxed iframe with a persistent
VM instance. `src/preview_host.zi` binds raw input, font measurements, and
raster effects to the existing Kryon Canvas host; widget behavior comes from
the ordinary Kryon library. Source stays in the browser. Programs must fit
Ziran's portable subset and the installed Canvas capabilities. Each VM frame
has a statement limit, and compilation and rendering have timeout recovery.

Build the normal initial preview with `build-queue -- ./build.sh`. For changes
in this checkout, set the ignored `ziran.local.toml` override `kryon` to this
repository root. The source build resolves that override with `ziran lock`
before compiling; production locks retain actual published revisions. To rebuild the editor's
compiler and Canvas host, activate Emscripten 6.0.6 and run:

```sh
python3 build_preview.py
```

The script resolves the configured Ziran and Kryon sources with `ziran pkg
path`, including the demo's ignored local overrides. Use a Ziran revision
that provides `BuildBundle` and `BundleInstanceLimitSteps`. The three generated
editor assets in `../assets/` are committed with the static site, alongside
the original `widgets.html` preview.

From the Kryon repository root, `build-queue -- make site-browser-test`
runs the Python browser suite in a private headless Chromium. It covers the
app and module catalogs, responsive navigation, themes, keyboard focus,
legacy routes, and real editor compilation, including rotated text. To retain
review images, run `build-queue -- python3 tests/site_browser_test.py
--screenshots backup/labs-site/verification` after confirming `backup/` is ignored.

From the Kryon repository root, `node tests/demo_browser.mjs` starts a local
server and private Xvfb/Chromium display. It checks real compilation, rendered
pixels, persistent button state, error recovery, source escaping, cancellation,
view switching, reset, responsive layout, and lazy loading in embeds. Its
screenshots and logs live in `build/scratch/web-preview/`.
Pass a deployed editor URL to run the same checks against a live site:

```sh
node tests/demo_browser.mjs https://kryonlabs.com/demo.html
```
