package kryon

import (
	"fmt"
	"hash/fnv"
	"reflect"
	"strings"
	"unicode/utf8"
)

type AppConfig struct {
	Title         string
	Width, Height int
	FPS           int
	Flags         uint
	MinWidth      int
	MinHeight     int
}

type Vector2 struct {
	X, Y float32
}

type Rectangle struct {
	X, Y, Width, Height float32
}

type Color struct {
	R, G, B, A uint8
}

type Texture2D struct {
	ID      uint32
	Width   int32
	Height  int32
	Mipmaps int32
	Format  int32
}

type UIKey uint64
type UISide int32
type UIButtonStyle int32
type UISyntaxMode int32
type ThemeStyle int32
type ThemeSource int32
type ThemeMode int32
type PictureFit int32

const (
	FlagVsyncHint       uint = 0x00000040
	FlagWindowResizable uint = 0x00000004

	KeyNull      int32 = 0
	KeyEnter     int32 = 257
	KeyBackspace int32 = 259
	KeyDelete    int32 = 261
	KeyRight     int32 = 262
	KeyLeft      int32 = 263
	KeyHome      int32 = 268
	KeyEnd       int32 = 269

	UISideTop UISide = iota
	UISideBottom
	UISideLeft
	UISideRight

	UIButtonStylePrimary UIButtonStyle = iota
	UIButtonStyleSecondary
	UIButtonStyleDanger
	UIButtonStyleTab
	UIButtonStyleTabSelected

	UISyntaxNone UISyntaxMode = iota
	UISyntaxKry
	UISyntaxC
	UISyntaxMake

	ThemeStyleSystem ThemeStyle = iota
	ThemeStyleRetro
	ThemeStyleMaterial

	ThemeSourceApp ThemeSource = iota
	ThemeSourceSystem

	ThemeModeSystem ThemeMode = iota
	ThemeModeLight
	ThemeModeDark

	PictureFitStretch PictureFit = iota
	PictureFitContain
	PictureFitCover

	Text8  int32 = 8
	Text12 int32 = 12
	Text14 int32 = 14
	Text16 int32 = 16
	Text18 int32 = 18
	Text20 int32 = 20
	Text24 int32 = 24
	Text32 int32 = 32
	Text48 int32 = 48

	UI_BUTTON_STYLE_PRIMARY   = UIButtonStylePrimary
	UI_BUTTON_STYLE_SECONDARY = UIButtonStyleSecondary
	UI_BUTTON_STYLE_DANGER    = UIButtonStyleDanger

	THEME_STYLE_SYSTEM   = ThemeStyleSystem
	THEME_STYLE_RETRO    = ThemeStyleRetro
	THEME_STYLE_MATERIAL = ThemeStyleMaterial

	PICTURE_FIT_STRETCH = PictureFitStretch
	PICTURE_FIT_CONTAIN = PictureFitContain
	PICTURE_FIT_COVER   = PictureFitCover
)

var (
	LIGHTGRAY  = Color{200, 200, 200, 255}
	GRAY       = Color{130, 130, 130, 255}
	DARKGRAY   = Color{80, 80, 80, 255}
	YELLOW     = Color{253, 249, 0, 255}
	GOLD       = Color{255, 203, 0, 255}
	ORANGE     = Color{255, 161, 0, 255}
	PINK       = Color{255, 109, 194, 255}
	RED        = Color{230, 41, 55, 255}
	MAROON     = Color{190, 33, 55, 255}
	GREEN      = Color{0, 228, 48, 255}
	LIME       = Color{0, 158, 47, 255}
	DARKGREEN  = Color{0, 117, 44, 255}
	SKYBLUE    = Color{102, 191, 255, 255}
	BLUE       = Color{0, 121, 241, 255}
	DARKBLUE   = Color{0, 82, 172, 255}
	PURPLE     = Color{200, 122, 255, 255}
	VIOLET     = Color{135, 60, 190, 255}
	DARKPURPLE = Color{112, 31, 126, 255}
	BEIGE      = Color{211, 176, 131, 255}
	BROWN      = Color{127, 106, 79, 255}
	DARKBROWN  = Color{76, 63, 50, 255}
	WHITE      = Color{255, 255, 255, 255}
	BLACK      = Color{0, 0, 0, 255}
	BLANK      = Color{0, 0, 0, 0}
	MAGENTA    = Color{255, 0, 255, 255}
	RAYWHITE   = Color{245, 245, 245, 255}
	White      = WHITE
	Black      = BLACK
)

