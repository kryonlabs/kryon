# 09 - Legacy Removal

Status: implementation plan
Scope: delete old styling paths after KSS coverage is complete.

## Objective

Remove legacy styling systems instead of preserving compatibility forever.

The final architecture should have one app-facing styling path:

```text
.kry structure + .kss style packs + StyleSheet resolver
```

Legacy code can remain only as an internal bridge during migration, and every bridge must have an owner and deletion condition.

## Legacy To Remove

### Theme Chrome

Remove app-facing reliance on:

- `GetThemeBackground()`
- `GetThemeSurface()`
- `GetThemeButton()`
- `GetThemeText()`
- `GetThemeBorder()`
- `GetThemeSelection()`
- theme getters used in widget paint paths.

Theme utilities may remain for platform/system preference and non-widget compatibility, but not as app-facing widget chrome.

### Visual Props

Remove public props such as:

- button style records;
- text color props;
- row/page color props;
- direct widget background/border/radius props;
- compatibility aliases that smuggle styling into widgets.

Keep semantic props:

- tone;
- role;
- class;
- state;
- size;
- disabled;
- selected;
- validation;
- orientation.

### Legacy Packs

Remove:

- standalone built-in `glow`;
- old theme import/export modes;
- hidden default Kryon style assumptions;
- compatibility loaders for deleted pack ids.

Do not re-add `<glow>` as a compatibility import. Use Lightfield variant support instead.

### Parser Compatibility

Remove old token-prefix syntax once all packs/docs are migrated:

```text
@token color text = #16181d;
```

Canonical syntax is grouped token blocks.

## Bridge Registry

While migration is ongoing, maintain a short list of temporary bridges.

Each bridge must document:

- file;
- purpose;
- owner widget;
- deletion condition;
- tests that protect the final path.

Example:

```text
Bridge: src/ui/ui_style.c theme token unpacking
Purpose: feeds old theme API while widgets migrate
Delete when: all app-facing widgets consume StyleData frames and downstream apps attach KSS
Tests: no-theme-chrome-check, widget no-style tests
```

## Deletion Order

1. Finish built-in pack coverage.
2. Finish widget style facts helpers.
3. Finish no-style tests.
4. Finish app/example migration.
5. Remove visual props.
6. Remove theme-style compatibility imports.
7. Remove legacy parser syntax.
8. Remove stale docs/examples.
9. Remove unused C helpers.
10. Add scanners preventing reintroduction.

## Search Patterns

Use these regularly:

```sh
rg "GetTheme" src runtime examples tests
rg "ThemeDefault|SetTheme|ThemeMode" src runtime examples tests
rg "StyleData base = \\{\\.fields" src go tests
rg "StyleControlFacts\\(" src go
rg "StyleControlRoleFacts\\(" src go
rg "<glow>|@pack glow|styles/kryon/glow" .
rg "background|foreground|border|radius|color" runtime/*_props.kry include
```

Each hit should be classified:

- test-only and valid;
- platform/system theme and valid;
- temporary bridge;
- migration target;
- bug.

## Compatibility Policy

Compatibility should be short-lived and explicit.

Allowed:

- temporary app migration notes;
- one release-cycle warnings if needed;
- internal conversion tools.

Not allowed:

- permanent hidden fallback to old themes;
- old pack id aliases;
- public API duplication for old names;
- parser accepting deleted syntax forever.

## Documentation Cleanup

Update or delete docs that mention:

- standalone Glow pack;
- theme as the main styling API;
- visual widget props as recommended API;
- old token prefix syntax;
- legacy default style assumptions.

Docs should consistently say:

- Material is default attached pack;
- TK, Vanilla, and Lightfield are selectable;
- Lightfield owns glow-capable treatment;
- widgets have zero hidden product styling.

## Done Criteria

Legacy removal is complete when:

- scanners find no unapproved theme chrome in widgets;
- no public visual props remain;
- no standalone Glow pack references remain;
- parser no longer accepts deleted syntax;
- examples attach style packs explicitly;
- downstream apps have migrated;
- CI fails if legacy paths are reintroduced.
