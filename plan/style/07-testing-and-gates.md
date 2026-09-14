# 07 - Testing And Gates

Status: implementation plan
Scope: tests and CI gates that prove style separation stays true.

## Objective

Prevent regressions where visual styling sneaks back into widget code.

The test suite should fail when:

- a widget adds a visual prop;
- C/Go paint code injects chrome;
- built-in packs miss a widget kind or role;
- no-style mode renders product chrome;
- C and Go resolve styles differently;
- generated style built-ins are stale;
- parser behavior changes without test updates.

## Test Categories

### Parser Tests

Files:

- `tests/kss_parser_test.c`
- Go parser tests where applicable

Must cover:

- pack declaration;
- imports;
- token blocks;
- layers;
- roles;
- classes;
- names;
- states;
- tones/emphasis/size;
- invalid properties;
- invalid values;
- stale legacy syntax removal.

### Resolver Tests

Must cover:

- cascade order;
- specificity;
- source order;
- explicit zero preservation;
- state matching;
- class matching;
- role matching;
- name/id matching;
- missing style sheet behavior;
- zero base behavior.

### Built-In Pack Tests

Files:

- `tests/style_builtin_packs_test.c`
- `go/kryon/style_sheet_test.go`
- `tests/style_assets_test.c`

Must assert:

- registered pack ids;
- Material default;
- Glow is not standalone;
- every pack parses;
- every pack asset embeds;
- every pack covers required style kinds;
- every pack covers required roles/states.

### Widget Policy Tests

Each migrated widget gets a policy test:

```text
tests/<widget>_policy_test.c
```

Must cover:

- facts helper values;
- metrics fallback;
- explicit zero;
- paint geometry;
- input/release policy where relevant.

### No-Style Tests

No-style tests must prove widgets do not draw product chrome without a style pack.

Targets:

- Button
- Text
- TextInput
- Dropdown
- Toast
- Modal
- NavigationBar
- TableView
- Menu/ListBox/TreeView
- Image placeholder
- Page/App background

No-style expected behavior:

- no product colors;
- no decorative border;
- no radius/material defaults;
- interaction still works;
- structural metrics still allow usable behavior where required.

### Runtime Parity Tests

Must compare:

- C immediate;
- Go retained;
- JS/KRB generated logic when available.

Use shared fixtures and expected resolved data.

### Capture Tests

Capture boards should verify:

- pack visual coverage;
- state visual differences;
- Material is cheap/flat;
- Lightfield is opt-in/premium;
- TK is compact;
- Vanilla preserves historical look.

Capture tests should be stable and tolerate backend rendering noise only through explicit thresholds.

## Scanners

Add or tighten scanners for:

- raw visual props in public widget props;
- theme getter calls in widget paint paths;
- `StyleData base` with visual defaults;
- direct `StyleControlFacts` in C/Go outside tests or approved bridges;
- hardcoded product colors in widget implementations;
- references to deleted built-in packs such as `<glow>`.

Suggested scanner names:

- `visual-props-check`
- `paint-style-leak-check`
- `style-facts-bridge-check`
- `no-glow-pack-check`
- `no-theme-chrome-check`

## CI Gates

Minimum gates:

```sh
make kss-parser-test
make style-assets-test
make style-builtins-test
make go-style-builtins-check
go test ./go/kryon
make toast-policy-test
make text-input-policy-test
make page-policy-test
git diff --check
```

Broad gates:

```sh
make fast-test
make test
make runtime-matrix-check
make renderer-matrix-check
make widget-matrix-check
```

Use focused tests per commit and broader gates before major merge/release points.

## Regression Template

Each bug fix should add one test in the nearest layer:

- parser bug -> parser test;
- resolver bug -> resolver test;
- widget leak -> no-style or policy test;
- built-in pack miss -> built-in coverage test;
- backend fork -> renderer parity test.

## Done Criteria

Testing is complete when:

- scanners catch new visual props and paint leaks;
- no-style mode is required in CI;
- built-in packs cannot miss widget roles silently;
- C and Go style results are parity-tested;
- every migrated widget has focused coverage;
- deleted legacy concepts cannot reappear unnoticed.
