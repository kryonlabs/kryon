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
func BeginCombo(props ComboProps) bool { return active().BeginCombo(props) }
func EndCombo()                        { active().EndCombo() }
func CloseCombo()                      { active().CloseCombo() }
func BeginPopup(props PopupProps) bool { return active().BeginPopup(props) }
func EndPopup()                        { active().EndPopup() }
func ClosePopup()                      { active().ClosePopup() }
func BeginTabBar(props TabBarProps, selectedIndex *int32) bool {
	return active().BeginTabBar(props, selectedIndex)
}
func BeginTabItem(index int32) bool { return active().BeginTabItem(index) }
func EndTabItem()                   { active().EndTabItem() }
func EndTabBar()                    { active().EndTabBar() }
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
func Background(c Color)      { active().Background(c) }
func Text(props TextProps)    { active().Text(props) }
func MeasureTextWidth(text string, font int32, typeface string) int32 {
	return active().MeasureTextWidth(text, font, typeface)
}
func ReadActivation(bounds Rectangle, id int32, enabled bool) Activation {
	return active().ReadActivation(bounds, id, enabled)
}
func LabelText(label, value string, bounds Rectangle, fontSize int32, color Color) {
	active().LabelText(label, value, bounds, fontSize, color)
}
func BulletText(text string, bounds Rectangle, fontSize int32, color Color) {
	active().BulletText(text, bounds, fontSize, color)
}
func ValueBool(prefix string, value bool, bounds Rectangle, fontSize int32, color Color) {
	active().ValueBool(prefix, value, bounds, fontSize, color)
}
func ValueInt(prefix string, value int32, bounds Rectangle, fontSize int32, color Color) {
	active().ValueInt(prefix, value, bounds, fontSize, color)
}
func ValueUInt(prefix string, value uint32, bounds Rectangle, fontSize int32, color Color) {
	active().ValueUInt(prefix, value, bounds, fontSize, color)
}
func ValueFloat(prefix string, value float32, format string, bounds Rectangle, fontSize int32, color Color) {
	active().ValueFloat(prefix, value, format, bounds, fontSize, color)
}
func TextFormat(format string, args ...any) string { return active().TextFormat(format, args...) }
func Scale(px int32) int32                         { return active().Scale(px) }
func GetFontSize() int32                           { return Scale(Text16) }
func GetSmallFontSize() int32                      { return Scale(Text14) }
func GetTitleFontSize(title string, maxWidth int32) int32 {
	large := Scale(Text24)
	medium := Scale(Text16)
	small := Scale(Text14)
	if maxWidth <= 0 || int32(runtimeTextWidth(title, large)) <= maxWidth {
		return large
	}
	if int32(runtimeTextWidth(title, medium)) <= maxWidth {
		return medium
	}
	return small
}
func FitFontSize(text string, maxWidth, preferredSize, minSize int32) int32 {
	minimum := Scale(Text8)
	caption := Scale(Text12)
	small := Scale(Text14)
	body := Scale(Text16)
	normalize := func(size int32) int32 {
		switch size {
		case Text8:
			return minimum
		case Text12:
			return caption
		case Text14:
			return small
		case Text16:
			return body
		case Text24:
			return Scale(Text24)
		}
		if size <= minimum {
			return minimum
		}
		if size <= caption {
			return caption
		}
		if size <= small {
			return small
		}
		if size <= body {
			return body
		}
		return Scale(Text24)
	}
	fontSize := normalize(preferredSize)
	minAllowed := normalize(minSize)
	if fontSize < minAllowed {
		fontSize = minAllowed
	}
	for fontSize > minAllowed && int32(runtimeTextWidth(text, fontSize)) > maxWidth {
		switch {
		case fontSize > body:
			fontSize = body
		case fontSize > small:
			fontSize = small
		case fontSize > caption:
			fontSize = caption
		default:
			fontSize = minimum
		}
	}
	return fontSize
}
func GetScreenWidth() int32     { return active().GetScreenWidth() }
func GetScreenHeight() int32    { return active().GetScreenHeight() }
func GetThemeBackground() Color { return active().GetThemeBackground() }
func GetThemeText() Color       { return active().GetThemeText() }
func GetThemeIcon() Color       { return active().GetThemeIcon() }
func DrawCircleV(center Vector2, radius any, color Color) {
	active().DrawCircleV(center, radius, color)
}
func DrawRing(center Vector2, innerRadius, outerRadius, startAngle, endAngle any, segments int32, color Color) {
	active().DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments, color)
}
func Rect(x, y, w, h int32, color Color, rest ...Color) { active().Rect(x, y, w, h, color, rest...) }
func Surface(bounds Rectangle, style Style)             { active().Surface(bounds, style) }
func RectGradientH(x, y, w, h int32, left, right Color) {
	active().RectGradientH(x, y, w, h, left, right)
}
func Line(x1, y1, x2, y2 int32, color Color) { active().Line(x1, y1, x2, y2, color) }
func Scroll(x, y, w, h, contentH int32, offset *int32) {
	active().Scroll(x, y, w, h, contentH, offset)
}
func EndScroll()                                { active().EndScroll() }
func BeginListBox(props ListBoxProps) Rectangle { return active().BeginListBox(props) }
func EndListBox()                               { active().EndListBox() }
func BeginTableCell(props TableViewProps, row, column int32) Rectangle {
	return active().BeginTableCell(props, row, column)
}
func EndTableCell() { active().EndTableCell() }
func BeginScroll(bounds Rectangle, contentHeight int32, offset *int32) Rectangle {
	return active().BeginScroll(bounds, contentHeight, offset)
}
func Button(props ButtonProps) bool          { return active().Button(props) }
func BeginButton(props ButtonProps)          { active().BeginButton(props) }
func MenuButton(props MenuButtonProps) int32 { return active().MenuButton(props) }
func SplitButton(props SplitButtonProps) SplitButtonResult {
	return active().SplitButton(props)
}
func Selectable(props SelectableProps) bool            { return active().Selectable(props) }
func CheckboxFlags(props CheckboxFlagsProps) bool      { return active().CheckboxFlags(props) }
func ImageWithBg(props ImageWithBgProps)               { active().ImageWithBg(props) }
func ImageButton(props ImageButtonProps) bool          { return active().ImageButton(props) }
func TabItemButton(props TabItemButtonProps) bool      { return active().TabItemButton(props) }
func ClosableTabBar(props ClosableTabBarProps) int32   { return active().ClosableTabBar(props) }
func InvisibleButton(props InvisibleButtonProps) bool  { return active().InvisibleButton(props) }
func ArrowButton(props ArrowButtonProps) bool          { return active().ArrowButton(props) }
func Bullet(bounds Rectangle)                          { active().Bullet(bounds) }
func Separator(bounds Rectangle, vertical int32)       { active().Separator(bounds, vertical) }
func SeparatorText(props SeparatorTextProps)           { active().SeparatorText(props) }
func DragDropSource(props DragDropSourceProps) bool    { return active().DragDropSource(props) }
func DragDropTarget(props DragDropTargetProps) bool    { return active().DragDropTarget(props) }
func MultiSelectList(props MultiSelectListProps) int32 { return active().MultiSelectList(props) }
func ColorEdit3(props ColorEditProps) bool             { return active().ColorEdit3(props) }
func ColorEdit4(props ColorEditProps) bool             { return active().ColorEdit4(props) }
func ColorPicker3(props ColorEditProps) bool           { return active().ColorPicker3(props) }
func ColorPicker4(props ColorEditProps) bool           { return active().ColorPicker4(props) }
func ColorButton(props ColorButtonProps) bool          { return active().ColorButton(props) }
func TabBar(props TabBarProps) int32                   { return active().TabBar(props) }
func Progress(props ProgressBarProps) {
	active().Progress(props)
}
func PlotLines(props PlotProps)           { active().PlotLines(props) }
func PlotHistogram(props PlotProps)       { active().PlotHistogram(props) }
func DragFloat(props DragFloatProps) bool { return active().DragFloat(props) }
func DragInt(props DragIntProps) bool     { return active().DragInt(props) }
func DragFloatRange2(props DragFloatRange2Props) bool {
	return active().DragFloatRange2(props)
}
func DragIntRange2(props DragIntRange2Props) bool { return active().DragIntRange2(props) }
func SliderFloat(props SliderFloatProps) bool     { return active().SliderFloat(props) }
func SliderInt(props SliderIntProps) bool         { return active().SliderInt(props) }
func VSliderFloat(props SliderFloatProps) bool    { return active().VSliderFloat(props) }
func VSliderInt(props SliderIntProps) bool        { return active().VSliderInt(props) }
func SliderAngle(props SliderAngleProps) bool     { return active().SliderAngle(props) }
func InputFloat(props InputFloatProps) bool       { return active().InputFloat(props) }
func InputInt(props InputIntProps) bool           { return active().InputInt(props) }
func InputDouble(props InputDoubleProps) bool     { return active().InputDouble(props) }
func Checkbox(id, x, y int32, label string, value *int32) bool {
	return active().Checkbox(id, x, y, label, value)
}
func Dropdown(id, x, y, w, h int32, options any, rest ...any) bool {
	return active().Dropdown(id, x, y, w, h, options, rest...)
}
func Column(props ColumnProps)  { active().Column(props) }
func Row(props ColumnProps)     { active().Row(props) }
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
func Link(props LinkProps) bool                      { return active().Link(props) }
func PagePicture(props PictureProps, altText string) { active().PagePicture(props, altText) }
func Flow(props FlowProps)                           { active().Flow(props) }
func Fade(c Color, alpha float32) Color              { return active().Fade(c, alpha) }
func GetThemeSurface() Color                         { return active().GetThemeSurface() }
func GetThemeBorder() Color                          { return active().GetThemeBorder() }
func GetThemeButton() Color                          { return active().GetThemeButton() }
func GetThemeButtonHover() Color                     { return active().GetThemeButtonHover() }
func GetThemeLink() Color                            { return active().GetThemeLink() }
func GetThemePrimary() Color                         { return active().GetThemePrimary() }
func GetThemeOnPrimary() Color                       { return active().GetThemeOnPrimary() }
func GetThemeSurfaceVariant() Color                  { return active().GetThemeSurfaceVariant() }
func TextLines(lines any, count int32, x int32, y *int32, font, lineH int32, color Color) {
	active().TextLines(lines, count, x, y, font, lineH, color)
}
func Bevel(x, y, w, h int32, light, dark Color) { active().Bevel(x, y, w, h, light, dark) }
func Icon(id, x, y, size int32, iconType int32, tint Color) {
	active().Icon(id, x, y, size, iconType, tint)
}
func Picture(props PictureProps) { active().Picture(props) }
func Paragraph(spec ParagraphSpec, x int32, y *int32) {
	active().Paragraph(spec, x, y)
}
func Card(props CardProps) bool { return active().Card(props) }
func BeginCard(props CardProps) { active().BeginCard(props) }
func Href(props HrefProps) bool { return active().Href(props) }
func Slider(id, x, y, w int32, label string, min, max int32, value *int32, rest ...any) bool {
	return active().Slider(id, x, y, w, label, min, max, value, rest...)
}
func Toggle(id, x, y, w, h int32, value *int32, offLabel, onLabel string) bool {
	return active().Toggle(id, x, y, w, h, value, offLabel, onLabel)
}
func Modal(title, message, cancelBtn, confirmBtn string) int {
	return active().Modal(title, message, cancelBtn, confirmBtn)
}
func TitleBar(title string, height int32)      { active().TitleBar(title, height) }
func BottomNav(props BottomNavProps)           { active().BottomNav(props) }
func TopNav(props TopNavProps)                 { active().TopNav(props) }
func Toolbar(props ToolbarProps) ToolbarResult { return active().Toolbar(props) }
func MenuBar(id int32, bounds Rectangle, menus []Menu, args ...any) MenuBarResult {
	var openIndex *int32
	for _, arg := range args {
		if value, ok := arg.(*int32); ok {
			openIndex = value
		}
	}
	return active().MenuBar(id, bounds, menus, openIndex)
}
func PopupMenu(id, x, y int32, items []MenuItem, itemCount int32) int32 {
	return active().PopupMenu(id, x, y, items, itemCount)
}
func ContextMenu(props ContextMenuProps) int32             { return active().ContextMenu(props) }
func CanvasGrid(bounds Rectangle, step int32, color Color) { active().CanvasGrid(bounds, step, color) }
func SelectableText(value string, x, y, fontSize int32, color Color) {
	active().SelectableText(value, x, y, fontSize, color)
}
func ShowToast(message string)                     { active().ShowToast(message) }
func ShowToastFor(message string, seconds float64) { active().ShowToastFor(message, seconds) }
func TextField(args ...any) bool                   { return textField(args...) }
func TextArea(props TextAreaProps) bool            { return active().TextArea(props) }
func Radio(props RadioButtonProps) int32           { return active().Radio(props) }
func Spinbox(props SpinboxProps) bool              { return active().Spinbox(props) }
func Combobox(props ComboboxProps) bool            { return active().Combobox(props) }
func LabelFrame(props LabelFrameProps)             { active().LabelFrame(props) }
func Notebook(props NotebookProps) int32           { return active().Notebook(props) }
func PanedView(props PanedViewProps) int32         { return active().PanedView(props) }
func Collapsible(props CollapsibleProps) int32     { return active().Collapsible(props) }
func ColorPicker(bounds Rectangle, color *Color) bool {
	return active().ColorPicker(bounds, color)
}
func TreeView(props TreeViewProps) int32           { return active().TreeView(props) }
func ListBox(props ListBoxProps) int32             { return active().ListBox(props) }
func SourceView(props SourceViewProps) int32       { return active().SourceView(props) }
func TableView(props TableViewProps) int32         { return active().TableView(props) }
func MessageDialog(props MessageDialogProps) int32 { return active().MessageDialog(props) }
func ConfirmDialog(props ConfirmDialogProps) int32 { return active().ConfirmDialog(props) }
func PromptDialog(props PromptDialogProps) int32   { return active().PromptDialog(props) }
func BeginCanvas(canvas Canvas) CanvasResult       { return active().BeginCanvas(canvas) }
func EndCanvas(canvas Canvas)                      { active().EndCanvas(canvas) }
func BeginFrameBox(bounds Rectangle, padX, padY, gap int32) FrameBox {
	return active().BeginFrameBox(bounds, padX, padY, gap)
}
func FramePack(frame *FrameBox, side Side, size int32) Rectangle {
	return active().FramePack(frame, side, size)
}
func GridCell(grid GridFrame, row, col, rowSpan, colSpan int32) Rectangle {
	return active().GridCell(grid, row, col, rowSpan, colSpan)
}
func Place(parent Rectangle, x, y, w, h int32) Rectangle {
	return active().Place(parent, x, y, w, h)
}
func SetCurrentTheme(themeID, darkMode int32) { active().SetCurrentTheme(themeID, darkMode) }
func SetTheme(theme Theme)                    { active().SetTheme(theme) }
func SetThemeFamily(family ThemeFamily)       { active().SetThemeFamily(family) }
func GetThemeFamily() ThemeFamily             { return active().GetThemeFamily() }
func GetTheme() Theme                         { return active().GetTheme() }
func SetThemeDarkMode(dark int32)             { active().SetThemeDarkMode(dark) }
func SetThemeStyle(style ThemeStyle)          { active().SetThemeStyle(style) }
func SetThemeSource(source ThemeSource)       { active().SetThemeSource(source) }
func SetThemeMode(mode ThemeMode)             { active().SetThemeMode(mode) }
func GetThemeMode() ThemeMode                 { return active().GetThemeMode() }
func GetThemeScheme() DefaultScheme           { return active().GetThemeScheme() }
func SystemThemePrefersDark() bool            { return systemPrefersDark() }