type TextInputStyle struct {
	Background  Color
	Border      Color
	FocusBorder Color
	Text        Color
	Cursor      Color
	Radius      float32
	PaddingX    int32
	PaddingY    int32
}

type ButtonProps struct {
	Bounds   Rectangle
	Label    string
	Style    UIButtonStyle
	Font     int32
	ID       int32
	Disabled bool
}

type IconButtonProps struct {
	Bounds          Rectangle
	Icon            Texture2D
	IconType        int32
	IconSize        int32
	IconPadding     int32
	FocusID         int32
	Disabled        bool
	Background      Color
	HoverBackground Color
	IconColor       Color
	Border          Color
	Radius          float32
}

type HrefProps struct {
	Bounds     Rectangle
	Text       string
	Href       string
	Font       int32
	FocusID    int32
	Disabled   bool
	Color      Color
	HoverColor Color
}

type TextFieldProps struct {
	Bounds         Rectangle
	Text           []byte
	CursorPosition *int32
	Focused        *bool
	MaxCodepoints  int32
	Font           int32
	FocusID        int32
	Style          TextInputStyle
	CommitPressed  *bool
	Secure         bool
}

type TextAreaProps struct {
	Bounds         Rectangle
	Text           []byte
	CursorPosition *int32
	Focused        *bool
	ScrollY        *int32
	MaxCodepoints  int32
	Font           int32
	LineGap        int32
	FocusID        int32
	Placeholder    string
	Syntax         UISyntaxMode
	Style          TextInputStyle
	ContentVersion int32
}

type ReadonlyTextBoxProps struct {
	Bounds  Rectangle
	Text    string
	Font    int32
	Style   TextInputStyle
	LineGap int32
}

type ColumnProps struct {
	Bounds  Rectangle
	Gap     int32
	Padding int32
	Key     UIKey
}

type RowProps = ColumnProps

type UIFrame struct {
	Bounds  Rectangle
	PadX    int32
	PadY    int32
	Gap     int32
	CursorX int32
	CursorY int32
}

type UIGrid struct {
	Bounds Rectangle
	Rows   int32
	Cols   int32
	GapX   int32
	GapY   int32
	PadX   int32
	PadY   int32
}

type UIParagraphSpec struct {
	Text     string
	IconType int32
	IconSize int32
	Width    int32
	Font     int32
	LineGap  int32
	Color    Color
}

type PictureProps struct {
	AssetPath string
	Bounds    Rectangle
	Source    Rectangle
	Origin    Vector2
	Rotation  float32
	Tint      Color
	Fit       PictureFit
}

type BottomNavItem struct {
	Route    int32
	Label    string
	Icon     Texture2D
	Active   bool
	Disabled bool
}

type BottomNavProps struct {
	ViewWidth      int32
	ViewHeight     int32
	Count          int32
	Items          []BottomNavItem
	Height         int32
	IconSize       int32
	IconPadding    int32
	SideMargin     int32
	BottomMargin   int32
	MaxButtonWidth int32
}

type TopNavProps struct {
	ID                int32
	X, Y              int32
	Width, Height     int32
	Title             string
	Options           string
	OptionCount       int32
	SelectedIndex     *int32
	Disabled          bool
	DropdownMinWidth  int32
	DropdownHeight    int32
	ActionIconSize    int32
	ActionIconPadding int32
	ActionGap         int32
	SidePadding       int32
}

type ToolbarProps struct {
	ID                int32
	X, Y              int32
	Width, Height     int32
	DrawMenu          bool
	Options           string
	OptionCount       int32
	SelectedIndex     *int32
	DropdownMinWidth  int32
	DropdownMaxWidth  int32
	DropdownHeight    int32
	ActionIconSize    int32
	ActionIconPadding int32
	ActionGap         int32
	SidePadding       int32
}

type RadioButtonProps struct {
	Bounds   Rectangle
	Label    string
	ID       int32
	Checked  bool
	Disabled bool
}

type ProgressBarProps struct {
	Bounds Rectangle
	Min    int32
	Max    int32
	Value  int32
	Label  string
}

