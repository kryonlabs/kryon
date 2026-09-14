# 05 - Runtime And Backends

Status: implementation plan
Scope: keep style resolution and backend consumption identical across Kryon runtimes.

## Objective

Every backend must consume the same resolved style frames. Backend code may degrade unsupported effects, but it must not choose different style values.

Primary paths:

- C immediate runtime;
- Go retained runtime;
- JS/Web runtime;
- KRB runtime;
- DOM backend;
- canvas backend;
- libdraw backend;
- termi backend;
- null/test backend.

## Runtime Style Model

The authoritative style model lives in runtime `.kry`:

- `StyleData`
- `StyleFacts`
- `StyleSelector`
- `StyleRule`
- `StyleSheet`
- cascade helpers
- selector matching
- style merging
- state transition helpers
- material helpers where backend-independent

C and Go generated outputs must be treated as build artifacts of the same model, not separate implementations.

## Resolution Contract

All backends receive:

- resolved `StyleData`;
- derived `StyleFrame` when needed;
- material/fill state when needed;
- text font request/typeface;
- layout/paint metrics derived in `.kry`.

Backends may not:

- call theme getters to decorate app-facing widgets;
- invent fallback colors;
- silently select Material or Lightfield;
- override radius/border/opacity for product chrome;
- interpret class names differently.

## Backend Degradation

Effects can degrade, values cannot fork.

Example:

- `material: Lightfield` can degrade to layered flat paint on termi or DOM;
- `background` remains the same resolved color;
- `border` remains the same resolved color;
- unsupported blur is reported to inspector;
- unsupported glow is skipped or approximated without changing selector resolution.

Every degradation should be explainable:

```text
property material=Lightfield resolved from lightfield.kss:844
backend termi degrades material Lightfield -> FlatLayered
```

## C Immediate Runtime

Tasks:

1. Replace direct `StyleControlFacts` construction with generated `.kry` facts helpers.
2. Replace nonzero visual `StyleData base` with zero base.
3. Move metrics derivation into `.kry`.
4. Keep text measurement in C only when it depends on the host font backend.
5. Keep draw command emission in C.
6. Add no-style tests for common C paths.

## Go Retained Runtime

Tasks:

1. Replace `minimalControlStyleData()` use where it decorates non-button widgets.
2. Use widget-specific facts helpers generated from `.kry`.
3. Add retained no-style tests.
4. Ensure frame ops include enough resolved metadata for tests and inspector.
5. Keep `go test .` green after each widget migration.

Important:

- The Go runtime often mirrors C logic manually.
- Each C style migration should check Go retained code for the same widget.
- If Go keeps a generic helper, it must be behavior-only or explicitly button-only.

## JS And KRB

Tasks:

- regenerate from `.kry`;
- add parity fixtures for style rule resolution;
- verify class/role/state selector matching;
- verify built-in style pack registration or compiled data loading;
- ensure no-style behavior is represented in snapshots.

## DOM/Canvas/Libdraw

Tasks:

- consume resolved frame ops;
- map supported properties directly;
- report unsupported material/effects;
- avoid CSS/browser defaults leaking into Kryon widgets;
- ensure text inherits only through Kryon style data, not browser stylesheet defaults.

## Termi

Tasks:

- map colors and text attributes from resolved style only;
- degrade borders/materials explicitly;
- avoid terminal theme colors as widget chrome;
- keep terminal-pane app behavior outside Kryon core unless reusable.

## Inspector Requirements

The inspector must be able to show:

- active style pack;
- active theme/overlay;
- matched rules;
- losing rules;
- winning declarations by field;
- source file and location;
- token origin;
- resolved numeric value;
- backend degradation result.

Backend frame ops should carry enough provenance or ids to connect rendered output to inspector data.

## Runtime Tests

Required:

- C policy tests for facts/metrics helpers;
- Go retained tests for no-style and pack-style rendering;
- style rule parity tests across generated runtimes;
- backend frame snapshot tests;
- material degradation tests;
- no-style mode smoke tests.

## Done Criteria

This phase is done when:

- generated `.kry` style resolver is the single source of truth;
- C and Go agree on widget facts and style results;
- JS/KRB parity tests pass;
- backends do not inject chrome;
- unsupported effects degrade explicitly and inspectably.
