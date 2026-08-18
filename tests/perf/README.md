# Text input performance contract

`text_input.kry` is the single declarative source used to verify the KIR, C,
Go, and KRB lowerings. `make perf-text-input` rejects TODO/stub Go output,
missing KRB nodes, or generated C that no longer declares the field.

The timed section drives four retained text fields through injected input,
reconciliation, layout, routing, and observable field state. It covers typing
bursts, backspace, selection replacement, Tab traversal, and idle frames.
Each scenario runs 250 warmups and 3,000 measured iterations. Linux fails when
any scenario has p99 latency at or above 1 ms or changes the retained node
count. Results are JSON Lines so CI can retain and compare them without
parsing human output.

The generated KIR, C, Go, and KRB artifacts are validated before the native
retained-core runtime measurement. The report names that runtime explicitly;
it never mislabels one native run as execution of every lowering. OS compositor
and display latency are deliberately outside this deterministic gate. For a
real app, set `KRYON_FRAME_TRACE=1`; sampled JSON lines report reconciliation,
layout, input, update, draw, and total time, always retaining budget violations.
