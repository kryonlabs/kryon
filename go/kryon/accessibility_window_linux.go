//go:build linux

package kryon

// TranslateCoordinates is asynchronous so accessibility geometry queries do
// not discard pending keyboard/pointer events while waiting for an X11 reply.
func (w *x11Window) requestScreenOrigin() {
	if w.originKnown || w.originPending || w.conn == nil {
		return
	}
	request := make([]byte, 16)
	request[0] = 40
	put16(request[2:], 4)
	put32(request[4:], w.window)
	put32(request[8:], w.root)
	if w.write(request) == nil {
		w.originSequence = w.seq
		w.originPending = true
		w.originDirty = false
	}
}
