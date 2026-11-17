# Input components

These six Ziran components cover the selected modern input families without an
external compatibility layer. Each render entry is `(Session, Props, *State) -> Result`;
each `Apply` function has the same behavior without requiring a building frame.
Caller-owned state is plain fixed-size data, including text bytes, so it can be
copied or serialized. No component owns global text, focus, clipboard, or
shortcut state.

All compositions submit real retained-tree nodes, resolve the installed theme
and stylesheet through the current style facts, and use the existing field, box,
text, paint, and session primitives. IDs, style classes, disabled state, focus,
bounds, and supported font/geometry/motion metrics remain customizable. A
disabled component neither accepts text nor keeps focus.

## Tag input

```zi
TagInput :: #import "kryon/tag_input";

TagInput.Draw :: (session: Session, props: TagInput.Props,
    state: *TagInput.TagInputState) -> TagInput.TagInputResult
TagInput.TagInputApply :: (state: *TagInput.TagInputState,
    props: TagInput.Props) -> TagInput.TagInputResult
```

`TagInputState` owns up to 64 tags and their stable caller IDs, the editable
text, selection, scroll, and focus. Initial IDs can be supplied through
`TagInputStateInit`; newly entered tags receive stable generated IDs.

Commit text on Enter or `TextField.Input.commit`. A nonempty separator also
splits committed and pasted text. Spaces can be trimmed, duplicates rejected,
and a positive `max_tags` enforced. Backspace on an empty field requests removal
of the final tag through `field.backspace_on_empty`; explicit removal is by
`remove_index` or stable `remove_id`. Results distinguish add, remove, duplicate
rejection, maximum count, removed ID, and number added. The group, chips, remove
buttons, and nested field all honor disabled state and stylesheet cascade.

## One-time code

```zi
OneTimeCode :: #import "kryon/one_time_code";

OneTimeCode.Draw :: (session: Session, props: OneTimeCode.Props,
    state: *OneTimeCode.OneTimeCodeState) -> OneTimeCode.OneTimeCodeResult
OneTimeCode.OneTimeCodeApply :: (state: *OneTimeCode.OneTimeCodeState,
    props: OneTimeCode.Props) -> OneTimeCode.OneTimeCodeResult
```

The state owns 1–12 alphanumeric slots, the cursor, focus, and prior completion.
Typed characters insert at the cursor and advance it. Left/right move, Backspace
deletes backward, and paste replaces all slots after filtering invalid bytes.
Escape blurs and reports cancellation. Completion is emitted only when the code
becomes full after a real change; it is not repeated by later frames or input.
Masked rendering shows `*`. This is only an input component and performs no
authentication or delivery.

Slots use the canonical 44×52 default, optional six-digit grouping, current
control radius, and display font. Width, height, radius, both gaps, and font
size are explicit style metrics. The group, every slot, and every character are
real retained nodes.

## Inline edit

```zi
InlineEdit :: #import "kryon/inline_edit";

InlineEdit.Draw :: (session: Session, props: InlineEdit.Props,
    state: *InlineEdit.InlineEditState) -> InlineEdit.InlineEditResult
InlineEdit.InlineEditApply :: (state: *InlineEdit.InlineEditState,
    props: InlineEdit.Props) -> InlineEdit.InlineEditResult
```

Activation selects the current value for replacement. Normal text and paste
edits update state directly; IME composition is forwarded to the nested field.
Enter or blur commits the edited bytes. Escape cancels by restoring the backup
taken at activation and leaves editing mode. Disabled props close editing and
reject input. Results expose the nested field result, editing, commit, cancel,
and whether the committed value changed.

## Scrub field

```zi
ScrubField :: #import "kryon/scrub_field";

ScrubField.Draw :: (session: Session, props: ScrubField.Props,
    state: *ScrubField.ScrubFieldState) -> ScrubField.ScrubFieldResult
ScrubField.ScrubFieldApply :: (state: *ScrubField.ScrubFieldState,
    props: ScrubField.Props) -> ScrubField.ScrubFieldResult
```

Arrow-left/right, arrow-up/down, and horizontal drag use the same step. Shift
selects fine or coarse behavior: by default the non-shift direction moves ten
steps while Shift moves one tenth. `fine_divisor` and `coarse_multiplier`
replace those ratios when positive. Values round to `precision` and clamp to
the supplied bounds. Activation enters text mode, commit parses/clamps/rounds,
and Escape restores the pre-edit value. Results distinguish ordinary change,
drag start, commit, and cancel.

The label, editable value, and suffix are retained child nodes. Height is an
explicit metric; the remaining visual contract starts at current control
metrics and can be overridden by ID/class stylesheet rules.

## Money field

```zi
MoneyField :: #import "kryon/money_field";

MoneyField.Draw :: (session: Session, props: MoneyField.Props,
    state: *MoneyField.MoneyFieldState) -> MoneyField.MoneyFieldResult
MoneyField.MoneyFieldApply :: (state: *MoneyField.MoneyFieldState,
    props: MoneyField.Props) -> MoneyField.MoneyFieldResult
```

The state owns a `float64` value and the focused edit bytes. Blurred display
uses the selected decimal separator, optional grouping, optional currency
prefix/suffix, precision, and minus-or-parentheses negative style. Gaining focus
changes to a plain round-trippable edit string; commit parses grouping and
either negative notation, rounds, clamps, and returns to formatted display.
Escape cancels. Results expose validity as well as commit/cancel/change.

Both comma-decimal and period-decimal conventions are supported. The last period
or comma is decimal; earlier punctuation is grouping. No locale service is
hidden in widget state.

## Shortcut recorder

```zi
ShortcutRecorder :: #import "kryon/shortcut_recorder";

ShortcutRecorder.Draw :: (session: Session, props: ShortcutRecorder.Props,
    state: *ShortcutRecorder.ShortcutRecorderState) -> ShortcutRecorder.ShortcutRecorderResult
ShortcutRecorder.ShortcutRecorderApply :: (state: *ShortcutRecorder.ShortcutRecorderState,
    props: ShortcutRecorder.Props) -> ShortcutRecorder.ShortcutRecorderResult
```

Activation records only while this widget is focused. A supported key plus
modifiers is accepted, including `Ctrl+Enter`; bare Escape cancels recording,
bare Backspace or Delete clears the shortcut, and bare Tab exits recording.
Unknown or out-of-range keys/chords report `unsupported` without changing the
prior shortcut. Blur, outside press, and disabled also stop recording. This
component never installs a global accelerator or consumes another owner's input.

The state owns the key, modifier mask, focus/recording flags, and its fixed
label buffer. Width, height, and font size have explicit metrics; animation
uses the current theme motion duration and installed style cascade.