type SpinboxProps struct {
	Bounds    Rectangle
	ID        int32
	Min       int32
	Max       int32
	Step      int32
	Value     *int32
	Disabled  bool
	ValueText string
	Wrap      bool
}

type ComboboxProps struct {
	Bounds        Rectangle
	ID            int32
	Options       []string
	SelectedIndex *int32
	Disabled      bool
}

type LabelFrameProps struct {
	Bounds Rectangle
	Title  string
}

type ListBoxProps struct {
	Bounds        Rectangle
	ID            int32
	Items         []string
	SelectedIndex *int32
	ScrollOffset  *int32
	RowHeight     int32
}

type SourceViewProps struct {
	Bounds          Rectangle
	Text            string
	ScrollX         *int32
	ScrollY         *int32
	FontSize        int32
	LineHeight      int32
	ShowLineNumbers bool
}

type UITableRow struct {
	Cells []string
}

type TableViewProps struct {
	Bounds       Rectangle
	ID           int32
	Columns      []string
	Rows         []UITableRow
	ColumnWidths []int32
	SelectedRow  *int32
	SortColumn   *int32
	ScrollOffset *int32
	RowHeight    int32
}

type NotebookProps struct {
	Bounds        Rectangle
	Tabs          []string
	SelectedIndex *int32
}

type PanedViewProps struct {
	Bounds    Rectangle
	ID        int32
	Vertical  bool
	Split     *int32
	MinFirst  int32
	MinSecond int32
}

type CollapsibleProps struct {
	Bounds Rectangle
	Label  string
	Open   *bool
}

type MessageDialogProps struct {
	Title   string
	Message string
	OKLabel string
}

type ConfirmDialogProps struct {
	Title        string
	Message      string
	CancelLabel  string
	ConfirmLabel string
}

type PromptDialogProps struct {
	Title        string
	Text         []byte
	Cursor       *int32
	Focused      *bool
	CancelLabel  string
	ConfirmLabel string
}

type UICanvas struct {
	Bounds  Rectangle
	ScrollX *int32
	ScrollY *int32
	Zoom    *float32
}

type UICanvasResult struct {
	Active        bool
	Dragging      bool
	SelectedIndex int32
	World         Vector2
}

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
	Rect(int32, int32, int32, int32, Color, ...Color)
	RectGradientH(int32, int32, int32, int32, Color, Color)
	Line(int32, int32, int32, int32, Color)
	Scroll(int32, int32, int32, int32, int32, *int32)
	EndScroll()
	Button(ButtonProps) bool
	TabBar(Rectangle, []string, *int32, *int32) int32
	Progress(Rectangle, int32, int32, int32, string)
	Checkbox(int32, int32, int32, string, *int32) bool
	Dropdown(id, x, y, w, h int32, options any, rest ...any) bool
	BeginUI(UIKey)
	EndUI()
	Column(ColumnProps)
	Row(ColumnProps)
	Stack(ColumnProps)
	End()
	TextField(TextFieldProps)
	Key(text string) UIKey
	Fade(Color, float32) Color
	GetThemeSurface() Color
	GetThemeButton() Color
	GetThemeButtonHover() Color
	GetThemeLink() Color
	TextInRect(text string, rect Rectangle, fontSize int32, color Color)
	TextLines(lines any, count int32, x int32, y *int32, font, lineH int32, color Color)
	Bevel(x, y, w, h int32, light, dark Color)
	IconTexture(id, x, y, size int32, iconType int32, tint Color)
	Picture(props PictureProps)
	Paragraph(spec UIParagraphSpec, x int32, y *int32)
	IconButton(props IconButtonProps) bool
	Href(props HrefProps) bool
	Slider(id, x, y, w int32, label string, min, max int32, value *int32, rest ...any) bool
	Toggle(id, x, y, w, h int32, value *int32, offLabel, onLabel string) bool
	Modal(title, message, cancelBtn, confirmBtn string) int
	TitleBar(title string, height int32)
	BottomNav(props BottomNavProps)
	TopNav(props TopNavProps)
	Toolbar(props ToolbarProps)
	GenericButton(id, x, y, w, h int32, label string, style UIButtonStyle, disabled int32, hover *int32) bool
	TextButton(id, x, y int32, label string, hover *int32) bool
	LocaleDropdown(id, x, y, w, h int32, selected *int32) bool
	VerticalSlider(id, x, y, h, min, max int32, value *int32) bool
	CanvasGrid(bounds Rectangle, step int32, color Color)
	SelectableText(value string, x, y, fontSize int32, color Color)
	ShowUIToast(message string)
	ShowUIToastFor(message string, seconds float64)
	ReadonlyTextBox(props ReadonlyTextBoxProps)
	TextArea(props TextAreaProps) bool
	Radio(props RadioButtonProps) int32
	Spinbox(props SpinboxProps) bool
	Combobox(props ComboboxProps) bool
	LabelFrame(props LabelFrameProps)
	Notebook(props NotebookProps) int32
	PanedView(props PanedViewProps) int32
	Collapsible(props CollapsibleProps) int32
	ListBox(props ListBoxProps) int32
	SourceView(props SourceViewProps) int32
	TableView(props TableViewProps) int32
	MessageDialog(props MessageDialogProps) int32
	ConfirmDialog(props ConfirmDialogProps) int32
	PromptDialog(props PromptDialogProps) int32
	BeginUICanvas(canvas UICanvas) UICanvasResult
	EndUICanvas(canvas UICanvas)
	BeginUIFrameBox(bounds Rectangle, padX, padY, gap int32) UIFrame
	UIFramePack(frame *UIFrame, side UISide, size int32) Rectangle
	UIGridCell(grid UIGrid, row, col, rowSpan, colSpan int32) Rectangle
	UIPlace(parent Rectangle, x, y, w, h int32) Rectangle
	SetCurrentTheme(themeID int32, darkMode int32)
	SetThemeDarkMode(dark int32)
	SetThemeStyle(style ThemeStyle)
	SetThemeSource(source ThemeSource)
	SetThemeMode(mode ThemeMode)
}

