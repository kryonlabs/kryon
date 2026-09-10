# Dropdown appearance

Button is the styling source of truth. The dropdown trigger uses the neutral
soft Button style unchanged. The floating panel uses the shared glass material;
dark selection uses accent filled Button styling, light selection uses accent
soft styling, and navigation hover uses neutral soft styling. Typography,
icons, padding, and gaps come from the resolved Button metrics.

The full contract, state rules, and allowed adaptations are documented in
[Widget styling](../../docs/WIDGET_STYLING.md).

Run `make dropdown-capture` to render the actual native controls in both themes.
The output is under `build/linux-x86_64/dropdown-captures/`. Captures are review
artifacts, not approved pixel baselines. Searchable and multiple-selection
menus in the reference are compositions, not implicit single-select behavior.
