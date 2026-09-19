# Kryon Labs website improvement plan

Date: 2026-09-19
Status: Implemented, published, and verified at https://kryonlabs.com/.
Site: https://kryonlabs.com/
Owner: `kryonlabs/kryon`, with website source in `docs/site/`.

## Outcome

Give Kryon Labs the warm, illustrated editorial style approved for Waozi and
Daochi, with a developer experience that makes it easy to understand Kryon,
build a first interface, inspect the API, and see what real applications use.

Keep Kryon as the main subject of the homepage. Present Daochi, TaijiOS, and
other projects as related work with their own destinations. Preserve the
existing Kryon mark and project names.

The approved design is implemented in `docs/site/`. The compiler, runtime, and
release versions remain outside this website change.

## Findings from the current site

Reviewed the public homepage and local website source on the date above.
These are observations to address, rather than a claim that the redesigned
site has already been implemented or tested.

| Finding | Evidence | Planned response |
| --- | --- | --- |
| The opening explains the stack but gives little guidance on the first useful thing to build. | `docs/site/index.html`: short product introduction, technical pills, docs and showcase links. | Add a benefit-led introduction, a direct quickstart, and one small working example. |
| Real apps are a separate destination; the homepage's showcase section contains links rather than app previews. | `index.html`, `showcase.html`, `showcase-data.json`. | Bring three selected apps onto the homepage using the existing showcase data and real captures. |
| The stylesheet contains successive paper, dark technical, and crayon theme layers. | Multiple root palettes and redesign blocks in `styles.css`. | Consolidate the cascade into shared tokens and explicit component styles. |
| Documentation uses a different navigation arrangement and omits the shared theme script. | `docs.html` compared with `index.html` and `language.html`. | Use the same header, active-page state, theme behavior, and footer throughout. |
| Broad homepage claims about web targets need reconciliation with current support policy. | `index.html`, `ide.html`, `docs/WEB_JS_ROADMAP.md`, `plan/COMPLETION.md`. | Verify target claims against release gates; distinguish the paused JS target from the browser-hosted compiler and KRB preview. |
| The homepage declares a large social card but does not provide a social image. | Open Graph and Twitter metadata in `index.html`. | Add a branded sharing image as part of the redesign and validate page metadata. |
| The site build includes generated API docs, showcase data, compiler assets, and examples. | `Makefile` target `docs-site`, `.github/workflows/pages.yml`. | Preserve this build and validate the generated output, including functional tools. |

## Visual direction

- Warm ivory paper `#f7f3eb`, dark ink `#202c29`, forest green `#315e48`,
  restrained terracotta `#a64e32`, and fine neutral dividers.
- Newsreader for editorial headings, Manrope for navigation and body copy,
  and a readable monospace face for code. Serve fonts locally with licenses.