type runtime struct {
	config    AppConfig
	closed    bool
	frames    int
	focusID   int32
	inputText []rune
	inputKeys []int32
}

func New(config AppConfig) Runtime {
	if config.Width <= 0 {
		config.Width = 640
	}
	if config.Height <= 0 {
		config.Height = 480
	}
	return &runtime{config: config}
}

func (r *runtime) QueueText(text string) { r.inputText = append(r.inputText, []rune(text)...) }
func (r *runtime) QueueKey(key int32)    { r.inputKeys = append(r.inputKeys, key) }

func (r *runtime) Close()                                                 { r.closed = true }
func (r *runtime) WindowShouldClose() bool                                { return r.closed || r.frames > 0 }
func (r *runtime) BeginDrawing()                                          {}
func (r *runtime) EndDrawing()                                            { r.frames++ }
func (r *runtime) ClearBackground(Color)                                  {}
func (r *runtime) Background(Color)                                       {}
func (r *runtime) Text(string, int32, int32, int32, Color)                {}
func (r *runtime) TextFormat(format string, args ...any) string           { return fmt.Sprintf(format, args...) }
func (r *runtime) ScaleUIPx(px int32) int32                               { return px }
func (r *runtime) GetScreenWidth() int32                                  { return int32(r.config.Width) }
func (r *runtime) GetScreenHeight() int32                                 { return int32(r.config.Height) }
func (r *runtime) GetThemeBackground() Color                              { return RAYWHITE }
func (r *runtime) GetThemeText() Color                                    { return BLACK }
func (r *runtime) GetThemeIcon() Color                                    { return DARKGRAY }
func (r *runtime) NewVector2(x, y any) Vector2                            { return NewVector2(number32(x), number32(y)) }
func (r *runtime) DrawCircleV(Vector2, any, Color)                        {}
func (r *runtime) DrawRing(Vector2, any, any, any, any, int32, Color)     {}
func (r *runtime) Rect(int32, int32, int32, int32, Color, ...Color)       {}
func (r *runtime) RectGradientH(int32, int32, int32, int32, Color, Color) {}
func (r *runtime) Line(int32, int32, int32, int32, Color)                 {}
func (r *runtime) Scroll(int32, int32, int32, int32, int32, *int32)       {}
func (r *runtime) EndScroll()                                             {}
func (r *runtime) Button(ButtonProps) bool                                { return false }
func (r *runtime) TabBar(Rectangle, []string, *int32, *int32) int32       { return -1 }
func (r *runtime) Progress(Rectangle, int32, int32, int32, string)        {}
func (r *runtime) Checkbox(_ int32, _ int32, _ int32, _ string, value *int32) bool {
	if value == nil {
		return false
	}
	return false
}
func (r *runtime) Dropdown(id, x, y, w, h int32, options any, rest ...any) bool {
	_, _, _, _, _ = id, x, y, w, h
	_ = labelsOf(options)
	return false
}
func (r *runtime) BeginUI(UIKey)         {}
func (r *runtime) EndUI()                {}
func (r *runtime) Column(ColumnProps)    {}
func (r *runtime) Row(ColumnProps)       {}
func (r *runtime) Stack(ColumnProps)     {}
func (r *runtime) End()                  {}
func (r *runtime) Key(text string) UIKey { return Key(text) }
func (r *runtime) Fade(c Color, alpha float32) Color {
	if alpha < 0 {
		alpha = 0
	}
	if alpha > 1 {
		alpha = 1
	}
	c.A = uint8(float32(c.A) * alpha)
	return c
}
func (r *runtime) GetThemeSurface() Color                     { return WHITE }
func (r *runtime) GetThemeButton() Color                      { return LIGHTGRAY }
func (r *runtime) GetThemeButtonHover() Color                 { return GRAY }
func (r *runtime) GetThemeLink() Color                        { return BLUE }
func (r *runtime) TextInRect(string, Rectangle, int32, Color) {}
func (r *runtime) TextLines(lines any, count int32, x int32, y *int32, font, lineH int32, color Color) {
	_, _, _, _, _ = lines, count, x, font, color
	if y != nil {
		*y += lineH * count
	}
}
func (r *runtime) Bevel(int32, int32, int32, int32, Color, Color)       {}
func (r *runtime) IconTexture(int32, int32, int32, int32, int32, Color) {}
func (r *runtime) Picture(PictureProps)                                 {}
func (r *runtime) Paragraph(spec UIParagraphSpec, x int32, y *int32) {
	_, _ = spec, x
	if y != nil {
		*y += spec.Font + spec.LineGap
	}
}
func (r *runtime) IconButton(IconButtonProps) bool { return false }
func (r *runtime) Href(HrefProps) bool             { return false }
func (r *runtime) Slider(id, x, y, w int32, label string, min, max int32, value *int32, rest ...any) bool {
	_, _, _, _, _, _, _, _, _ = id, x, y, w, label, min, max, value, rest
	return false
}
func (r *runtime) Toggle(id, x, y, w, h int32, value *int32, offLabel, onLabel string) bool {
	_, _, _, _, _, _, _, _ = id, x, y, w, h, value, offLabel, onLabel
	return false
}
func (r *runtime) Modal(title, message, cancelBtn, confirmBtn string) int {
	_, _, _, _ = title, message, cancelBtn, confirmBtn
	return 0
}
func (r *runtime) TitleBar(string, int32)   {}
func (r *runtime) BottomNav(BottomNavProps) {}
func (r *runtime) TopNav(TopNavProps)       {}
func (r *runtime) Toolbar(ToolbarProps)     {}
func (r *runtime) GenericButton(id, x, y, w, h int32, label string, style UIButtonStyle, disabled int32, hover *int32) bool {
	_, _, _, _, _, _, _, _ = id, x, y, w, h, label, style, disabled
	if hover != nil {
		*hover = 0
	}
	return false
}
func (r *runtime) TextButton(id, x, y int32, label string, hover *int32) bool {
	_, _, _, _ = id, x, y, label
	if hover != nil {
		*hover = 0
	}
	return false
}
func (r *runtime) LocaleDropdown(id, x, y, w, h int32, selected *int32) bool {
	_, _, _, _, _, _ = id, x, y, w, h, selected
	return false
}
func (r *runtime) VerticalSlider(id, x, y, h, min, max int32, value *int32) bool {
	_, _, _, _, _, _, _ = id, x, y, h, min, max, value
	return false
}
func (r *runtime) CanvasGrid(Rectangle, int32, Color)                {}
func (r *runtime) SelectableText(string, int32, int32, int32, Color) {}
func (r *runtime) ShowUIToast(string)                                {}
func (r *runtime) ShowUIToastFor(string, float64)                    {}
func (r *runtime) ReadonlyTextBox(ReadonlyTextBoxProps)              {}
func (r *runtime) TextArea(props TextAreaProps) bool {
	return r.editText(props.Text, props.CursorPosition, props.Focused, nil, props.FocusID, props.MaxCodepoints)
}
func (r *runtime) Radio(props RadioButtonProps) int32 {
	if props.Checked {
		return props.ID
	}
	return 0
}
func (r *runtime) Spinbox(SpinboxProps) bool              { return false }
func (r *runtime) Combobox(ComboboxProps) bool            { return false }
func (r *runtime) LabelFrame(LabelFrameProps)             {}
func (r *runtime) Notebook(NotebookProps) int32           { return 0 }
func (r *runtime) PanedView(PanedViewProps) int32         { return 0 }
func (r *runtime) Collapsible(CollapsibleProps) int32     { return 0 }
func (r *runtime) ListBox(ListBoxProps) int32             { return 0 }
func (r *runtime) SourceView(SourceViewProps) int32       { return 0 }
func (r *runtime) TableView(TableViewProps) int32         { return 0 }
func (r *runtime) MessageDialog(MessageDialogProps) int32 { return 0 }
func (r *runtime) ConfirmDialog(ConfirmDialogProps) int32 { return 0 }
func (r *runtime) PromptDialog(PromptDialogProps) int32   { return 0 }
func (r *runtime) BeginUICanvas(canvas UICanvas) UICanvasResult {
	return UICanvasResult{Active: true, World: Vector2{X: canvas.Bounds.X, Y: canvas.Bounds.Y}}
}
func (r *runtime) EndUICanvas(UICanvas) {}
func (r *runtime) BeginUIFrameBox(bounds Rectangle, padX, padY, gap int32) UIFrame {
	return UIFrame{Bounds: bounds, PadX: padX, PadY: padY, Gap: gap, CursorX: int32(bounds.X) + padX, CursorY: int32(bounds.Y) + padY}
}
func (r *runtime) UIFramePack(frame *UIFrame, side UISide, size int32) Rectangle {
	if frame == nil {
		return Rectangle{}
	}
	out := frame.Bounds
	switch side {
	case UISideTop:
		out.Y = float32(frame.CursorY)
		out.Height = float32(size)
		frame.CursorY += size + frame.Gap
	case UISideBottom:
		out.Y = frame.Bounds.Y + frame.Bounds.Height - float32(size) - float32(frame.PadY)
		out.Height = float32(size)
	case UISideLeft:
		out.X = float32(frame.CursorX)
		out.Width = float32(size)
		frame.CursorX += size + frame.Gap
	case UISideRight:
		out.X = frame.Bounds.X + frame.Bounds.Width - float32(size) - float32(frame.PadX)
		out.Width = float32(size)
	}
	return out
}
func (r *runtime) UIGridCell(grid UIGrid, row, col, rowSpan, colSpan int32) Rectangle {
	if grid.Rows <= 0 || grid.Cols <= 0 {
		return Rectangle{}
	}
	x := grid.Bounds.X + float32(grid.PadX) + float32(col)*(cellW(grid)+float32(grid.GapX))
	y := grid.Bounds.Y + float32(grid.PadY) + float32(row)*(cellH(grid)+float32(grid.GapY))
	w := cellW(grid)*float32(colSpan) + float32(max32(0, colSpan-1)*grid.GapX)
	h := cellH(grid)*float32(rowSpan) + float32(max32(0, rowSpan-1)*grid.GapY)
	return Rectangle{X: x, Y: y, Width: w, Height: h}
}
func (r *runtime) UIPlace(parent Rectangle, x, y, w, h int32) Rectangle {
	return Rectangle{X: parent.X + float32(x), Y: parent.Y + float32(y), Width: float32(w), Height: float32(h)}
}
func (r *runtime) SetCurrentTheme(int32, int32) {}
func (r *runtime) SetThemeDarkMode(int32)       {}
func (r *runtime) SetThemeStyle(ThemeStyle)     {}
func (r *runtime) SetThemeSource(ThemeSource)   {}
func (r *runtime) SetThemeMode(ThemeMode)       {}

