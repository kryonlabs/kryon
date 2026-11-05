# Product previews

These images show real applications and modules. App captures use synthetic
fixtures on a private display; they contain no owner account or repository data.
They describe development builds, not released downloads.

- `terminal.webp`: Terminal with sample shell output, captured on private Xvfb.
- `browser.webp`: Browser rendering a local Field notes sample page.
- `github-overview.webp` and `github-repositories.webp`: GitHub client using
  fixture repositories and accounts on private Xvfb.
- `kryon-gallery.webp`: a selection of widgets from the real browser gallery.
- `plot-candles.webp`: the gallery CandleChart example (`plot/4`), with sample data.
- `data-views-table.webp`: the gallery TableView example (`data_views/2`), with sample data.
- `kss-packs.webp`: the same fixture interface with the real KSS style packs.
- `game2d-controls.webp`: the Game2D control chooser, compiled and rendered in
  the private browser live editor.

The gallery and editor captures use an isolated headless Chromium with inherited
display and desktop-session variables removed. WebP files are binary exports of
those captures. The original app and module implementations own their interfaces;
these are not illustrated mockups.

The SVG icons in `mingcute/` are copied from the MingCute package's upstream
assets. Their Apache-2.0 license is retained as `mingcute/LICENSE.txt`.
