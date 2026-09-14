# 08 - Downstream Rollout

Status: implementation plan
Scope: how to bring the style system into apps that vendor Kryon.

## Objective

Move Kryon styling upstream first, then update downstream apps by bumping their `vendor/kryon` submodule pointers.

Never edit downstream `vendor/kryon` files directly.

## Upstream Rule

All Kryon work lands in:

```text
/mnt/storage/Projects/kryon
```

Directly on `master`, unless the user explicitly asks for a branch.

Downstream apps only receive Kryon changes through submodule updates.

## Vendor Rule

Do not edit:

```text
*/vendor/kryon/*
*/vendor/*
```

If a downstream app exposes a Kryon bug:

1. reproduce in real Kryon repo;
2. fix and commit in `/mnt/storage/Projects/kryon`;
3. fetch that commit into the app's `vendor/kryon`;
4. commit only the submodule pointer in the app.

## Submodule Bump Procedure

```sh
cd /mnt/storage/Projects/<app>/vendor/kryon
git fetch /home/wao/Projects/kryon master
git checkout -f FETCH_HEAD
cd ..
git status
```

Expected downstream status:

```text
 M vendor/kryon
```

No modified files inside the submodule.

## App Migration Order

Recommended order:

1. Kryon examples.
2. Kryon parity/catalog fixtures.
3. Small internal sample apps.
4. Inbe.
5. Kapsule, but only for generic Kryon UI primitives.
6. Other apps with `vendor/kryon`.

Kapsule-specific terminal emulator/product behavior stays in Kapsule.

## App Source Migration

For each app:

1. Attach a style pack explicitly.
2. Remove theme-style compatibility imports.
3. Replace visual widget props with class/tone/role.
4. Move product styling into app `.kss`.
5. Add StylePicker if the app should expose style switching.
6. Verify no vendor Kryon changes.

Target app header:

```kry
#style <material> as material
#style "app.kss" as app
```

Or for apps with picker:

```kry
#style <material> as material
#style <tk> as tk
#style <vanilla> as vanilla
#style <lightfield> as lightfield
#style "app.kss" as app
```

## App KSS Migration

Create an app stylesheet:

```text
@pack app;
@import <material>;

tokens {
  color {
    brand: #2f6bff;
  }
}

Button.primary {
  background: brand;
}
```

Use classes for product-specific looks:

- `.primary`
- `.danger`
- `.quiet`
- `.hero`
- `.panel`

Use semantic widget facts for common states:

- `Button[tone=Accent]`
- `Text[role=Title]`
- `NavigationBarItem:selected`

## Compatibility Removal

Do not maintain theme import/export compatibility after migration.

Remove:

- theme-style compatibility modes;
- legacy theme pack bridges;
- default glow/theme import assumptions;
- app code that expects hidden Kryon chrome.

If an app still needs the old look, attach `vanilla`.

## Style Picker Rollout

Apps can expose style switching through:

- settings screen;
- developer menu;
- command palette;
- onboarding preference.

Picker options should come from registered packs. Do not hardcode stale pack ids.

Expected choices:

- Material
- TK
- Vanilla
- Lightfield
- app/product packs

## Verification Per App

Run app-specific checks:

- native build;
- key UI smoke tests;
- screenshot/capture comparison if available;
- settings persistence if StylePicker is exposed;
- vendor submodule clean check.

Check:

```sh
cd vendor/kryon
git status --short
```

Expected output is empty.

## Changelog Guidance

For app changelogs, describe user-facing behavior:

- refreshed styling system;
- new style choices;
- improved consistency;
- reduced heavy visual effects by default.

Do not mention:

- CI internals;
- submodule mechanics;
- parser implementation;
- generated files.

## Done Criteria

Downstream rollout is complete when:

- apps attach styles explicitly;
- no app edits vendored Kryon files;
- app product visuals live in app `.kss`;
- legacy theme compatibility is removed;
- Material default is accepted or intentionally overridden;
- apps with style choice use StylePicker.
