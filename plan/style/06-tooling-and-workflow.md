# 06 - Tooling And Workflow

Status: implementation plan
Scope: build, preview, generation, inspector, and developer workflow for style sheets.

## Objective

Make styling pleasant to author and safe to ship:

- development uses hot-reloaded `.kss`;
- release builds use compiled style data;
- errors are precise;
- generated files stay synchronized;
- previews show pack differences quickly;
- inspector explains every visible style value.

## Build Pipeline

Development:

1. Read `.kry`.
2. Read `.kss`.
3. Parse KSS with diagnostics.
4. Build token/rule tables.
5. Resolve and render.
6. Hot-reload changed files.

Release:

1. Parse `.kss` at build time.
2. Flatten imports and token overlays.
3. Emit typed rule tables.
4. Embed assets or compiled tables.
5. Runtime loads compiled data, not text parser on hot paths.

## Generated Files

Generated outputs that must remain synchronized:

- generated C runtime files under `build/.../generated/src/runtime`;
- generated public prop headers under `include/ui_*.generated.h`;
- generated Go runtime files under `go/kryon/*.go`;
- generated built-in style sources under `go/kryon/style_builtins.go`;
- embedded asset data.

Useful commands:

```sh
make generate-runtime
make go-style-builtins
make go-style-builtins-check
make style-builtins-test
make kss-parser-test
```

Rule:

- commit generated files that are tracked and expected by the repository;
- do not commit build output under `build/`;
- stage only files related to the current change.

## Preview Tooling

`kryon-preview` should support:

- selecting active style pack;
- selecting theme overlay;
- selecting Lightfield variants when variants exist;
- reloading `.kss` without app rebuild;
- showing parse errors inline;
- showing matched rules for hovered widget;
- toggling no-style mode;
- comparing Material/TK/Vanilla/Lightfield captures.

Preview should make the style picker real early, even before the final inspector is complete.

## Style Capture Boards

Create boards for:

- Button
- Text
- TextInput/TextArea
- Dropdown
- Slider
- Toggle
- Checkbox/Radio
- Progress
- Card/Surface
- Modal/Toast/Popup
- NavigationBar/TabBar
- TableView/Menu/ListBox/TreeView

Each board should render:

- normal;
- hover;
- pressed;
- focus;
- disabled;
- selected;
- loading when applicable;
- class selector sample;
- no-style mode where feasible.

Each built-in pack should have captures:

- Material
- TK
- Vanilla
- Lightfield

## Inspector

Inspector phases:

### Phase 1 - Rule Results

Show:

- active pack id;
- style kind;
- role;
- class;
- state;
- final `StyleData`.

### Phase 2 - Winning Declarations

For each field:

- winner rule;
- source file;
- line/column;
- layer;
- specificity;
- token resolved.

### Phase 3 - Backend Degradation

For each rendered frame:

- requested material/effect;
- backend capability;
- degradation decision.

### Phase 4 - Authoring Aid

Add:

- copy selector;
- jump to source;
- list unmatched classes;
- warn on dead rules.

## Diagnostics

KSS errors must include:

- file path;
- line;
- column;
- offending token;
- expected syntax;
- suggestion where possible.

Examples:

```text
styles/app.kss:42:13: unknown property "backgrond"; did you mean "background"?
styles/app.kss:88:7: token "accent" is not defined in color tokens
```

## Formatter

Formatter requirements:

- stable output;
- preserve comments when possible;
- canonical token block layout;
- canonical selector spacing;
- no semantic rewrites;
- test with round-trip fixtures.

Formatter can land after parser stabilization.

## Developer Workflow

Recommended widget migration loop:

```sh
rg "WidgetName|StyleControlFacts|ResolveActiveStyle" src/ui go/kryon runtime tests
edit runtime/widget.kry
make generate-runtime
make widget-policy-test
go test .
make build/linux-x86_64/tests/ui_tk_test
build/linux-x86_64/tests/ui_tk_test
git diff --check
git add scoped files
git commit -m "Move widget style facts to kry"
```

## Done Criteria

Tooling is complete when:

- preview hot-reloads KSS;
- release builds embed compiled style data;
- generated C/Go files stay checked;
- style captures exist for built-in packs;
- inspector explains final style values;
- diagnostics are precise enough for normal app authors.
