# Style size codec consistency

Necessary upstream fix for Harmony's previously authorized native stylesheet
milestone. Preserve the other Kryon workers' source and normal index.

- [x] Read canonical instructions and styling API; compare current source to
  verified Gitea master. Width and height exist in StyleData, but the generated
  hosted codec still has 32 fields and drops both dimensions.
- [x] Generate to disposable output and replace only the StyleData block: 34
  fields, with width/height encoding, decoding and validation. No generator or
  unrelated generated codec changes.
- [x] Verify native source/saved-IR round trips, reordered wire
  fields, invalid scalar kinds and compatibility with earlier hosts.
- [x] Run the existing codec suite and required push-check gate.
- [ ] Publish only own hunks atop verified Gitea master. No release.
- [ ] Update CSS and Harmony to the exact single Kryon identity through package
  commands; verify the application before publishing its stylesheet milestone.

The current local branch is nine commits behind Gitea master; these affected
working files already equal the corresponding remote files before this fix.
No source checkout, worktree, clone, reset or shared-index mutation is needed.

These are native host codecs containing raw HostValue pointers, matching the
existing C-only codec suite. A direct portable test is unsupported by Ziran's
lowering (and returned exit 139 after its diagnostic); the portable guest uses
its ordinary values rather than these native codec buffers. No compiler change
is attempted in this task. The first new fixture also needed its explicit
control_props import, which is now present.

Local tests: `env ZIRAN_JOBS=1 make -j1 test-focus TEST=codec TEST_JOBS=1 &&
env ZIRAN_JOBS=1 make -j1 push-check` completed with code 0, managed receipt
`37433c2360a098c57913f529d86e42e82ad9`. Both codec suites passed; the required
source/public surface/docs/player/desktop/gamepad/text-host/GPU gate passed.
The full unrelated Kryon test suite was not rerun. No live desktop was used.
