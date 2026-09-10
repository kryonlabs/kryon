# Dropdown appearance

Proposal 2 (Frosted Glass) is the visual direction for the standard Dropdown
and Combobox. The native implementation uses the existing button materials:
neutral soft triggers, a raised panel with the theme's large radius, and flat
inset rows. Selection uses accent soft selected colors plus a checkmark, while
hover and keyboard navigation use neutral hover colors.

The appearance follows the active palette rather than hardcoding blue. Button
style resolution remains in runtime/button.kry and surface drawing policy in
runtime/surface.kry. The C and Go dropdown adapters reuse those policies.

Run `make dropdown-capture` to render actual native dropdowns beside buttons in
dark and light themes. Images are written to the build directory under
`dropdown-captures/`. These captures are review artifacts, not approved pixel
baselines. The reference image also illustrates searchable and multiple-selection
compositions; this styling change does not add those to the single-select API.
