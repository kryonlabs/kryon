# 04 - Widget Migration

Status: implementation plan
Scope: move already-`.kry` widget styling policy out of C/Go and into runtime `.kry` plus `.kss`.

## Objective

For each widget family, remove hidden visual styling from implementation code and make KSS the only source of product chrome.

The work should proceed widget by widget, with small commits:

1. identify remaining C/Go styling logic;
2. add `.kry` facts/metrics/paint helpers;
3. resolve from zero visual base;
4. update built-in packs if coverage is missing;
5. add no-style and pack-style tests;
6. regenerate C/Go outputs;
7. commit.

## Migration Rule

Only migrate widgets that are already substantially represented in `.kry`.

Do not fight the parallel agent converting whole widgets to `.kry`. If a widget is currently being converted, leave it alone unless the relevant surface is already stable.

## Standard Widget Shape

Each widget should own these helpers in `runtime/<widget>.kry`:

```kry
WidgetRole :: () -> i32 #export
WidgetFactsFor :: (...) -> StyleFacts #export
WidgetRoleFactsFor :: (...) -> StyleFacts #export
WidgetMetricsFor :: (...) -> WidgetMetrics #export
WidgetPaintFor :: (...) -> WidgetPaint #export
WidgetInputPolicyFor :: (...) -> WidgetPolicy #export
```

Not every widget needs every helper, but every app-facing widget should have a canonical facts helper.

## C/Go Migration Pattern

Before:

```c
StyleData base = {
    .fields = StyleOpacity | StyleFontSize | StyleMaterial,
    .opacity = 1,
    .font_size = 16,
    .material = MaterialFlat,
};
StyleFacts facts = StyleControlRoleFacts(...);
StyleData value = ResolveActiveStyle(base, facts, state);
```

After:

```c
StyleData value = ResolveActiveStyle((StyleData){0},
    WidgetFactsFor(props.class_name, state),
    state);
```

The facts helper must live in `.kry`.

## Preserve Explicit Zero

For metrics helpers:

- missing padding may fallback;
- explicit zero padding remains zero;
- missing gap may fallback;
- explicit zero gap remains zero;
- negative input clamps or falls back according to documented policy.

For visual helpers:

- missing radius stays zero unless KSS declares it;
- missing opacity stays zero unless KSS declares it;
- missing material stays zero unless KSS declares it;
- missing color stays transparent/zero unless KSS declares it.

## Widget Priority

### High Priority

These are visible, common, and already have substantial `.kry` policy:

- Button
- Text
- TextInput/TextField/TextArea
- Dropdown
- Slider
- Toggle
- Checkbox
- Radio
- Progress
- Separator
- Card
- Surface/App/Page
- Toast
- Modal
- NavigationBar
- TabBar
- TableView
- Menu
- ListBox
- TreeView
- Toolbar
- TitleBar
- PanedView
- Reorder

### Medium Priority

- Plot
- Guide
- GuidePager
- Fieldset
- Collapsible
- ColorPicker
- Drag/Spinbox
- Selectable
- Image
- Popup
- Focus
- Rows

### Special Case

TerminalPane has product-adjacent color/theme behavior. Move only reusable style facts and chrome into Kryon. Do not move Kapsule terminal application behavior into Kryon.

## Audit Checklist Per Widget

Search for:

```sh
rg "GetTheme|Theme|StyleData base|ResolveActiveStyle|StyleControlFacts|StyleControlRoleFacts|minimalControlStyleData" src/ui go/kryon runtime tests
```

For the target widget, answer:

- Does C/Go construct style facts directly?
- Does C/Go seed visual defaults?
- Does C/Go resolve with a nonzero base?
- Does C/Go compute visual paint policy that belongs in `.kry`?
- Does KSS cover every role/state used by the widget?
- Is there a no-style regression?
- Does Go retained path match C immediate path?

## Required Tests Per Widget

Minimum:

- policy test for `.kry` facts/metrics/paint helper;
- no-style test for no hidden visual defaults;
- built-in pack coverage test;
- KSS class selector test if the widget exposes class names;
- Go test if retained runtime has a path for the widget.

When useful:

- capture test across built-in packs;
- layout measurement test;
- pointer/input policy test;
- backend parity test.

## Commit Discipline

Each widget commit should be narrow:

- one widget family;
- generated C/Go output only for touched `.kry` files;
- tests for that widget;
- built-in pack updates only if the widget needs coverage;
- no unrelated refactors.

Commit message pattern:

```text
Move <widget> style facts to kry
Move <widget> style metrics to KSS policy
Remove <widget> hidden style defaults
```

## Known Recent Examples

Use these as templates:

- text input style resolution moved to `.kry`;
- toast style facts moved to `.kry`;
- button radius fallback moved to `.kry`;
- page layout metrics moved to KSS policy;
- link/tab/dropdown release-consumption policy moved to `.kry`.

## Done Criteria

Widget migration is complete when:

- all app-facing widgets resolve style from KSS or zero base;
- no app-facing widget paint path calls theme getters for chrome;
- C/Go direct calls to `StyleControlFacts` are either tests or temporary bridges;
- every widget has no-style behavior coverage;
- built-in packs cover every widget kind and role;
- examples render only through attached packs.
