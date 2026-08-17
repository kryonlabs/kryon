# Text input performance contract

`text_input.kry` is the single declarative source used to verify the KIR, C,
Go, and KRB lowerings. `make perf-text-input` rejects TODO/stub Go output,
missing KRB nodes, or generated C that no longer declares the field.

The timed section is the shared retained-core pipeline from injected input
through reconciliation, layout, routing, and observable field state. Each
scenario runs 1,000 warmups and 10,000 measured iterations. Linux fails when
any scenario has p99 latency at or above 1 ms, needs more than one declarative
frame to expose the new value, or changes the retained node count. Results are
JSON Lines so CI can retain and compare them without parsing human output.

The `lowering` field identifies the artifact whose generated contract was
validated before the shared runtime measurement; it does not claim physical
keyboard-to-photon latency. OS compositor and display latency are deliberately
outside this deterministic gate.
