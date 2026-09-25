# Composition input

Kryon's checked `TextField` and `TextArea` accept raw composition events in
`props.input.composition_event`. They return the next `CompositionState` and,
for a commit, a normal byte-range edit. The caller owns the committed text and
applies that edit before the next frame.

`composition_input.zi` exposes `SampleComposition()` as an ordinary Ziran
library function. It calls the platform's `PollComposition` capability and
returns a `CompositionSample`. Convert an available sample with
`CompositionEventForSample(sample)` and pass it to the focused text widget.
An unavailable sample converts to a no-op event. Events have the same phase
numbers as the platform input phases: start 1, update 2, commit 3, cancel 4.

`src/backend/composition_queue.zi` implements a single-threaded queue for up
to 16 pending events. `BeginCompositionFrame`, `SubmitComposition`, and
`TakeComposition` manage ordering, owned byte copies, and UTF-8 truncation.
The queue is tested from source and saved `.zir` on C, C++, and Go targets.
Its pointer parameters are not yet supported by the portable VM.

Portable applications bind `composition_input:PollComposition` to an ordinary
Ziran provider with `ziran bundle --bind`; the checked test uses this route for
source and saved `.zib` execution. A native platform host may implement the
same `PollComposition() -> CompositionSample` function directly. The returned
text must stay alive while the caller uses it. A caller that retains
`CompositionState.text` must copy the bytes into its own storage before the
provider releases them. Kryon owns composition decisions, preedit paint, and
commit ranges; platform adapters own event delivery.
