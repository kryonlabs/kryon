# Downstream Kryon consumers

Reviewed after Kryon `153d7b2e`. This inventory scans git projects under
`/mnt/storage/Projects` for `.gitmodules` entries whose submodule path basename
is `kryon` or whose URL basename is `kryon.git`. It records the current local
checkout state only; it does not imply every app has been migrated or verified.

| Project | Submodule path | URL | Current local HEAD | Submodule status | Parent status for submodule |
|---|---|---|---|---|---|
| `atr` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `95f8170` | clean | clean |
| `dashboard` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `074bd9403` | clean | clean |
| `inbe` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `153d7b2ef` | clean | clean |
| `job` | `kryon` | `https://github.com/kryonlabs/kryon.git` | `b7a72150d` | clean | `M kryon` |
| `kam` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `14c8ddf` | clean | clean |
| `kasaival` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `e583287df` | clean | clean |
| `katalis` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `97790eb6` | clean | clean |
| `krait` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `34f677c9` | clean | clean |
| `neocats` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `b44b067` | clean | `A .gitmodules`; `AM vendor/kryon` |
| `oroot` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `8289a7b` | clean | clean |
| `pass` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `0c864559e` | clean | clean |
| `taiji` | `sys/src/kryon` | `https://github.com/kryonlabs/kryon.git` | `745c2d30` | clean | clean |
| `uku` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `74e2dcda8` | clean | clean |
| `workbook` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `815aa28` | clean | clean |

## Follow-up ownership

- `inbe` is the only downstream app intentionally moved to current Kryon during
  this plan pass; its vendored tree is clean and tracked by the Inbe pointer
  commits.
- `ktrem` is not a Kryon submodule consumer in this scan, but it is a direct
  downstream gate in `scripts/conformance-matrix.py --verify-downstream`. ktrem
  `5106e42` migrates it to the current Kryon clipboard/theme APIs, and the
  downstream gate passes.
- `job` has a clean Kryon checkout but an uncommitted parent pointer change.
- `neocats` has an in-progress Kryon submodule addition in the parent checkout.
- The remaining direct consumers still need per-app caller, asset, platform, and
  migration evidence before P6 can close.
