package kryon

var activeRuntime Runtime

func Open(config AppConfig) Runtime {
	activeRuntime = New(config)
	return activeRuntime
}

func SetRuntime(runtime Runtime) {
	activeRuntime = runtime
}

type focusController interface {
	SetFocus(int32)
	Focus() int32
}

type pointerController interface {
	QueueTap(float32, float32)
}

type inputController interface {
	QueueText(string)
	QueueKey(int32)
	QueueShiftKey(int32)
	QueueShortcut(int32)
}

type clipboardController interface {
	SetClipboardText(string)
	ClipboardText() string
}

type selectionController interface {
	SetSelection(int32, int32, int32)
	Selection(int32) (int32, int32, bool)
}

func active() Runtime {
	if activeRuntime == nil {
		activeRuntime = New(AppConfig{})
	}
	return activeRuntime
}

func Close()                  { active().Close() }
func WindowShouldClose() bool { return active().WindowShouldClose() }
func BeginFrame()             { active().BeginFrame() }
func EndFrame()               { active().EndFrame() }
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
func Background(c Color)      { active().Background(c) }
func Text(text string, x, y, fontSize int32, color Color) {
	active().Text(text, x, y, fontSize, color)
}
func TextFormat(format string, args ...any) string { return active().TextFormat(format, args...) }
func ScaleUIPx(px int32) int32                     { return active().ScaleUIPx(px) }
func GetScreenWidth() int32                        { return active().GetScreenWidth() }
func GetScreenHeight() int32                       { return active().GetScreenHeight() }
func GetThemeBackground() Color                    { return active().GetThemeBackground() }
func GetThemeText() Color                          { return active().GetThemeText() }
func GetThemeIcon() Color                          { return active().GetThemeIcon() }
func DrawCircleV(center Vector2, radius any, color Color) {
	active().DrawCircleV(center, radius, color)
}
func DrawRing(center Vector2, innerRadius, outerRadius, startAngle, endAngle any, segments int32, color Color) {
	active().DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments, color)
}
func Rect(x, y, w, h int32, color Color, rest ...Color) { active().Rect(x, y, w, h, color, rest...) }
func RectGradientH(x, y, w, h int32, left, right Color) {
	active().RectGradientH(x, y, w, h, left, right)
}
func Line(x1, y1, x2, y2 int32, color Color) { active().Line(x1, y1, x2, y2, color) }
func Scroll(x, y, w, h, contentH int32, offset *int32) {
	active().Scroll(x, y, w, h, contentH, offset)
}
func EndScroll()              { active().EndScroll() }
func Button(args ...any) bool { return button(args...) }
func TabBar(bounds Rectangle, labels []string, selected, hover *int32) int32 {
	return active().TabBar(bounds, labels, selected, hover)
}
func Progress(bounds Rectangle, min, max, value int32, suffix string) {
	active().Progress(bounds, min, max, value, suffix)
}
func Checkbox(id, x, y int32, label string, value *int32) bool {
	return active().Checkbox(id, x, y, label, value)
}
func Dropdown(id, x, y, w, h int32, options any, rest ...any) bool {
	return active().Dropdown(id, x, y, w, h, options, rest...)
}
func BeginUI(key UIKey)                 { active().BeginUI(key) }
func EndUI()                            { active().EndUI() }
func Column(props ColumnProps)          { active().Column(props) }
func Row(props ColumnProps)             { active().Row(props) }
func Stack(props ColumnProps)           { active().Stack(props) }
func End()                              { active().End() }
func Fade(c Color, alpha float32) Color { return active().Fade(c, alpha) }
func GetThemeSurface() Color            { return active().GetThemeSurface() }
func GetThemeButton() Color             { return active().GetThemeButton() }
func GetThemeButtonHover() Color        { return active().GetThemeButtonHover() }
func GetThemeLink() Color               { return active().GetThemeLink() }
func TextInRect(text string, rect Rectangle, fontSize int32, color Color) {
	active().TextInRect(text, rect, fontSize, color)
}
func TextLines(lines any, count int32, x int32, y *int32, font, lineH int32, color Color) {
	active().TextLines(lines, count, x, y, font, lineH, color)
}
func Bevel(x, y, w, h int32, light, dark Color) { active().Bevel(x, y, w, h, light, dark) }
func IconTexture(id, x, y, size int32, iconType int32, tint Color) {
	active().IconTexture(id, x, y, size, iconType, tint)
}
func Picture(props PictureProps) { active().Picture(props) }
func Paragraph(spec UIParagraphSpec, x int32, y *int32) {
	active().Paragraph(spec, x, y)
}
func IconButton(props IconButtonProps) bool { return active().IconButton(props) }
func Href(props HrefProps) bool             { return active().Href(props) }
func Slider(id, x, y, w int32, label string, min, max int32, value *int32, rest ...any) bool {
	return active().Slider(id, x, y, w, label, min, max, value, rest...)
}
func Toggle(id, x, y, w, h int32, value *int32, offLabel, onLabel string) bool {
	return active().Toggle(id, x, y, w, h, value, offLabel, onLabel)
}
func Modal(title, message, cancelBtn, confirmBtn string) int {
	return active().Modal(title, message, cancelBtn, confirmBtn)
}
func TitleBar(title string, height int32)                  { active().TitleBar(title, height) }
func BottomNav(props BottomNavProps)                       { active().BottomNav(props) }
func TopNav(props TopNavProps)                             { active().TopNav(props) }
func Toolbar(props ToolbarProps)                           { active().Toolbar(props) }
func CanvasGrid(bounds Rectangle, step int32, color Color) { active().CanvasGrid(bounds, step, color) }
func SelectableText(value string, x, y, fontSize int32, color Color) {
	active().SelectableText(value, x, y, fontSize, color)
}
func ShowUIToast(message string)                     { active().ShowUIToast(message) }
func ShowUIToastFor(message string, seconds float64) { active().ShowUIToastFor(message, seconds) }
func TextField(args ...any) bool                     { return textField(args...) }
func TextArea(props TextAreaProps) bool              { return active().TextArea(props) }
func Radio(props RadioButtonProps) int32             { return active().Radio(props) }
func Spinbox(props SpinboxProps) bool                { return active().Spinbox(props) }
func Combobox(props ComboboxProps) bool              { return active().Combobox(props) }
func LabelFrame(props LabelFrameProps)               { active().LabelFrame(props) }
func Notebook(props NotebookProps) int32             { return active().Notebook(props) }
func PanedView(props PanedViewProps) int32           { return active().PanedView(props) }
func Collapsible(props CollapsibleProps) int32       { return active().Collapsible(props) }
func ListBox(props ListBoxProps) int32               { return active().ListBox(props) }
func SourceView(props SourceViewProps) int32         { return active().SourceView(props) }
func TableView(props TableViewProps) int32           { return active().TableView(props) }
func MessageDialog(props MessageDialogProps) int32   { return active().MessageDialog(props) }
func ConfirmDialog(props ConfirmDialogProps) int32   { return active().ConfirmDialog(props) }
func PromptDialog(props PromptDialogProps) int32     { return active().PromptDialog(props) }
func BeginUICanvas(canvas UICanvas) UICanvasResult   { return active().BeginUICanvas(canvas) }
func EndUICanvas(canvas UICanvas)                    { active().EndUICanvas(canvas) }
func BeginUIFrameBox(bounds Rectangle, padX, padY, gap int32) UIFrame {
	return active().BeginUIFrameBox(bounds, padX, padY, gap)
}
func UIFramePack(frame *UIFrame, side UISide, size int32) Rectangle {
	return active().UIFramePack(frame, side, size)
}
func UIGridCell(grid UIGrid, row, col, rowSpan, colSpan int32) Rectangle {
	return active().UIGridCell(grid, row, col, rowSpan, colSpan)
}
func UIPlace(parent Rectangle, x, y, w, h int32) Rectangle {
	return active().UIPlace(parent, x, y, w, h)
}
func SetCurrentTheme(themeID, darkMode int32) { active().SetCurrentTheme(themeID, darkMode) }
func SetThemeDarkMode(dark int32)             { active().SetThemeDarkMode(dark) }
func SetThemeStyle(style ThemeStyle)          { active().SetThemeStyle(style) }
func SetThemeSource(source ThemeSource)       { active().SetThemeSource(source) }
func SetThemeMode(mode ThemeMode)             { active().SetThemeMode(mode) }
