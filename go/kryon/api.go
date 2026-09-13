package kryon

import (
	"fmt"
	"os"
)

var activeRuntime Runtime

func Open(config AppConfig) Runtime {
	resetDirectState()
	if runtime, err := openWindowRuntime(config); err == nil {
		activeRuntime = runtime
		return activeRuntime
	} else if os.Getenv("KRYON_WINDOW_DEBUG") != "" {
		fmt.Fprintln(os.Stderr, "kryon: native window fallback:", err)
	}
	activeRuntime = New(config)
	return activeRuntime
}

func SetRuntime(runtime Runtime) {
	resetDirectState()
	activeRuntime = runtime
}

type focusController interface {
	SetFocus(int32)
	Focus() int32
}

type pointerController interface {
	QueueTap(float32, float32)
}

type mouseController interface {
	QueueMouseButton(int32, float32, float32)
	QueueMouseButtonDown(int32, float32, float32)
	QueueMouseMove(float32, float32)
	QueueMouseButtonUp(int32, float32, float32)
}

type inputController interface {
	QueueText(string)
	QueueKey(int32)
	QueueShiftKey(int32)
	QueueShortcut(int32)
}

type modifiedInputController interface {
	queueModifiedKey(int32, bool, bool)
}

type clipboardController interface {
	SetClipboardText(string)
	ClipboardText() string
}

type selectionController interface {
	SetSelection(int32, int32, int32)
	Selection(int32) (int32, int32, bool)
}

func FrameOps() []FrameOp {
	if runtime, ok := active().(frameOpController); ok {
		return runtime.FrameOps()
	}
	return nil
}

func active() Runtime {
	if activeRuntime == nil {
		activeRuntime = New(AppConfig{})
	}
	return activeRuntime
}

