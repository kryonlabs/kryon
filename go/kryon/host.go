package kryon

import "image"

// FrameFunc draws one complete frame using the package-level Kryon API.
type FrameFunc func()

// Host owns a native Go runtime and renders frames without cgo.
//
// It is the standard-library host core used by tests, generated-code smoke
// runs, and future platform windows. Frame functions may be generated k2g
// functions or handwritten Go that calls BeginFrame, widgets, and EndFrame.
type Host struct {
	runtime Runtime
	config  AppConfig
}

func NewHost(config AppConfig) *Host {
	runtime := New(config)
	if config.Width <= 0 {
		config.Width = int(runtime.GetScreenWidth())
	}
	if config.Height <= 0 {
		config.Height = int(runtime.GetScreenHeight())
	}
	resetDirectState()
	return &Host{runtime: runtime, config: config}
}

func (h *Host) Runtime() Runtime {
	if h == nil {
		return nil
	}
	return h.runtime
}

func (h *Host) Draw(frame FrameFunc) {
	if h == nil || h.runtime == nil || frame == nil {
		return
	}
	previous := activeRuntime
	activeRuntime = h.runtime
	defer func() {
		activeRuntime = previous
	}()
	frame()
}

func (h *Host) Frame(frame FrameFunc) *image.RGBA {
	h.Draw(frame)
	return h.Render()
}

func (h *Host) Render() *image.RGBA {
	if h == nil || h.runtime == nil {
		return RenderFrame(1, 1, nil)
	}
	return RenderFrame(int(h.runtime.GetScreenWidth()), int(h.runtime.GetScreenHeight()), h.FrameOps())
}

func (h *Host) FrameOps() []FrameOp {
	if h == nil || h.runtime == nil {
		return nil
	}
	runtime, ok := h.runtime.(frameOpController)
	if !ok {
		return nil
	}
	return runtime.FrameOps()
}

func (h *Host) Close() {
	if h != nil && h.runtime != nil {
		h.runtime.Close()
	}
}

func (h *Host) QueueTap(x, y float32) {
	if h == nil {
		return
	}
	if runtime, ok := h.runtime.(pointerController); ok {
		runtime.QueueTap(x, y)
	}
}

func (h *Host) QueueText(text string) {
	if h == nil {
		return
	}
	if runtime, ok := h.runtime.(inputController); ok {
		runtime.QueueText(text)
	}
}

func (h *Host) QueueKey(key int32) {
	if h == nil {
		return
	}
	if runtime, ok := h.runtime.(inputController); ok {
		runtime.QueueKey(key)
	}
}

func (h *Host) QueueShiftKey(key int32) {
	if h == nil {
		return
	}
	if runtime, ok := h.runtime.(inputController); ok {
		runtime.QueueShiftKey(key)
	}
}

func (h *Host) QueueShortcut(key int32) {
	if h == nil {
		return
	}
	if runtime, ok := h.runtime.(inputController); ok {
		runtime.QueueShortcut(key)
	}
}

func (h *Host) SetFocus(id int32) {
	if h == nil {
		return
	}
	if runtime, ok := h.runtime.(focusController); ok {
		runtime.SetFocus(id)
	}
}

func (h *Host) Focus() int32 {
	if h == nil {
		return 0
	}
	if runtime, ok := h.runtime.(focusController); ok {
		return runtime.Focus()
	}
	return 0
}

func (h *Host) SetClipboardText(text string) {
	if h == nil {
		return
	}
	if runtime, ok := h.runtime.(clipboardController); ok {
		runtime.SetClipboardText(text)
	}
}

func (h *Host) ClipboardText() string {
	if h == nil {
		return ""
	}
	if runtime, ok := h.runtime.(clipboardController); ok {
		return runtime.ClipboardText()
	}
	return ""
}

func (h *Host) SetSelection(focusID, anchor, cursor int32) {
	if h == nil {
		return
	}
	if runtime, ok := h.runtime.(selectionController); ok {
		runtime.SetSelection(focusID, anchor, cursor)
	}
}

func (h *Host) Selection(focusID int32) (anchor, cursor int32, ok bool) {
	if h == nil {
		return 0, 0, false
	}
	if runtime, ok := h.runtime.(selectionController); ok {
		return runtime.Selection(focusID)
	}
	return 0, 0, false
}
