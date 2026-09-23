# Style picker

`StylePicker(props, packs)` is an ordinary Ziran widget in
`src/ui/style_picker.zi`. The caller owns a slice of `StylePackOption` values,
including stable IDs, labels, and the active flag. The picker composes the
checked `Dropdown` widget from at most 32 options, matching the old picker
limit. Its result includes the dropdown's open, highlight, and scroll state;
the caller stores those values for the next frame. When selection changes,
`selected_id` names the pack for the caller to apply. An empty pack ID is not
reported as a change.

The picker does not read a C style-pack registry or mutate theme storage.
Native app hosts still need to provide a pack list and apply the returned ID.
