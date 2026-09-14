# 02 - KSS Language

Status: implementation plan
Scope: define the final `.kss` language shape and the parser/compiler work needed to support it.

## Objective

Make KSS a compact, typed styling language for Kryon. It should be stronger and cleaner than ad hoc theme APIs, but smaller and more deterministic than browser CSS.

KSS should support:

- packs;
- imports;
- tokens;
- layers;
- semantic selectors;
- state selectors;
- roles;
- classes;
- names;
- themes/overlays;
- style variants where needed;
- typed properties;
- deterministic cascade;
- parser diagnostics and source maps.

It should not support arbitrary browser features that make resolution hard to test.

## Syntax Direction

The old token prefix form:

```text
@token color text = #16181d;
@token length space.2 = 8;
```

should not be the canonical form.

Canonical form:

```text
@pack material;
@import <reset>;
@layer tokens, base, components, screens;

tokens {
  color {
    text: #16181d;
    muted: #6f7480;
    canvas: #fafafa;
    surface: #ffffff;
    accent: #2f6bff;
    danger: #bd2430;
  }

  length space {
    2: 8;
    3: 12;
    4: 16;
  }

  radius {
    sm: 4;
    md: 6;
  }

  duration {
    fast: 80ms;
    normal: 140ms;
  }

  font {
    ui: "Noto Sans";
  }

  material {
    default: Flat;
    premium: Lightfield;
  }
}
```

Parser compatibility with older syntax should be deleted once generated packs and docs are migrated. Do not add a long-lived compatibility layer for token prefixes.

## Selectors

Required selectors:

```text
Button
Button:hover
Button:pressed
Button:focus
Button:disabled
Button:selected
Button:loading
Button[tone=Accent]
Button[emphasis=Outline]
Button[size=Small]
Button[role=Label]
Button.primary
Button[class=primary]
Button#save
*
```

Selector rules:

- kind selector is semantic, not implementation class;
- role selector targets widget subparts;
- class selector is app-controlled;
- id/name selector is stable but should be used sparingly;
- state selector matches the active interaction state;
- tone/emphasis/size selectors are semantic axes;
- `*` is allowed for broad reset/base rules only.

Do not add sibling, child, descendant, attribute substring, or arbitrary expression selectors in this phase.

## Cascade

Cascade order:

1. layer order;
2. specificity;
3. source order.

Specificity weights:

- kind: low;
- role/tone/emphasis/size/state/validation/orientation/placement: medium;
- class: high;
- name/id: highest.

The resolver must be deterministic and portable. C, Go, JS, KRB, and generated data must resolve the same field winners.

## Properties

Required first-class properties:

- `background`
- `background-end`
- `foreground`
- `border`
- `focus`
- `radius`
- `border-width`
- `opacity`
- `padding-x`
- `padding-y`
- `gap`
- `font-size`
- `letter-spacing`
- `typeface`
- `icon-size`
- `offset-x`
- `offset-y`
- `material`

Near-term additions:

- `font-family` as canonical alias or replacement for typeface where appropriate;
- `line-height`;
- `min-width`/`min-height` only if they remain widget-local metrics;
- transition properties for supported fields;
- pack-level variant metadata.

Every property needs:

- parser support;
- typed field in `StyleData` or a companion style table;
- C, Go, JS, and KRB generated parity;
- resolver tests;
- at least one built-in pack declaration;
- inspector source reporting.

## Values

Supported value types:

- color literals: `#rgb`, `#rrggbb`, `#rrggbbaa`;
- numeric lengths;
- duration values: `80ms`, `0.14s`;
- strings for fonts;
- enum values such as `Flat`, `Glass`, `Lightfield`;
- token references;
- small built-in functions where deterministic.

Allowed functions:

- `color-mix(a, b, amount)`;
- `opacity(color, amount)`;
- future: `scale(value, factor)` if needed.

Avoid general expressions until the compiler has full parity tests.

## Themes And Overlays

Theme blocks should override tokens, not duplicate whole packs:

```text
@theme dark {
  text: #f4f6fb;
  canvas: #101116;
  surface: #191b22;
  accent: #7aa2ff;
}
```

Overlay order:

```text
style pack + theme overlay + environment overlay + scoped rules
```

High contrast should be either:

- a full pack when the look is fundamentally different; or
- an overlay when it mainly changes colors and contrast.

## Variants

Lightfield should own glow-like treatment as an option, not as a separate top-level built-in pack.

Target shape:

```text
@pack lightfield {
  option glow {
    label: "Glow";
    default: false;
  }
}

Button[variant=glow] {
  material: premium;
}
```

If the final syntax changes, preserve the product truth:

- `material`, `tk`, `vanilla`, `lightfield` are built-in packs;
- glow is a Lightfield treatment/variant;
- glow is not its own shipped stylesheet.

## Parser Work

Implementation tasks:

1. Parse grouped token blocks.
2. Parse `@import <pack>` and file imports.
3. Parse explicit `@layer` declarations.
4. Parse `@theme` blocks as token overlays.
5. Parse class, role, tone, emphasis, size, state, name selectors.
6. Produce stable diagnostics with line/column.
7. Produce source maps for inspector.
8. Delete legacy token-prefix syntax after pack migration.
9. Add fuzz tests for invalid syntax.
10. Add round-trip formatter tests once formatting lands.

## Done Criteria

KSS language work is complete when:

- every shipped built-in pack uses canonical syntax;
- parser tests cover valid and invalid syntax;
- generated C and Go built-ins are in sync;
- old token-prefix docs are gone;
- source maps can explain winning declarations;
- no shipped pack uses a hidden runtime theme fallback.