func (r *runtime) TextField(props TextFieldProps) {
	r.editText(props.Text, props.CursorPosition, props.Focused, props.CommitPressed, props.FocusID, props.MaxCodepoints)
}

func (r *runtime) editText(buf []byte, cursor *int32, focused *bool, commit *bool, focusID int32, maxCodepoints int32) bool {
	if len(buf) == 0 {
		return false
	}
	if commit != nil {
		*commit = false
	}
	if focused != nil && *focused {
		r.focusID = focusID
	}
	if r.focusID != focusID {
		if focused != nil {
			*focused = false
		}
		return false
	}
	if focused != nil {
		*focused = true
	}
	if cursor == nil {
		return false
	}
	text := string(buf[:zeroIndex(buf)])
	pos := clampCursor(text, int(*cursor))
	changed := false
	for _, key := range r.inputKeys {
		switch key {
		case KeyLeft:
			pos = prevRune(text, pos)
		case KeyRight:
			pos = nextRune(text, pos)
		case KeyHome:
			pos = 0
		case KeyEnd:
			pos = len(text)
		case KeyBackspace:
			if pos > 0 {
				prev := prevRune(text, pos)
				text = text[:prev] + text[pos:]
				pos = prev
				changed = true
			}
		case KeyDelete:
			if pos < len(text) {
				next := nextRune(text, pos)
				text = text[:pos] + text[next:]
				changed = true
			}
		case KeyEnter:
			if commit != nil {
				*commit = true
			}
		}
	}
	if len(r.inputKeys) > 0 {
		r.inputKeys = nil
	}
	limit := len(buf) - 1
	if maxCodepoints > 0 && int(maxCodepoints) < limit {
		limit = int(maxCodepoints)
	}
	for _, ch := range r.inputText {
		add := string(ch)
		if len([]byte(text))+len(add) > limit {
			break
		}
		text = text[:pos] + add + text[pos:]
		pos += len(add)
		changed = true
	}
	if len(r.inputText) > 0 {
		r.inputText = nil
	}
	clear(buf)
	copy(buf, text)
	*cursor = int32(pos)
	return changed
}

