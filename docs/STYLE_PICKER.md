# Style picker

`StylePicker(props, packs)` is an ordinary Ziran widget in
`src/ui/style_picker.zi`. The caller owns a slice of `StylePackOption` values,
including stable IDs, labels, and the active flag. The picker composes the
checked `Dropdown` widget from at most 32 options. Its result includes the
dropdown's open, highlight, and scroll state; the caller stores those values
for the next frame. When selection changes, `selected_id` names the pack for
the caller to apply. An empty pack ID is not reported as a change.

The picker does not keep a style-pack registry or change theme storage. The
caller supplies the pack list and applies the returned ID.
