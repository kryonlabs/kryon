# 04 Text Editing

## Goal

Finish moving text field and text area policy into `.kry` while keeping true
platform text services native.

## Current State

`TextField` and `TextArea` already have substantial `.kry` ownership:

- metrics
- KSS typography defaults
- paint geometry
- buffer limits
- cursor normalization
- navigation decisions
- edit intent policy
- selection state/range policy
- double-click and pan gates
- focus/platform text-input sync decisions
- text-buffer mutation/range/bracket policy
- composition/preedit gates and paint spans
- context-menu and keyboard edit-command decisions

Remaining native work includes raw string storage, memmove/scanning, IME,
selection ownership/painting, and rendering.

## Tasks

1. Split native text work into:
   - required platform service
   - reusable `.kry` policy
   - old immediate compatibility path that should disappear
2. Move remaining edit gates from `src/ui/ui.c` and retained-tree paths into
   `runtime/text_input.kry`.
3. Convert keyboard shortcut interpretation to `.kry` decision records where
   it is widget behavior.
4. Keep actual string memory operations native until `.kry` has a real
   string/storage story that is safe for all backends.
5. Make selection ownership decisions testable in `.kry` even if painting stays
   native.
6. Ensure Go and web generated runtimes use the same `.kry` text policies.

## Proof

```sh
make text-input-policy-test
make public-headers-compile-check
go test ./... ./go/kryon/...
sh tests/public_api_names_test.sh
rg -n 'UIText|TextInputControl|RenderTextField|RenderTextArea' include src runtime docs go/kryon --glob '!build/**'
```

## Done When

- Text widget decisions are generated from `.kry`.
- C keeps only storage, platform IME, text measurement, drawing, and unavoidable
  host glue.
- No public stale `UIText*` or split text-input helper names remain.