- Follow the spacing, project rows, and quiet illustration style at
  [Waozi](https://waozi.xyz/) and [Daochi](https://daochi.net/).
- Keep the saved light/dark preference working. Use the paper palette for
  light mode and a restrained ink palette for dark mode; test both. Avoid
  layering another theme on top of the old overrides.
- Use one original ink-and-watercolor illustration: a small workshop beneath
  pines, on warm paper, with a muted terracotta sun. It should convey making
  useful things and remain secondary to the introduction and primary action.
- Keep artwork in its own layout area, with intrinsic dimensions and soft
  edges. Verify its framing at phone and desktop widths before integration.
- Use actual application and runtime captures for product evidence. Label
  conceptual illustrations clearly; never present generated art as a screenshot.
- Keep documentation compact and readable: restrained headings, clear links,
  comfortable code blocks, and tables that scroll within their own containers.

## Homepage structure

| Order | Section | Content and action |
| --- | --- | --- |
| 1 | Shared header | Kryon mark; Get started, Docs, Showcase, Playground, and Source. Secondary documentation destinations remain easy to reach from Docs. |
| 2 | Introduction | Proposed headline: **Small foundations. Useful software.** Explain the language and runtime in two short sentences. Primary action: **Build your first interface**; secondary: **See the apps**. |
| 3 | A first interface | One minimal, verified `.kry` example beside its real rendered result. Link to the same example in the quickstart and Playground. |
| 4 | Three ways to explore | **Write an interface**, **Use the runtime**, **Choose a renderer**. Each gets a plain explanation, a small visual, and a direct documentation link. |
| 5 | Built with Kryon | Three maintained apps selected from the showcase registry. Real screenshots, one-sentence purpose, platform information, app/site link, and source link. |
| 6 | Where it runs | A compact overview of supported, experimental, and paused targets, with links to evidence and the detailed matrices. |
| 7 | Related projects | Daochi and TaijiOS with matching editorial rows and links to their own sites. Keep their relationship to Kryon explicit. |
| 8 | Footer | Source, releases, contribution guide, documentation, community, Waozi, and related projects. |

The first screen should show the purpose and primary action at common desktop
and phone sizes. Avoid a full-height opening that hides the useful content.

## Work by page

| Surface | Improvements | Preserve |
| --- | --- | --- |
| `index.html` | New introduction, illustration, verified example, selected app previews, accurate target summary. | Kryon identity, useful external destinations, and existing section deep links where practical. |
| New `getting-started.html` | Prerequisites, installation, a minimal app, exact build/run commands, expected output, and the next useful step. Start with one verified native path. | Link to existing build documentation for additional hosts; commands must match the current release. |
| `docs.html` | A visible beginner path, clear task-based categories, consistent navigation, and links to examples and support status. | Existing documentation tabs and deep links. |
| `language.html` | Improve section navigation, examples, copy-code controls, and explanations of unsupported forms. | Correct language semantics and current compiler boundaries. |
| `api-template.html` and generated `api.html` | Consistent styling, accessible contents navigation, readable signatures and code, and heading anchors. Add lightweight local filtering if needed for the generated reference. | Generate from `docs/API.md` through `scripts/render-api-html.sh`; retain existing API anchors. |
| `showcase.html` | Matching project rows/cards, useful descriptions, screenshot sizing, direct actions, and clear empty/error states. | Featured/first-party/community filters, real registry data, and the contribution workflow. |
| `ide.html`, `ide.js` | Restyle the tool shell, improve pane sizing on phones, and make loading, compiling, success, and error states clear. Preserve edits when switching views or encountering errors. | Source editor, examples, active artifact tabs, compiler assets, and supported KRB preview. |
| `live-examples.html`, `live-examples.js` | Keep older example links working and guide visitors to the appropriate Playground example. | Existing examples and manifests. |
| `renderers.html`, `matrices.html` | Clear status labels, legends, last-generated information, links to evidence, sticky table headers where useful, and contained horizontal scrolling. | Generated matrix data and honest distinctions between coverage and full support. |
| `benchmarks.html` | Readable comparisons with date, revision, host, units, and methodology when available. Explain missing data rather than implying a result. | Real benchmark output; no invented numbers. |

## Accuracy and content requirements

- Recheck `docs/WEB_JS_ROADMAP.md`, `plan/COMPLETION.md`, the release workflow,
  and current generated matrices before writing target claims. Code existing
  in the repository is not evidence of supported release status.
- A compiler running inside a browser is distinct from a supported JavaScript
  application target. Explain this distinction near the Playground controls.
- Verify C, C++, and Go tool availability independently. Do not describe a
  compiler output as supported in the Playground unless its shipped tool and
  UI are actually wired there.
- Compile the exact sample shown on the homepage. Capture its result through
  Kryon's own preview/capture tooling. Use canonical `Text(TextProps)` and
  `Image(ImageProps)` APIs when those widgets are present.
- Use the showcase registry as the source of project metadata. Change registry
  content in its owning `kryonlabs/showcase` repository when necessary, rather
  than hand-editing generated output or maintaining competing lists.
- Keep fresh source/release links. Display GitHub stars only when real data is
  available; the primary presentation should explain what the project does.

## Implementation sequence

### 1. Establish the content and build baseline

- [x] Inventory all public routes, anchors, external links, generated files,
  and functional interactions before changing markup.
- [x] Record which target claims are current, experimental, or paused, with
  their source revision and supporting release/test evidence.
- [x] Select one runnable native quickstart and three maintained showcase apps.
- [x] Run the current site build once and record any existing failures separately.

Completion: a verified content map and build baseline, with no unsupported
claims carried into the redesign.

### 2. Build the shared layout and homepage

- [x] Consolidate `styles.css` into tokens, layout, components, documentation,
  and tool-specific rules. Update `theme.js` to use the same palette.
- [x] Implement the common navigation, footer, focus states, and theme behavior.
- [x] Generate the workshop artwork, preserve its original, and export an
  optimized WebP. Save the exact prompt with the design artifacts.
- [x] Build the homepage and quickstart, including the real sample capture and
  featured project data.
- [x] Review desktop and phone previews against the approved Waozi/Daochi style.
- [x] Create and wire the matching social image after the visual direction is
  represented in the actual page.

Completion: the first-time visitor can understand Kryon and reach a working
first-app path; the homepage is useful without loading compiler WebAssembly.

### 3. Carry the design through documentation and tools

- [x] Update Docs, Language, generated API, and Showcase consistently.
- [x] Update Playground and the older examples route without losing tool state.
- [x] Update renderer, feature, and benchmark tables while preserving data.
- [x] Check theme persistence, active navigation, error states, and old links.

Completion: all public pages share a coherent design and existing developer
workflows still function.

### 4. Validate and publish the implemented site

- [x] Run `make docs-site CMAKE=/usr/bin/cmake SITE_BUILD_DIR=build/site` with
  its documented compiler/build prerequisites, then inspect the generated site.
- [x] Check 390 px, 768 px, and 1440 px layouts in light and dark modes. Code
  and matrices may scroll internally; the document itself must not overflow.
- [x] Verify mobile navigation, keyboard tabs, theme controls, focus visibility,
  skip links, heading order, contrast, reduced motion, and alt text.
- [x] Load a Playground example, edit it, run it, inspect each supported output,
  and exercise a compile error and failed asset load. User edits must survive.
- [x] Check showcase filters and missing-data behavior; check docs/API anchors
  and representative old example links.
- [x] Check canonical URLs, unique titles/descriptions, social image URLs,
  image dimensions, font licenses, and local asset paths.
- [x] Aim for at most 250 KB per optimized editorial illustration and 150 KB
  total for required homepage fonts. Lazy-load below-fold screenshots; do not
  preload compiler assets on marketing pages. Measure actual transfer sizes.
- [x] Publish through the existing Cloudflare Pages workflow after successful
  validation; confirm the production commit, routes, images, and core tools.

Completion: successful deployment and a short record of the checks, screenshots,
measured asset sizes, and any remaining known limitations.

## Files and delivery boundaries

Primary changes belong in `docs/site/`. Add artwork and licensed fonts beneath
`docs/site/assets/`; the existing site copy step should carry these to output.
Touch `Makefile`, site generation scripts, or `.github/workflows/pages.yml`
only when needed to support the new page/data or correct a proven build issue.
Use the current static architecture and deployment rather than a new framework.

Keep this work in the canonical Kryon repository on `master`. Stage only website
and plan changes; preserve concurrent runtime/compiler work. If an example
reveals a runtime defect, track and fix it as a distinct upstream change with
the relevant tests. Never modify a downstream vendor copy for site work.

## References

- Current public site: https://kryonlabs.com/
- Approved visual direction: https://waozi.xyz/ and https://daochi.net/
- [Website source](../docs/site/index.html)
- [Site build](../Makefile) and [deployment workflow](../.github/workflows/pages.yml)
- [Web target roadmap](../docs/WEB_JS_ROADMAP.md)
- [Current completion and support notes](COMPLETION.md)
- [Showcase generator](../scripts/update-showcase.py)
- [Showcase registry](https://github.com/kryonlabs/showcase)

## Implementation and validation record — 2026-09-19

- Rebuilt all ten public content/tool pages, with the older examples route
  forwarding its example and artifact parameters to the Playground.
- Generated the approved workshop artwork and a matching social card; exact
  prompts and font licenses are in `docs/site/assets/editorial/`. The original
  generated images are retained with the approved design artifacts.
- Homepage app metadata comes from the existing showcase registry. The example
  image is a real Kryon-owned KRB capture of the downloadable `hello.kry`.
- Verified the exact sample with native C generation, KRB compilation, and the
  SDL native host capture. The quickstart explicitly starts with the KRB subset.
- Built the site in an isolated checkout, including all five JavaScript/WASM
  tool pairs. The first baseline build lacked the raylib header submodule;
  initializing the pinned dependency resolved it without source changes.
- Checked all ten content/tool pages at 390, 768, and 1440 pixels in both themes:
  60 combinations, no document overflow, one primary heading, valid skip link.
- Checked Docs keyboard tabs, showcase filters and missing data, API contents
  filtering and anchors, matrix filtering, mobile menu/Escape, reduced motion,
  theme persistence, compiler outputs, error diagnostics, draft recovery, failed
  compiler downloads, and the older example/output-tab links.
- Corrected the Playground's theme-color decoding (signed bit comparison made
  text invisible), obsolete default sample, incomplete output count, and draft
  loss when switching examples. These are website-tool changes, not runtime APIs.
- Checked generated API links under the Markdown renderer, including stable
  heading IDs, older style-section links, and links to repository documents.
- Validated local assets, internal links/fragments, metadata, scripts, and
  unique IDs. Text palette contrast on the page background is at least 5.05:1
  in light mode and 7.73:1 in dark mode.
- Hero WebP: 210,322 bytes. Both required fonts together: 82,104 bytes.
  The homepage does not load compiler modules; app images load lazily.
- Benchmark data, coverage evidence, paused JS-target status, and KRB subset
  limits remain explicit. No fresh benchmark numbers or broader target support
  are claimed by this redesign.

Local design review artifacts: `waozi-design-proposals/2026-09-19/kryonlabs/`.
Publishing uses the existing `Cloudflare Pages` workflow and project `kryon`.

### Production verification

Published website commit: `667a36f0490c941e1d38bbba04ca6465b98b4704`.
[Successful build and production deployment](https://github.com/kryonlabs/kryon/actions/runs/35451762905).

Verified all ten production routes and their updated metadata, workshop image,
social card, example capture, downloadable source, and compiler binaries.
The public homepage loads the three selected registry projects. The live
Playground compiles the exact homepage example into KIR, C, Go, and KRB, and
its greeting is visible in the preview.

Final desktop and Playground production captures are retained beside the
approved proposal under `kryonlabs/implementation/`.
