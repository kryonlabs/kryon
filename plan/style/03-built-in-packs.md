# 03 - Built-In Style Packs

Status: implementation plan
Scope: define shipped style packs, their responsibilities, and how apps attach/switch them.

## Objective

Ship a small, high-quality style catalog:

- `material` is default.
- `tk` is compact and desktop/toolkit-like.
- `vanilla` preserves current Kryon personality as an explicit pack.
- `lightfield` is premium and glow-capable, never default.

There is no standalone built-in `glow` pack. Glow belongs under Lightfield as an option/variant/treatment.

## Pack List

### material

Purpose:

- default attached pack for new Kryon apps;
- cheap to render;
- clean Material-like controls;
- calm dark-first visual direction matching the Android reference;
- no glow, no blur-heavy volume, no premium Lightfield cost.

Visual qualities:

- charcoal canvas for dark mode;
- crisp panels;
- 6-10 px radii;
- thin dividers;
- lavender accent states;
- flat paint material;
- clear typography;
- no large decorative depth.

### tk

Purpose:

- dense desktop utility style;
- easy to recognize in style picker;
- useful for tooling, editors, inspectors, and compact apps.

Visual qualities:

- square-ish controls;
- hard borders;
- compact spacing;
- high information density;
- light toolkit colors by default;
- predictable hover/pressed/disabled states.

### vanilla

Purpose:

- preserve Kryon's current default styling as an explicit opt-in pack;
- avoid losing existing product personality while removing hidden defaults.

Rules:

- may look like the historical Kryon default;
- must still be ordinary KSS;
- must not be hardcoded into widget C/Go;
- must be switchable through `StylePicker`.

### lightfield

Purpose:

- premium translucent/depth style family;
- includes glow-capable treatment;
- opt-in because it is more rendering intensive.

Rules:

- never selected automatically as default;
- may use `MaterialLightfield`;
- may use glass, layered surfaces, glow-like effects;
- should expose glow as an option/variant once variant API lands;
- should remain one style family, not a separate `glow` peer pack.

## Registry Requirements

C registry:

- `src/ui/style_builtin_packs.c` registers exactly the built-in catalog.
- `RegisterBuiltInStylePacks()` selects `material`.
- `EnsureBuiltInStylePacks()` preserves the active pack when possible.
- `FindStylePack("glow")` must be `NULL` for shipped built-ins.

Go registry:

- `scripts/generate-go-style-builtins.py` mirrors the C catalog.
- `go/kryon/style_builtins.go` is generated.
- `make go-style-builtins-check` must pass.

Asset registry:

- only shipped `.kss` files should be embedded.
- deleted packs must not remain as embedded assets.
- `tests/style_assets_test.c` must assert current pack assets.

## Pack Coverage

Every built-in pack must cover all style kinds that are part of the app-facing widget surface.

Required coverage categories:

- base kind rules;
- state rules for hover/pressed/focus/disabled/selected/loading where meaningful;
- role rules for widget subparts;
- text roles;
- image roles;
- popup/panel roles;
- data/table roles.

Tests:

- `tests/style_builtin_packs_test.c`
- `go/kryon/style_sheet_test.go`
- parser tests for each pack file
- asset tests for embedded pack files

## StylePicker

The style picker should present:

1. Material
2. TK
3. Vanilla
4. Lightfield
5. app/product packs

Do not present Glow as a top-level option. When Lightfield variants exist, expose them as nested treatment choices or pack options.

Target UI behavior:

- picker uses current style pack for its own chrome;
- switching packs preserves widget state;
- switching invalidates style fingerprints;
- switching recomputes metrics only when affected;
- picker works even when the registry was initially empty by ensuring built-ins.

## Template Behavior

Project templates should attach Material explicitly:

```kry
#style <material> as material
```

Runtime startup may ensure and activate Material only when no active pack exists, but generated app source should still declare its baseline styling.

## Import Shape

Target app source:

```kry
#style <material> as material
#style <tk> as tk
#style <vanilla> as vanilla
#style <lightfield> as lightfield
#style "brand.kss" as brand
```

No built-in example should import `<glow>`.

## Migration Tasks

1. Keep current built-in catalog at four packs.
2. Audit examples for stale `<glow>` imports.
3. Audit docs for stale standalone Glow language.
4. Add variant syntax and runtime model for Lightfield glow treatment.
5. Convert every built-in pack to canonical token block syntax.
6. Add style capture boards for each pack.
7. Add performance captures for Material vs Lightfield.
8. Make style picker test assert four built-in options.
9. Ensure Go/C generated catalogs remain identical.
10. Delete unused legacy theme-pack bridges.

## Done Criteria

This phase is done when:

- `material`, `tk`, `vanilla`, and `lightfield` are the only built-in packs;
- Material is default;
- Lightfield is opt-in;
- glow is represented as Lightfield treatment/variant, not a pack;
- all packs cover the full public widget style surface;
- docs, examples, C tests, Go tests, and embedded assets agree.
