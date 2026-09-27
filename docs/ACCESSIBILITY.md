# Accessibility status

Kryon's Ziran UI modules carry semantic labels, kinds, selected state, and
heading levels in the retained tree. `accessibility_props.zi` defines action
values; `accessibility_policy.zi` decides which actions are allowed and how
selection and editable-value limits apply. `make test` runs these decisions
from source, saved `.zir`, and `.zib`.

The current terminal, desktop, libdraw, raylib, and canvas hosts do not expose a
verified platform accessibility tree. The removed C and native Go
AT-SPI adapters are not part of this Ziran package. The policy tests do not
establish screen-reader usability.

A platform adapter needs to export the committed semantic tree, keep node
identity and focus stable across frames, apply actions through the session,
protect secure text, and pass a real private-session assistive-technology
test. Until such a test passes, Kryon does not claim AT-SPI, UI Automation,
macOS accessibility, or mobile accessibility support.
