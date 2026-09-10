# Dropdown appearance

Button is the styling source of truth. The dropdown trigger uses the neutral
soft Button style unchanged. The floating panel uses the shared glass material;
dark selection uses accent filled Button styling, light selection uses accent
soft styling, and navigation hover uses neutral soft styling. Typography,
icons, padding, and gaps come from the resolved Button metrics.

The full contract, state rules, and allowed adaptations are documented in
[Widget styling](../../docs/WIDGET_STYLING.md).

Run `make dropdown-capture` to render the actual native controls in both themes.
The output is under `build/linux-x86_64/dropdown-captures/`. The dark half of `approved.png` is the user-approved reference (2026-09-10).
The light half remains a review artifact. Searchable and multiple-selection
menus in the reference are compositions, not implicit single-select behavior.

Run `make -C examples 27_dropdowns` and launch
`build/examples/bin/27_dropdowns` for the interactive implementation. It starts
in dark mode; theme selection changes the real theme, workflow selection updates
the displayed value, and disabled options cannot be selected.

Run `make dropdown-reference-test` to regenerate native captures and compare
the dark panel against the approved image. This never updates the reference.
