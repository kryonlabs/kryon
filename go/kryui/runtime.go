package kryui

import "fmt"

// AppConfig is the window configuration emitted by k2g.
type AppConfig struct {
	Title         string
	Width, Height int
	FPS           int
	Flags         uint        // window flags applied before InitWindow (0 = none)
	MinWidth      int         // minimum window size (0 = no limit)
	MinHeight     int
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
	GetScreenWidth() int32
	GetScreenHeight() int32
	GetThemeBackground() Color
	GetThemeText() Color
	GetThemeIcon() Color
	NewVector2(any, any) Vector2
	DrawCircleV(Vector2, any, Color)
	DrawRing(Vector2, any, any, any, any, int32, Color)
	Rect(int32, int32, int32, int32, Color)
	RectGradientH(int32, int32, int32, int32, Color, Color)
	Line(int32, int32, int32, int32, Color)
	Scroll(int32, int32, int32, int32, int32, *int32)
	EndScroll()
	Button(ButtonProps) bool
	TabBar(Rectangle, []string, *int32, *int32) int32
	Progress(Rectangle, int32, int32, int32, string)
	Checkbox(int32, int32, int32, string, *int32) bool
	Dropdown(int32, int32, int32, int32, int32, string, *int32) bool
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
	if config.Flags != 0 {
		SetConfigFlags(config.Flags)
	}
	InitWindow(int32(config.Width), int32(config.Height), config.Title)
	if config.MinWidth > 0 || config.MinHeight > 0 {
		SetWindowMinSize(int32(config.MinWidth), int32(config.MinHeight))
	}
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

func (r *runtime) GetScreenWidth() int32  { return GetScreenWidth() }
func (r *runtime) GetScreenHeight() int32 { return GetScreenHeight() }
func (r *runtime) Rect(x, y, w, h int32, c Color) {
	DrawRectangleRec(NewRectangle(float32(x), float32(y), float32(w), float32(h)), c)
}
func (r *runtime) RectGradientH(x, y, w, h int32, left, right Color) {
	DrawRectangleGradientH(x, y, w, h, left, right)
}
func (r *runtime) Line(x1, y1, x2, y2 int32, c Color) { DrawLine(x1, y1, x2, y2, c) }
func (r *runtime) TabBar(bounds Rectangle, labels []string, selected *int32, scrollOffset *int32) int32 {
	return TabBar(bounds, labels, selected, scrollOffset)
}
func (r *runtime) Progress(bounds Rectangle, min, max, value int32, label string) {
	Progress(ProgressBarProps{Bounds: bounds, Min: min, Max: max, Value: value, Label: label})
}
func (r *runtime) Checkbox(id, x, y int32, label string, value *int32) bool {
	on := *value != 0
	changed := DrawUICheckboxToggle(x, y, label, &on)
	if on {
		*value = 1
	} else {
		*value = 0
	}
	return changed
}
func (r *runtime) Dropdown(id, x, y, w, h int32, options string, selected *int32) bool {
	return DrawUIDropdown(id, x, y, w, h, DropdownLabels(options), selected)
}
