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

The portable host supplies `CompositionQueue` in
`libkryon_host.a`. Create a queue, submit platform events with
`CompositionQueueSubmit`, call `CompositionQueueBeginFrame` before each
`BundleInstanceRun`, and bind `PollCompositionBinding(queue)`. Native generated
hosts can use `CompositionQueueTake` to implement their `PollComposition`
function. The queue is single-threaded and holds up to 16 pending events;
sampling consumes them in order.

Polled text borrows queue storage through the current frame. A caller that
retains `CompositionState.text` must copy the bytes into its own storage before
the next `CompositionQueueBeginFrame`. The widget owns composition decisions,
preedit paint, and commit ranges. Platform adapters own event delivery and
text storage. Current Android and browser adapters still submit to the old
runtime queue and need to be wired to this host queue during native host
migration.
