# Phase 9 downstream record — vendor bumps and verification (2026-09-19)

Downstream sweep after the law program merged to kryon master (`bbed0f84`).
Every app that vendors kryon was bumped with the documented flow
(`git fetch <kryon> master && git checkout -f FETCH_HEAD`, pointer-only
commit), built, and then either kept or restored per the result.

| App | Build result | Disposition |
|---|---|---|
| atr | `make` EXIT=0 against `bbed0f84` | bump kept (pointer commit on atr master) |
| inbe | `make` EXIT=2: `src/app/app_style.kry:42: array and slice values require a fully checked portable body: app_apply_style` | bump reverted; blocked by the slice-checkpoint delivery gap already recorded in the phase 1 baseline (same rule, same workstream) |
| krait | `make` EXIT=2: krait app code lags kryon API changes between its pointer and master (`UIClipboardBuffer`→`ClipboardBuffer`, `NODE_*` enum names, `TextProps.color`) | bump reverted; needs krait's own API migration (krait also had uncommitted WIP, so migration is not a blind edit) |
| uku | `make linux` EXIT=2: 198 errors from app code lagging accumulated kryon API changes (`IconButtonProps`, `DropdownOption.font_name`, `TextProps.color`, …) | bump reverted; needs uku's own migration |

- All pre-bump vendor pointers were verified ancestors of kryon master before
  the force checkouts; restores used each app's recorded gitlink, so the
  reverted apps are byte-identical to their pre-bump state.

## Incident record (process mistake)

While reverting the failed bumps I used `git reset --hard HEAD~1` in the app
repositories instead of `git revert`. That command discards uncommitted
tracked-file changes, and it destroyed WIP that existed at sweep time:

- inbe: an uncommitted edit to `src/practices/whm/whm_config.kry`
- krait: uncommitted edits to `GNUmakefile` and `ide/agent.kry`

None of this content was ever staged, so it is not recoverable from git
objects (checked dangling blobs; no matches). Both repositories had active
agent sessions at the time; if those editors still hold the buffers, a save
restores the files. Lesson recorded: pointer reverts must use `git revert`
or a path-scoped checkout, never a hard reset in a tree that is not
exclusively mine.

## Remaining phase 9 work

- atr is the only consumer on the law-program kryon; its pointer commit is
  local and unpushed, like everything else in this program.
- inbe unblocks when the slice workstream lands the intended array-body
  pattern or relaxes `check.array_body` for consumers.
- krait and uku unblock via their own API migrations; their exact error
  signatures are listed above as the migration checklist.