func Close()                  { active().Close() }
func WindowShouldClose() bool { return active().WindowShouldClose() }
func BeginFrame() {
	beginDirectFrame()
	active().BeginFrame()
}
func EndFrame() {
	active().EndFrame()
	endDirectFrame()
}
func BeginDisabled(disabled bool)      { active().BeginDisabled(disabled) }
func EndDisabled()                     { active().EndDisabled() }
func BeginPopup(props PopupProps) bool { return active().BeginPopup(props) }
func EndPopup()                        { active().EndPopup() }
func AcceleratorPressed(accelerator Accelerator) int32 {
	return active().AcceleratorPressed(accelerator)
}
func DispatchAccelerators(accelerators []Accelerator, count ...int32) int32 {
	return active().DispatchAccelerators(accelerators, count...)
}
func SetFocus(id int32) {
	if runtime, ok := active().(focusController); ok {
		runtime.SetFocus(id)
	}
}
func Focus() int32 {
	if runtime, ok := active().(focusController); ok {
		return runtime.Focus()
	}
	return 0
}
func QueueTap(x, y float32) {
	if runtime, ok := active().(pointerController); ok {
		runtime.QueueTap(x, y)
	}
}
func QueueText(text string) {
	if runtime, ok := active().(inputController); ok {
		runtime.QueueText(text)
	}
}
func QueueKey(key int32) {
	if runtime, ok := active().(inputController); ok {
		runtime.QueueKey(key)
	}
}
func QueueShiftKey(key int32) {
	if runtime, ok := active().(inputController); ok {
		runtime.QueueShiftKey(key)
	}
}
func QueueShortcut(key int32) {
	if runtime, ok := active().(inputController); ok {
		runtime.QueueShortcut(key)
	}
}
func SetClipboardText(text string) {
	if runtime, ok := active().(clipboardController); ok {
		runtime.SetClipboardText(text)
	}
}
func ClipboardText() string {
	if runtime, ok := active().(clipboardController); ok {
		return runtime.ClipboardText()
	}
	return ""
}
func SetSelection(focusID, anchor, cursor int32) {
	if runtime, ok := active().(selectionController); ok {
		runtime.SetSelection(focusID, anchor, cursor)
	}
}
func Selection(focusID int32) (anchor, cursor int32, ok bool) {
	if runtime, ok := active().(selectionController); ok {
		return runtime.Selection(focusID)
	}
	return 0, 0, false
}
func ClearBackground(c Color) { active().ClearBackground(c) }
func AppBackground()          { active().AppBackground() }
func Background(c Color)      { active().Background(c) }
func Text(props TextProps)    { active().Text(props) }
func MeasureTextWidth(text string, font int32, typeface string) int32 {
	return active().MeasureTextWidth(text, font, typeface)
}
func ReadActivation(bounds Rectangle, id int32, enabled bool) Activation {
	return active().ReadActivation(bounds, id, enabled)
}
func TextFormat(format string, args ...any) string { return active().TextFormat(format, args...) }
func Scale(px int32) int32                         { return active().Scale(px) }
func GetScreenWidth() int32     { return active().GetScreenWidth() }
func GetScreenHeight() int32    { return active().GetScreenHeight() }
func GetThemeBackground() Color { return active().GetThemeBackground() }
func GetThemeText() Color       { return active().GetThemeText() }
func GetThemeIcon() Color       { return active().GetThemeIcon() }
func Circle(centerX, centerY, radius int32, color Color) {
	active().Circle(centerX, centerY, radius, color)
}
func Ring(centerX, centerY, innerRadius, outerRadius int32, color Color) {
	active().Ring(centerX, centerY, innerRadius, outerRadius, color)
}
func Box(bounds Rectangle, fill Color, border Color) { active().Box(bounds, fill, border) }
func Surface(bounds Rectangle, style Style)          { active().Surface(bounds, style) }
func RectGradientH(x, y, w, h int32, left, right Color) {
	active().RectGradientH(x, y, w, h, left, right)
}
func Line(x1, y1, x2, y2 int32, color Color) { active().Line(x1, y1, x2, y2, color) }
func Scroll(x, y, w, h, contentH int32, offset *int32) {
	active().Scroll(x, y, w, h, contentH, offset)
}
func EndScroll() { active().EndScroll() }
func BeginTableCell(props TableViewProps, row, column int32) Rectangle {
	return active().BeginTableCell(props, row, column)
}
func EndTableCell() { active().EndTableCell() }
func BeginScroll(bounds Rectangle, contentHeight int32, offset *int32) Rectangle {
	return active().BeginScroll(bounds, contentHeight, offset)
}
func Button(props ButtonProps) bool           { return active().Button(props) }
func Selectable(props SelectableProps) bool   { return active().Selectable(props) }
func Checkbox(props CheckboxProps) bool       { return active().Checkbox(props) }
func Bullet(bounds Rectangle)                 { active().Bullet(bounds) }
func Separator(props SeparatorProps)          { active().Separator(props) }
func DragDrop(props DragDropProps) bool       { return active().DragDrop(props) }
func ColorPicker(props ColorPickerProps) bool { return active().ColorPicker(props) }
func TabBar(props TabBarProps) int32          { return active().TabBar(props) }
func Progress(props ProgressProps) {
	active().Progress(props)
}
func Plot(props PlotProps)          { active().Plot(props) }
func Drag(props DragProps) bool     { return active().Drag(props) }
func Slider(props SliderProps) bool { return active().Slider(props) }
func Input(props InputProps) bool   { return active().Input(props) }
func Dropdown(args ...any) bool     { return active().Dropdown(args...) }
func GetSegmentedControlHeight(props SegmentedControlProps) int32 {
	return active().GetSegmentedControlHeight(props)
}
func SegmentedControl(props SegmentedControlProps) SegmentedControlResult {
	return active().SegmentedControl(props)
}
func StylePicker(props StylePickerProps) bool {
	return active().StylePicker(props)
}
func Column(props ColumnProps)  { active().Column(props) }
func Row(props ColumnProps)     { active().Row(props) }
func Group(props ColumnProps)   { active().Group(props) }
func Stack(props ColumnProps)   { active().Stack(props) }
func Screen(props ColumnProps)  { active().Screen(props) }
func Grid(props GridProps)      { active().Grid(props) }
func End()                      { active().End() }
func SetPageTitle(title string) { active().SetPageTitle(title) }
func SetPageDescription(description string) {
	active().SetPageDescription(description)
}
func SetPageCanonicalURL(url string) { active().SetPageCanonicalURL(url) }
func SetPageThemeColor(color Color)  { active().SetPageThemeColor(color) }
func GetRoutePath() string           { return active().GetRoutePath() }
func GetRouteHash() string           { return active().GetRouteHash() }
func GetRouteVersion() int32         { return active().GetRouteVersion() }
func PushRoute(path string)          { active().PushRoute(path) }
func ReplaceRoute(path string)       { active().ReplaceRoute(path) }
func Page(props PageProps)           { active().Page(props) }
func Section(props SectionProps)     { active().Section(props) }
func Heading(props HeadingProps)     { active().Heading(props) }
func ParagraphText(props ParagraphTextProps) {
	active().ParagraphText(props)
}
func Link(props LinkProps) bool                 { return active().Link(props) }
func Flow(props FlowProps)                      { active().Flow(props) }
func Fade(c Color, alpha float32) Color         { return active().Fade(c, alpha) }
func GetThemeSurface() Color                    { return active().GetThemeSurface() }
func GetThemeBorder() Color                     { return active().GetThemeBorder() }
func GetThemeButton() Color                     { return active().GetThemeButton() }
func GetThemeButtonHover() Color                { return active().GetThemeButtonHover() }
func GetThemeLink() Color                       { return active().GetThemeLink() }
func GetThemePrimary() Color                    { return active().GetThemePrimary() }
func GetThemeOnPrimary() Color                  { return active().GetThemeOnPrimary() }
func GetThemeSurfaceVariant() Color             { return active().GetThemeSurfaceVariant() }
func Bevel(x, y, w, h int32, light, dark Color) { active().Bevel(x, y, w, h, light, dark) }
func Icon(id, x, y, size int32, iconType int32, tint Color) {
	active().Icon(id, x, y, size, iconType, tint)
}
func Image(props ImageProps) { active().Image(props) }
func Paragraph(spec ParagraphSpec, x int32, y *int32) {
	active().Paragraph(spec, x, y)
}
func Card(props CardProps) bool     { return active().Card(props) }
func Toggle(props ToggleProps) bool { return active().Toggle(props) }
func Modal(props ModalProps) int32 {
	return active().Modal(props)
}
func TitleBar(props TitleBarProps) int32                   { return active().TitleBar(props) }
func NavigationBar(props NavigationBarProps)               { active().NavigationBar(props) }
func Toolbar(props ToolbarProps) ToolbarResult             { return active().Toolbar(props) }
func Menu(props MenuProps) MenuResult                      { return active().Menu(props) }
func CanvasGrid(bounds Rectangle, step int32, color Color) { active().CanvasGrid(bounds, step, color) }
func Toast(props ToastProps)                               { active().Toast(props) }
func TextField(args ...any) bool                           { return textField(args...) }
func TextArea(props TextAreaProps) bool                    { return active().TextArea(props) }
func Radio(props RadioProps) int32                         { return active().Radio(props) }
func Spinbox(props SpinboxProps) bool                      { return active().Spinbox(props) }
func Fieldset(props FieldsetProps)                         { active().Fieldset(props) }
func PanedView(props PanedViewProps) int32                 { return active().PanedView(props) }
func Collapsible(props CollapsibleProps) int32             { return active().Collapsible(props) }
func TreeView(props TreeViewProps) int32                   { return active().TreeView(props) }
func ListBox(props ListBoxProps) int32                     { return active().ListBox(props) }
func TableView(props TableViewProps) int32                 { return active().TableView(props) }
func BeginCanvas(canvas Canvas) CanvasResult               { return active().BeginCanvas(canvas) }
func EndCanvas(canvas Canvas)                              { active().EndCanvas(canvas) }
func SetCurrentTheme(themeID, darkMode int32)              { active().SetCurrentTheme(themeID, darkMode) }
func SetTheme(theme Theme)                                 { active().SetTheme(theme) }
func SetThemeFamily(family ThemeFamily)                    { active().SetThemeFamily(family) }
func GetThemeFamily() ThemeFamily                          { return active().GetThemeFamily() }
func GetTheme() Theme                                      { return active().GetTheme() }
func SetThemeDarkMode(dark int32)                          { active().SetThemeDarkMode(dark) }
func SetThemeStyle(style ThemeStyle)                       { active().SetThemeStyle(style) }
func SetThemeSource(source ThemeSource)                    { active().SetThemeSource(source) }
func SetThemeMode(mode ThemeMode)                          { active().SetThemeMode(mode) }
func GetThemeMode() ThemeMode                              { return active().GetThemeMode() }
func GetThemeScheme() DefaultScheme                        { return active().GetThemeScheme() }
func SystemThemePrefersDark() bool                         { return systemPrefersDark() }