func NewVector2(x, y float32) Vector2 { return Vector2{X: x, Y: y} }
func NewRectangle(x, y, w, h float32) Rectangle {
	return Rectangle{X: x, Y: y, Width: w, Height: h}
}

func Key(text string) UIKey {
	h := fnv.New64a()
	_, _ = h.Write([]byte(text))
	return UIKey(h.Sum64())
}

func labelsOf(v any) []string {
	switch l := v.(type) {
	case string:
		if l == "" {
			return nil
		}
		return strings.Split(l, ";")
	case []string:
		return l
	}
	rv := reflect.ValueOf(v)
	if rv.Kind() == reflect.Array && rv.Type().Elem().Kind() == reflect.String {
		out := make([]string, rv.Len())
		for i := range out {
			out[i] = rv.Index(i).String()
		}
		return out
	}
	return nil
}

func number32(v any) float32 {
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

func zeroIndex(buf []byte) int {
	for i, b := range buf {
		if b == 0 {
			return i
		}
	}
	return len(buf)
}

func clampCursor(text string, pos int) int {
	if pos < 0 {
		return 0
	}
	if pos > len(text) {
		return len(text)
	}
	if pos == len(text) {
		return pos
	}
	for pos > 0 && !utf8.RuneStart(text[pos]) {
		pos--
	}
	return pos
}

func prevRune(text string, pos int) int {
	pos = clampCursor(text, pos)
	if pos == 0 {
		return 0
	}
	_, size := utf8.DecodeLastRuneInString(text[:pos])
	return pos - size
}

func nextRune(text string, pos int) int {
	pos = clampCursor(text, pos)
	if pos >= len(text) {
		return len(text)
	}
	_, size := utf8.DecodeRuneInString(text[pos:])
	return pos + size
}

func cellW(grid UIGrid) float32 {
	return (grid.Bounds.Width - float32(grid.PadX*2) - float32(max32(0, grid.Cols-1)*grid.GapX)) / float32(grid.Cols)
}

func cellH(grid UIGrid) float32 {
	return (grid.Bounds.Height - float32(grid.PadY*2) - float32(max32(0, grid.Rows-1)*grid.GapY)) / float32(grid.Rows)
}

func max32(a, b int32) int32 {
	if a > b {
		return a
	}
	return b
}
