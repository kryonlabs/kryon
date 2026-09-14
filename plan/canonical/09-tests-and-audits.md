# 09 Tests And Audits

## Goal

Use focused, fast tests for each policy move and broader guardrails for public
surface and transpilation safety.

## Required Focused Tests

For each moved policy:

1. Add or extend a `tests/*_policy_test.c` file.
2. Assert the decision record fields directly.
3. Assert boundary cases:
   - disabled
   - captured input
   - release without active drag
   - release with active drag
   - empty/null data
   - invalid index
4. Prefer policy tests over GUI-heavy tests for logic.

## Required Guardrails

Run these before committing most widget migration slices:

```sh
make generate-runtime
make <widget>-policy-test
go test ./... ./go/kryon/...
python3 tests/canonical_widget_surface_doc_test.py
sh tests/public_api_names_test.sh
git diff --check
```

Run broader checks when touching generated backends or retained tree behavior:

```sh
make build/linux-x86_64/tests/ui_tk_test
xvfb-run -a build/linux-x86_64/tests/ui_tk_test
sh tests/generated_runtime_parity_test.sh
```

## Audit Scans

```sh
rg -n 'ConsumeRelease\(|InputPointerInteractionFor\(|IsMouseButtonReleased\(|IsKeyPressed\(' src/ui --glob '!build/**'
rg -n '\b(Href|Picture|Combo|MenuButton|SplitButton|InfoButton|ArrowButton|DragFloat|DragInt|SliderFloat|SliderInt)\b' include src runtime docs go/kryon --glob '!build/**'
```

## Done When

- Every migration slice has a focused policy test.
- Public-name guardrails pass.
- Broad UI/runtime tests are used for higher-risk shared behavior.
