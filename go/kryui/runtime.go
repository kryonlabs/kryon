package kryui

import "fmt"

// AppConfig is the window configuration emitted by k2g.
type AppConfig struct {
	Title         string
	Width, Height int
	FPS           int
}

// Runtime is the stable Go target used by generated Kry programs. It keeps
// window and nested-widget state behind Go methods rather than exposing cgo.
type Runtime interface {
	Close()
	WindowShouldClose() bool
	BeginDrawing()
	EndDrawing()
	ClearBackground(Color)
	Background(Color)
	Text(string, int32, int32, int32, Color)
	TextFormat(string, ...any) string
	ScaleUIPx(int32) int32
	GetThemeBackground() Color
	GetThemeText() Color
	GetThemeIcon() Color
	NewVector2(any, any) Vector2
	DrawCircleV(Vector2, any, Color)
	DrawRing(Vector2, any, any, any, any, int32, Color)
	Scroll(int32, int32, int32, int32, int32, *int32)
	EndScroll()
	Button(ButtonProps) bool
	BeginUI(UIKey)
	EndUI()
	Column(ColumnProps)
	End()
	TextField(TextFieldProps)
}

type runtime struct {
	scrolls []runtimeScroll
}

type runtimeScroll struct {
	bounds Rectangle
	view   ScrollView
}

func New(config AppConfig) Runtime {
	InitWindow(int32(config.Width), int32(config.Height), config.Title)
	if config.FPS > 0 {
		SetTargetFPS(int32(config.FPS))
	}
	return &runtime{}
}

func (r *runtime) Close()                  { CloseWindow() }
func (r *runtime) WindowShouldClose() bool { return WindowShouldClose() }
func (r *runtime) BeginDrawing() {
	BeginDrawing()
	w, h := GetScreenWidth(), GetScreenHeight()
	dpi := GetWindowScaleDPI().X
	if dpi <= 0 {
		dpi = 1
	}
	BeginUIFrame(w, h, dpi)
}
func (r *runtime) EndDrawing() {
	EndUIFrame()
	EndDrawing()
}
func (r *runtime) ClearBackground(c Color)                      { ClearBackground(c) }
func (r *runtime) Background(c Color)                           { Background(c) }
func (r *runtime) Text(s string, x, y, size int32, c Color)     { Text(s, x, y, size, c) }
func (r *runtime) TextFormat(format string, args ...any) string { return fmt.Sprintf(format, args...) }
func (r *runtime) ScaleUIPx(px int32) int32                     { return ScaleUIPx(px) }
func (r *runtime) GetThemeBackground() Color                    { return GetThemeBackground() }
func (r *runtime) GetThemeText() Color                          { return GetThemeText() }
func (r *runtime) GetThemeIcon() Color                          { return GetThemeIcon() }
func runtimeFloat(v any) float32 {
	switch n := v.(type) {
	case int:
		return float32(n)
	case int32:
		return float32(n)
	case int64:
		return float32(n)
	case float32:
		return n
	case float64:
		return float32(n)
	default:
		return 0
	}
}
func (r *runtime) NewVector2(x, y any) Vector2 {
	return NewVector2(runtimeFloat(x), runtimeFloat(y))
}
func (r *runtime) DrawCircleV(center Vector2, radius any, c Color) {
	DrawCircleV(center, runtimeFloat(radius), c)
}
func (r *runtime) DrawRing(center Vector2, inner, outer, start, end any, segments int32, c Color) {
	DrawRing(center, runtimeFloat(inner), runtimeFloat(outer), runtimeFloat(start), runtimeFloat(end), segments, c)
}
func (r *runtime) Scroll(x, y, w, h, contentHeight int32, offset *int32) {
	bounds := NewRectangle(float32(x), float32(y), float32(w), float32(h))
	goOffset := int(*offset)
	view := BeginScrollContainer(bounds, int(contentHeight), &goOffset, ScrollOptions{})
	*offset = int32(goOffset)
	r.scrolls = append(r.scrolls, runtimeScroll{bounds: bounds, view: view})
}
func (r *runtime) EndScroll() {
	if len(r.scrolls) == 0 {
		return
	}
	i := len(r.scrolls) - 1
	s := r.scrolls[i]
	r.scrolls = r.scrolls[:i]
	EndScrollContainer(s.bounds, s.view)
}
func (r *runtime) Button(props ButtonProps) bool  { return Button(props) }
func (r *runtime) BeginUI(key UIKey)              { BeginUI(key) }
func (r *runtime) EndUI()                         { EndUI() }
func (r *runtime) Column(props ColumnProps)       { Column(props) }
func (r *runtime) End()                           { End() }
func (r *runtime) TextField(props TextFieldProps) { declareTextField(props) }
