# Semantic DOM browser host

Select `backend = "dom"` in a Kryon project profile. The host uses the same
Ziran-to-C and Emscripten path as Canvas2D, then reconciles Kryon's committed
retained tree into real browser elements. Application code keeps the ordinary
`Frame(Session, Rectangle)` contract and does not import a DOM library.

The DOM adapter maps stable Kryon identity generations—not frame-local array
positions—to element identity. It emits semantic landmarks, headings, text,
links, buttons, text fields, text areas, trees, tabs, and options where those
widget facts are available. It also carries labels, heading levels, selected
state, disabled state, and loading state. Links retain their URL and images
retain their asset path and alt text in the semantic tree.

This first host is intentionally hybrid. Canvas2D remains responsible for the
existing high-fidelity paint queue, while the semantic DOM layer is transparent
and positioned above it. Native interactive elements receive pointer events;
those events bubble to the existing browser input path. This preserves shared
widget behavior while making the document inspectable. The next styling phase
will move supported presentation from inline geometry to KSS-generated CSS and
normal browser layout.

The DOM root is removed when the session closes. Kryon keeps a test-only
semantic snapshot after exit so private headless Chromium can assert exactly
what was mounted without retaining a live second UI tree. Browser project
generation keeps the full raster adapter set alongside this DOM host; it does
not prune modules by the application entry.

## Focused checks

```sh
python3 tests/dom_project_test.py
```

The test creates a real `ziran.toml` project, saves `.zir`, generates C, links
Emscripten, and loads the resulting page in a new headless Chromium process. It
verifies the saved host module, generated C host module, page output, semantic
tags, heading level, link URL, disabled button state, and stable parent
identity. It scrubs all desktop display variables and never contacts the
developer's desktop session.
