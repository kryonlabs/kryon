# Changelog
## v0.1.6 - 2026-07-25

### Changed

- Bump version to v0.1.6
- Resolve tray icon from installed hicolor paths
- Fix site card cursors, release CI, and IDE recent-projects grid
- Trigger site deploy (previous run hit GitHub internal error)
- Restyle docs site with refined modern-retro theme
- Add Kryon IDE start page and fix preview inspection
- Improve Kry language and generic APIs
- Support uninitialized C locals in kc
- Allow args locals in kc functions
- Add Kryon platform thread primitives
- Support local enum blocks in kc
- Support bitwise compound assignments in kc
- Guard Kry compile-time expansion recursion
- Add runtime asset sync API
- Support Kry multidimensional arrays
- Raise Kry type block capacity
- Add Kry named enum syntax
- Add Kry switch syntax
- Update Kryon site goals
- Add Kry web intrinsics
- Add Kry anonymous block scopes
- Add top-level Kry extern declarations
- Add Jai-style Kry defines
- Add Kry top-level macro translation
- Add Kry type aliases
- Allow Kry type-only modules
- Generate project headers from Kry files
- Guard Kry text shorthand parsing
- Respect Kry locals when resolving function values
- Limit Kry function value rewriting
- Resolve Kry function values in expressions
- Add exact C exports for Kry modules
- Make Kry modules the only import path
- Support path-based Kry module use
- Fix IDE preview toolbar and text state
- Tighten leading continuation parsing
- Support leading expression continuations in Kry
- Add native Kry enum blocks
- Support break statements in Kry
- Add native Kry struct declarations
- Support multiline Kry function declarations
- Rewrite nil in Kry static initializers
- Add Jai-style Kry compile-time macros
- Support multiline Kry state initializers
- Use Jai-style typed Kry declarations
- Keep postfix operators on one Kry line
- Support multiline Kry block headers
- Support operator line continuations in Kry
- Support multiline Kry statements
- Add Kry while statements
- Allow app locals in Kry functions
- Lift Kry function limits
- Add flat Kry modules


## v0.1.5 - 2026-07-21

### Added

- Add the Kryon logo as the site and IDE app icon.
- Add C-close Kry language direction docs.
- Add automatic release tagging when the checked-in Kryon version changes.

### Changed

- Replace Kry `include` syntax with explicit `cimport` for C headers.
- Use cleaner changelog version headings without brackets.

### Fixed

- Allocate Kry compiler state on the heap to avoid Linux runner stack crashes.

## v0.1.4 - 2026-07-18

### Changed

- Add Clay as a vendored layout dependency for future Kryon UI work.
- Remove the vendored fontchop dependency and chopped bitmap font assets.

## v0.1.3 - 2026-07-16

### Changed

- Add public Kryon version metadata for release artifacts.
- Move font atlas generation to the vendored fontchop submodule.
- Replace the single implicit UI font with a registered font selection API.
- Improve generated icon asset formatting and desktop/embedded fallbacks.

### Added

- Add Markdown support and text area UI.
- Add pointer release helper APIs and modal layer input capture.

## v0.1.2 - 2026-07-12

### Added

- Add web file dialog loading.

## v0.1.1 - 2026-07-12

### Fixed

- Fix release CI vendor builds.

## v0.1.0 - 2026-07-12

### Added

- Add Kryon release automation.
