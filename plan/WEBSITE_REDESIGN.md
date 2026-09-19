# Kryon Labs website improvement plan

Date: 2026-09-19
Status: Planned; implementation has not started.
Site: https://kryonlabs.com/
Owner: `kryonlabs/kryon`, with website source in `docs/site/`.

## Outcome

Give Kryon Labs the warm, illustrated editorial style approved for Waozi and
Daochi, with a developer experience that makes it easy to understand Kryon,
build a first interface, inspect the API, and see what real applications use.

Keep Kryon as the main subject of the homepage. Present Daochi, TaijiOS, and
other projects as related work with their own destinations. Preserve the
existing Kryon mark and project names.

This document plans the work. It does not change the public site, compiler,
runtime, release version, or application behavior.

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

- [ ] Inventory all public routes, anchors, external links, generated files,
  and functional interactions before changing markup.
- [ ] Record which target claims are current, experimental, or paused, with
  their source revision and supporting release/test evidence.
- [ ] Select one runnable native quickstart and three maintained showcase apps.
- [ ] Run the current site build once and record any existing failures separately.

Completion: a verified content map and build baseline, with no unsupported
claims carried into the redesign.

### 2. Build the shared layout and homepage

- [ ] Consolidate `styles.css` into tokens, layout, components, documentation,
  and tool-specific rules. Update `theme.js` to use the same palette.
- [ ] Implement the common navigation, footer, focus states, and theme behavior.
- [ ] Generate the workshop artwork, preserve its original, and export an
  optimized WebP. Save the exact prompt with the design artifacts.
- [ ] Build the homepage and quickstart, including the real sample capture and
  featured project data.
- [ ] Review desktop and phone previews against the approved Waozi/Daochi style.
- [ ] Create and wire the matching social image after the visual direction is
  represented in the actual page.

Completion: the first-time visitor can understand Kryon and reach a working
first-app path; the homepage is useful without loading compiler WebAssembly.

### 3. Carry the design through documentation and tools

- [ ] Update Docs, Language, generated API, and Showcase consistently.
- [ ] Update Playground and the older examples route without losing tool state.
- [ ] Update renderer, feature, and benchmark tables while preserving data.
- [ ] Check theme persistence, active navigation, error states, and old links.

Completion: all public pages share a coherent design and existing developer
workflows still function.

### 4. Validate and publish the implemented site

- [ ] Run `make docs-site CMAKE=/usr/bin/cmake SITE_BUILD_DIR=build/site` with
  its documented compiler/build prerequisites, then inspect the generated site.
- [ ] Check 390 px, 768 px, and 1440 px layouts in light and dark modes. Code
  and matrices may scroll internally; the document itself must not overflow.
- [ ] Verify mobile navigation, keyboard tabs, theme controls, focus visibility,
  skip links, heading order, contrast, reduced motion, and alt text.
- [ ] Load a Playground example, edit it, run it, inspect each supported output,
  and exercise a compile error and failed asset load. User edits must survive.
- [ ] Check showcase filters and missing-data behavior; check docs/API anchors
  and representative old example links.
- [ ] Check canonical URLs, unique titles/descriptions, social image URLs,
  image dimensions, font licenses, and local asset paths.
- [ ] Aim for at most 250 KB per optimized editorial illustration and 150 KB
  total for required homepage fonts. Lazy-load below-fold screenshots; do not
  preload compiler assets on marketing pages. Measure actual transfer sizes.
- [ ] Publish through the existing Cloudflare Pages workflow after successful
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
