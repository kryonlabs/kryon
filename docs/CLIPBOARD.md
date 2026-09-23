# Clipboard state

`src/ui/clipboard.zi` is a checked, portable UI value module. Applications
keep a `ClipboardState` and own the bytes referenced by its strings. A platform
host supplies system clipboard observations with `ClipboardObserve` and reads
`ClipboardPendingWrite` to apply requested OS writes. After a successful OS
write, it calls `ClipboardWriteCompleted`. Pending writes take precedence over
older host observations.

`ClipboardCopySelection` updates the primary selection and requests a system
write for nonempty text. Copying an empty selection clears the primary value
without erasing the current system value. `ClipboardRead` selects system,
primary, or primary with system fallback. The checked text widgets return
copy, cut, and paste intents; the application handles those intents using this
state and its platform host.

OSC 52 and bracketed paste are terminal protocols owned by Kapsule. Their old
Kryon host sources have been removed from the new library tree. Kapsule still
uses its existing vendored Kryon version until its terminal host is migrated.
