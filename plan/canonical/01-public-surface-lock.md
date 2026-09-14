# 01 Public Surface Lock

## Goal

Keep the public widget surface clean, canonical, and prefix-free while the rest
of the implementation moves behind those names.

## Current State

- Public canonical names are listed in `docs/CANONICAL_WIDGET_SURFACE.md`.
- Compatibility names such as `Href`, `Picture`, `Combo`, `DragFloat`,
  `SliderInt`, `MenuButton`, `SplitButton`, `InfoButton`, and `ArrowButton`
  are rejected by guard tests.
- Generated Go should expose short package names such as `kr.Slider`, not
  project-prefixed widget names.
- Guard scripts may mention forbidden names in their own regexes; that is
  expected.

## Tasks

1. Keep `docs/CANONICAL_WIDGET_SURFACE.md` as the authoritative widget list.
2. Do not add public aliases or compatibility wrappers.
3. Treat helper variants as props/composition on canonical widgets:
   `Button` handles menu/split/info/arrow-style behavior through props or
   composition, not separate public widgets.
4. Keep `Image(ImageProps)` as the only public app image surface. Do not
   reintroduce `Picture`, `Texture`, or page-image aliases.
5. Keep URL activation named `Link`; do not reintroduce `Href`.
6. Keep numeric widgets as `Slider`, `Drag`, `Input`, and `Spinbox` with typed
   props. Do not add split public names like `SliderInt` or `DragFloat`.
7. Keep lowered host helpers out of public docs and generated API names.

## Proof

Run these after every public-surface change:

```sh
sh tests/public_api_names_test.sh
sh tests/canonical_surface_test.sh
python3 tests/canonical_widget_surface_doc_test.py
```

Optional broad scan:

```sh
rg -n '\b(Href|Picture|Combo|MenuButton|SplitButton|InfoButton|ArrowButton|DragFloat|DragInt|SliderFloat|SliderInt)\b' include src runtime docs go/kryon --glob '!build/**'
```

## Done When

- The guard commands pass.
- No public docs, headers, generated Go exports, parser names, or web runtime
  names expose compatibility names.
- Any old name appears only inside tests that reject it.
