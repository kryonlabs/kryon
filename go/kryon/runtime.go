package kryon

import (
	"fmt"
	"hash/fnv"
	"reflect"
	"strings"
	"time"
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

type Font struct {
	Texture Texture2D
	ID      uint32
}

type KeyID uint64
type Side int32
type ButtonStyle int32
type SyntaxMode int32
type ThemeId int32
type ThemeStyle int32
type ThemeSource int32
type ThemeMode int32
type PictureFit int32

const (
	FlagVsyncHint       uint = 0x00000040
	FlagWindowResizable uint = 0x00000004

	KeyNull      int32 = 0
	KeyEscape    int32 = 256
	KeyEnter     int32 = 257
	KeyTab       int32 = 258
	KeyBackspace int32 = 259
	KeyDelete    int32 = 261
	KeyRight     int32 = 262
	KeyLeft      int32 = 263
	KeyDown      int32 = 264
	KeyUp        int32 = 265
	KeyHome      int32 = 268
	KeyEnd       int32 = 269
	KeyF2        int32 = 291
	KeyA         int32 = 65
	KeyC         int32 = 67
	KeyV         int32 = 86
	KeyX         int32 = 88

	MouseButtonLeft  int32 = 0
	MouseButtonRight int32 = 1

	FilterBilinear int32 = 1

	SideTop Side = iota
	SideBottom
	SideLeft
	SideRight

	ButtonStylePrimary ButtonStyle = iota
	ButtonStyleSecondary
	ButtonStyleDanger
	ButtonStyleTab
	ButtonStyleTabSelected

	SyntaxNone SyntaxMode = 0
	SyntaxKry  SyntaxMode = 1
	SyntaxC    SyntaxMode = 2
	SyntaxMake SyntaxMode = 3

	ThemeSky ThemeId = iota
	ThemeOcean
	ThemeForest
	ThemeSunset
	ThemeLavender
	ThemeCherry
	ThemeDawn
	ThemeSage
	ThemeInk
	ThemeMono
	ThemeMint
	ThemeCobalt
	ThemeCount

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

	THEME_STYLE_SYSTEM   = ThemeStyleSystem
	THEME_STYLE_RETRO    = ThemeStyleRetro
	THEME_STYLE_MATERIAL = ThemeStyleMaterial

	THEME_SKY      = ThemeSky
	THEME_OCEAN    = ThemeOcean
	THEME_FOREST   = ThemeForest
	THEME_SUNSET   = ThemeSunset
	THEME_LAVENDER = ThemeLavender
	THEME_CHERRY   = ThemeCherry
	THEME_DAWN     = ThemeDawn
	THEME_SAGE     = ThemeSage
	THEME_INK      = ThemeInk
	THEME_MONO     = ThemeMono
	THEME_MINT     = ThemeMint
	THEME_COBALT   = ThemeCobalt
	THEME_COUNT    = ThemeCount

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
	Style    ButtonStyle
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
	Syntax         SyntaxMode
	Style          TextInputStyle
	ContentVersion int32
}

type ColumnProps struct {
	Bounds  Rectangle
	Gap     int32
	Padding int32
	Key     KeyID
}

type RowProps = ColumnProps

type FrameBox struct {
	Bounds  Rectangle
	PadX    int32
	PadY    int32
	Gap     int32
	CursorX int32
	CursorY int32
}

type Grid struct {
	Bounds Rectangle
	Rows   int32
	Cols   int32
	GapX   int32
	GapY   int32
	PadX   int32
	PadY   int32
}

type ParagraphSpec struct {
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
	Bounds             Rectangle
	ID                 int32
	Columns            []string
	Rows               []UITableRow
	ColumnWidths       []int32
	SelectedRow        *int32
	SelectedColumn     *int32
	ActivatedRow       *int32
	ActivatedColumn    *int32
	RightClickedRow    *int32
	RightClickedColumn *int32
	SortColumn         *int32
	ScrollOffset       *int32
	RowHeight          int32
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

type Canvas struct {
	Bounds  Rectangle
	ScrollX *int32
	ScrollY *int32
	Zoom    *float32
}

type CanvasResult struct {
	Active        bool
	Dragging      bool
	SelectedIndex int32
	World         Vector2
}

type Runtime interface {
	Close()
	WindowShouldClose() bool
	BeginFrame()
	EndFrame()
	ClearBackground(Color)
	Background(Color)
	Text(string, int32, int32, int32, Color)
	TextWithFont(string, int32, int32, int32, Color, uint32)
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
	Column(ColumnProps)
	Row(ColumnProps)
	Stack(ColumnProps)
	End()
	TextField(TextFieldProps)
	Key(text string) KeyID
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
	Paragraph(spec ParagraphSpec, x int32, y *int32)
	IconButton(props IconButtonProps) bool
	Href(props HrefProps) bool
	Slider(id, x, y, w int32, label string, min, max int32, value *int32, rest ...any) bool
	Toggle(id, x, y, w, h int32, value *int32, offLabel, onLabel string) bool
	Modal(title, message, cancelBtn, confirmBtn string) int
	TitleBar(title string, height int32)
	BottomNav(props BottomNavProps)
	TopNav(props TopNavProps)
	Toolbar(props ToolbarProps)
	CanvasGrid(bounds Rectangle, step int32, color Color)
	SelectableText(value string, x, y, fontSize int32, color Color)
	ShowToast(message string)
	ShowToastFor(message string, seconds float64)
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
	BeginCanvas(canvas Canvas) CanvasResult
	EndCanvas(canvas Canvas)
	BeginFrameBox(bounds Rectangle, padX, padY, gap int32) FrameBox
	FramePack(frame *FrameBox, side Side, size int32) Rectangle
	GridCell(grid Grid, row, col, rowSpan, colSpan int32) Rectangle
	Place(parent Rectangle, x, y, w, h int32) Rectangle
	SetCurrentTheme(themeID int32, darkMode int32)
	SetThemeDarkMode(dark int32)
	SetThemeStyle(style ThemeStyle)
	SetThemeSource(source ThemeSource)
	SetThemeMode(mode ThemeMode)
}

type runtime struct {
	config         AppConfig
	closed         bool
	frames         int
	focusID        int32
	clipboard      string
	inputEvents    []inputEvent
	taps           []tapEvent
	clicks         []mouseClickEvent
	mousePos       Vector2
	mouseWheel     float32
	mouseDown      map[int32]bool
	keyDown        map[int32]bool
	chars          []rune
	fieldOrder     []int32
	prevOrder      []int32
	focusRefs      map[int32]*bool
	selection      map[int32]selection
	layout         []layoutFrame
	ops            []FrameOp
	lastTableClick tableClick
	currentThemeID ThemeId
	themeSource    ThemeSource
	themeMode      ThemeMode
	themeStyle     ThemeStyle
}

type themePalette struct {
	background   Color
	surface      Color
	text         Color
	button       Color
	buttonHover  Color
	icon         Color
	link         Color
	selected     Color
	selectedHot  Color
	selectedText Color
	border       Color
	focus        Color
}

type layoutFrame struct {
	bounds     Rectangle
	cursorX    float32
	cursorY    float32
	gap        float32
	padding    float32
	horizontal bool
}

type inputEvent struct {
	key      int32
	text     string
	shift    bool
	shortcut bool
}

type tapEvent struct {
	x, y     float32
	consumed bool
}

type mouseClickEvent struct {
	button   int32
	x, y     float32
	when     time.Time
	consumed bool
}

type tableClick struct {
	id     int32
	row    int32
	column int32
	when   time.Time
}

type selection struct {
	Anchor int
	Cursor int
}

func New(config AppConfig) Runtime {
	if config.Width <= 0 {
		config.Width = 640
	}
	if config.Height <= 0 {
		config.Height = 480
	}
	return &runtime{
		config:         config,
		focusRefs:      map[int32]*bool{},
		selection:      map[int32]selection{},
		mouseDown:      map[int32]bool{},
		keyDown:        map[int32]bool{},
		currentThemeID: ThemeMono,
		themeSource:    ThemeSourceSystem,
		themeMode:      ThemeModeSystem,
		themeStyle:     ThemeStyleSystem,
	}
}

func (r *runtime) QueueText(text string) {
	if text != "" {
		r.inputEvents = append(r.inputEvents, inputEvent{text: text})
		r.chars = append(r.chars, []rune(text)...)
	}
}
func (r *runtime) QueueKey(key int32) {
	r.inputEvents = append(r.inputEvents, inputEvent{key: key})
	r.keyDown[key] = true
}
func (r *runtime) QueueShiftKey(key int32) {
	r.inputEvents = append(r.inputEvents, inputEvent{key: key, shift: true})
	r.keyDown[key] = true
}
func (r *runtime) QueueShortcut(key int32) {
	r.inputEvents = append(r.inputEvents, inputEvent{key: key, shortcut: true})
	r.keyDown[key] = true
}
func (r *runtime) QueueTap(x, y float32) {
	r.QueueMouseButton(MouseButtonLeft, x, y)
}
func (r *runtime) QueueMouseButton(button int32, x, y float32) {
	r.mousePos = Vector2{X: x, Y: y}
	r.mouseDown[button] = true
	r.clicks = append(r.clicks, mouseClickEvent{button: button, x: x, y: y, when: time.Now()})
	if button == MouseButtonLeft {
		r.taps = append(r.taps, tapEvent{x: x, y: y})
	}
}
func (r *runtime) QueueMouseWheel(delta float32) {
	r.mouseWheel += delta
}
func (r *runtime) MousePosition() Vector2 {
	return r.mousePos
}
func (r *runtime) MouseButtonPressed(button int32) bool {
	return r.mouseDown[button]
}
func (r *runtime) MouseWheelMove() float32 {
	return r.mouseWheel
}
func (r *runtime) KeyPressed(key int32) bool {
	return r.keyDown[key]
}
func (r *runtime) CharPressed() int32 {
	if len(r.chars) == 0 {
		return 0
	}
	rn := r.chars[0]
	r.chars = r.chars[1:]
	return rn
}
func (r *runtime) SetClipboardText(text string) { r.clipboard = text }
func (r *runtime) ClipboardText() string        { return r.clipboard }
func (r *runtime) SetSelection(focusID, anchor, cursor int32) {
	r.selection[focusID] = selection{Anchor: int(anchor), Cursor: int(cursor)}
}
func (r *runtime) Selection(focusID int32) (anchor, cursor int32, ok bool) {
	s, ok := r.selection[focusID]
	return int32(s.Anchor), int32(s.Cursor), ok
}

func (r *runtime) Close()                  { r.closed = true }
func (r *runtime) WindowShouldClose() bool { return r.closed || r.frames > 0 }
func (r *runtime) BeginFrame() {
	r.fieldOrder = r.fieldOrder[:0]
	clear(r.focusRefs)
	r.layout = r.layout[:0]
	r.ops = r.ops[:0]
}
func (r *runtime) EndFrame() {
	r.prevOrder = append(r.prevOrder[:0], r.fieldOrder...)
	r.taps = nil
	r.clicks = nil
	r.mouseWheel = 0
	r.mouseDown = map[int32]bool{}
	r.keyDown = map[int32]bool{}
	r.chars = nil
	r.inputEvents = nil
	r.frames++
}
func (r *runtime) SetFocus(id int32) { r.setFocus(id) }
func (r *runtime) Focus() int32      { return r.focusID }
func (r *runtime) setFocus(id int32) {
	if r.focusID == id {
		return
	}
	if ref := r.focusRefs[r.focusID]; ref != nil {
		*ref = false
	}
	r.focusID = id
	if ref := r.focusRefs[id]; ref != nil {
		*ref = true
	}
}
func (r *runtime) FrameOps() []FrameOp {
	return append([]FrameOp(nil), r.ops...)
}
func (r *runtime) ClearBackground(c Color) {
	r.record(FrameOp{Kind: FrameOpBackground, Color: c})
}
func (r *runtime) Background(c Color) {
	r.record(FrameOp{Kind: FrameOpBackground, Color: c})
}
func (r *runtime) Text(text string, x, y, fontSize int32, color Color) {
	r.TextWithFont(text, x, y, fontSize, color, 0)
}
func (r *runtime) TextWithFont(text string, x, y, fontSize int32, color Color, fontID uint32) {
	bounds := Rectangle{X: float32(x), Y: float32(y), Width: float32(fontSize * 8), Height: float32(fontSize)}
	if x == 0 && y == 0 {
		bounds = r.layoutRect(bounds)
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: text, Color: color, FontSize: fontSize, FontID: fontID})
}
func (r *runtime) TextFormat(format string, args ...any) string       { return fmt.Sprintf(format, args...) }
func (r *runtime) ScaleUIPx(px int32) int32                           { return px }
func (r *runtime) GetScreenWidth() int32                              { return int32(r.config.Width) }
func (r *runtime) GetScreenHeight() int32                             { return int32(r.config.Height) }
func (r *runtime) GetThemeBackground() Color                          { return r.theme().background }
func (r *runtime) GetThemeText() Color                                { return r.theme().text }
func (r *runtime) GetThemeIcon() Color                                { return r.theme().icon }
func (r *runtime) NewVector2(x, y any) Vector2                        { return NewVector2(number32(x), number32(y)) }
func (r *runtime) DrawCircleV(Vector2, any, Color)                    {}
func (r *runtime) DrawRing(Vector2, any, any, any, any, int32, Color) {}
func (r *runtime) Rect(x, y, w, h int32, color Color, rest ...Color) {
	op := FrameOp{
		Kind:   FrameOpRect,
		Bounds: Rectangle{X: float32(x), Y: float32(y), Width: float32(w), Height: float32(h)},
		Color:  color,
	}
	if len(rest) > 0 {
		op.SecondaryColor = rest[0]
	}
	r.record(op)
}
func (r *runtime) RectGradientH(x, y, w, h int32, left, right Color) {
	r.record(FrameOp{
		Kind:           FrameOpRect,
		Bounds:         Rectangle{X: float32(x), Y: float32(y), Width: float32(w), Height: float32(h)},
		Color:          left,
		SecondaryColor: right,
	})
}
func (r *runtime) Line(x1, y1, x2, y2 int32, color Color) {
	r.record(FrameOp{
		Kind:   FrameOpLine,
		Bounds: Rectangle{X: float32(x1), Y: float32(y1), Width: float32(x2 - x1), Height: float32(y2 - y1)},
		Color:  color,
	})
}
func (r *runtime) Scroll(int32, int32, int32, int32, int32, *int32) {}
func (r *runtime) EndScroll()                                       {}
func (r *runtime) Button(props ButtonProps) bool {
	theme := r.theme()
	if props.Disabled {
		r.record(FrameOp{Kind: FrameOpButton, Bounds: r.layoutRect(props.Bounds), Text: props.Label, Color: mixColor(theme.surface, theme.button, 0.5), BorderColor: theme.button, TextColor: theme.icon, ID: props.ID, FontSize: props.Font, Disabled: true})
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	pressed := r.consumeTap(props.Bounds)
	fill := theme.button
	if pressed {
		fill = theme.buttonHover
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: props.Bounds, Text: props.Label, Color: fill, BorderColor: theme.buttonHover, TextColor: theme.text, ID: props.ID, FontSize: props.Font, Pressed: pressed})
	return pressed
}
func (r *runtime) TabBar(Rectangle, []string, *int32, *int32) int32 { return -1 }
func (r *runtime) Progress(Rectangle, int32, int32, int32, string)  {}
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
func (r *runtime) Column(props ColumnProps) {
	r.pushLayout(props, false, FrameOpColumn)
}
func (r *runtime) Row(props ColumnProps) {
	r.pushLayout(props, true, FrameOpRow)
}
func (r *runtime) Stack(props ColumnProps) {
	r.pushLayout(props, false, FrameOpStack)
}
func (r *runtime) End() {
	if len(r.layout) > 0 {
		r.layout = r.layout[:len(r.layout)-1]
	}
	r.record(FrameOp{Kind: FrameOpEnd})
}
func (r *runtime) Key(text string) KeyID { return Key(text) }
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
func (r *runtime) GetThemeSurface() Color     { return r.theme().surface }
func (r *runtime) GetThemeButton() Color      { return r.theme().button }
func (r *runtime) GetThemeButtonHover() Color { return r.theme().buttonHover }
func (r *runtime) GetThemeLink() Color        { return r.theme().link }
func (r *runtime) TextInRect(text string, rect Rectangle, fontSize int32, color Color) {
	r.record(FrameOp{Kind: FrameOpText, Bounds: rect, Text: text, Color: color, FontSize: fontSize})
}
func (r *runtime) TextLines(lines any, count int32, x int32, y *int32, font, lineH int32, color Color) {
	_, _, _, _, _ = lines, count, x, font, color
	for i, line := range labelsOf(lines) {
		if int32(i) >= count {
			break
		}
		lineY := int32(0)
		if y != nil {
			lineY = *y + int32(i)*lineH
		}
		r.record(FrameOp{
			Kind:     FrameOpText,
			Bounds:   Rectangle{X: float32(x), Y: float32(lineY), Width: float32(font * 8), Height: float32(font)},
			Text:     line,
			Color:    color,
			FontSize: font,
		})
	}
	if y != nil {
		*y += lineH * count
	}
}
func (r *runtime) Bevel(int32, int32, int32, int32, Color, Color)       {}
func (r *runtime) IconTexture(int32, int32, int32, int32, int32, Color) {}
func (r *runtime) Picture(PictureProps)                                 {}
func (r *runtime) Paragraph(spec ParagraphSpec, x int32, y *int32) {
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
func (r *runtime) TitleBar(string, int32)                            {}
func (r *runtime) BottomNav(BottomNavProps)                          {}
func (r *runtime) TopNav(TopNavProps)                                {}
func (r *runtime) Toolbar(ToolbarProps)                              {}
func (r *runtime) CanvasGrid(Rectangle, int32, Color)                {}
func (r *runtime) SelectableText(string, int32, int32, int32, Color) {}
func (r *runtime) ShowToast(string)                                  {}
func (r *runtime) ShowToastFor(string, float64)                      {}
func (r *runtime) TextArea(props TextAreaProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	changed := r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, nil, props.FocusID, props.MaxCodepoints, false)
	r.recordTextInput(FrameOpTextArea, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, props.Font, false)
	return changed
}
func (r *runtime) Radio(props RadioButtonProps) int32 {
	if props.Checked {
		return props.ID
	}
	return 0
}
func (r *runtime) Spinbox(SpinboxProps) bool          { return false }
func (r *runtime) Combobox(ComboboxProps) bool        { return false }
func (r *runtime) LabelFrame(LabelFrameProps)         {}
func (r *runtime) Notebook(NotebookProps) int32       { return 0 }
func (r *runtime) PanedView(PanedViewProps) int32     { return 0 }
func (r *runtime) Collapsible(CollapsibleProps) int32 { return 0 }
func (r *runtime) ListBox(ListBoxProps) int32         { return 0 }
func (r *runtime) SourceView(SourceViewProps) int32   { return 0 }
func (r *runtime) TableView(props TableViewProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	if len(props.Columns) == 0 {
		return 0
	}

	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	headerH := int32(30)
	body := Rectangle{
		X:      props.Bounds.X,
		Y:      props.Bounds.Y + float32(headerH),
		Width:  props.Bounds.Width,
		Height: props.Bounds.Height - float32(headerH),
	}
	if body.Height < 0 {
		body.Height = 0
	}
	if props.ActivatedRow != nil {
		*props.ActivatedRow = -1
	}
	if props.ActivatedColumn != nil {
		*props.ActivatedColumn = -1
	}
	if props.RightClickedRow != nil {
		*props.RightClickedRow = -1
	}
	if props.RightClickedColumn != nil {
		*props.RightClickedColumn = -1
	}

	changed := int32(0)
	maxScroll := max32(0, int32(len(props.Rows))*rowH-int32(body.Height))
	if props.ScrollOffset != nil {
		*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
	}
	if pointInRect(r.mousePos.X, r.mousePos.Y, body) && props.ScrollOffset != nil && r.mouseWheel != 0 {
		*props.ScrollOffset = clamp32(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, 0, maxScroll)
		changed = 1
	}

	if props.ID != 0 {
		r.registerField(props.ID)
	}
	headerBounds := Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: props.Bounds.Width, Height: float32(headerH)}
	headerClickX, headerClicked := r.consumeMouseButtonPoint(MouseButtonLeft, headerBounds)
	if headerClicked && headerClickX >= props.Bounds.X && headerClickX < props.Bounds.X+props.Bounds.Width &&
		r.mousePos.Y >= props.Bounds.Y && r.mousePos.Y < props.Bounds.Y+float32(headerH) {
		col := tableColumnAtX(props, headerClickX)
		if col >= 0 && props.SelectedRow != nil {
			*props.SelectedRow = -1
			changed = 1
		}
		if col >= 0 && props.SelectedColumn != nil {
			*props.SelectedColumn = col
			changed = 1
		}
		if col >= 0 && props.SortColumn != nil {
			*props.SortColumn = col
			changed = 1
		}
		if props.ID != 0 {
			r.setFocus(props.ID)
		}
	}

	for _, click := range r.consumeMouseButtonEvents(MouseButtonLeft, body) {
		row, col := tableCellAt(props, body, rowH, click.x, click.y)
		if row >= 0 && col >= 0 {
			if props.SelectedRow != nil && *props.SelectedRow != row {
				*props.SelectedRow = row
				changed = 1
			}
			if props.SelectedColumn != nil && *props.SelectedColumn != col {
				*props.SelectedColumn = col
				changed = 1
			}
			if props.ID != 0 {
				r.setFocus(props.ID)
			}
			if r.lastTableClick.id == props.ID && r.lastTableClick.row == row &&
				r.lastTableClick.column == col && click.when.Sub(r.lastTableClick.when) <= 450*time.Millisecond {
				if props.ActivatedRow != nil {
					*props.ActivatedRow = row
				}
				if props.ActivatedColumn != nil {
					*props.ActivatedColumn = col
				}
				changed = 1
			}
			r.lastTableClick = tableClick{id: props.ID, row: row, column: col, when: click.when}
		}
	}

	if clickX, clicked := r.consumeMouseButtonPoint(MouseButtonRight, body); clicked {
		row, col := tableCellAt(props, body, rowH, clickX, r.mousePos.Y)
		if row >= 0 && col >= 0 {
			if props.RightClickedRow != nil {
				*props.RightClickedRow = row
			}
			if props.RightClickedColumn != nil {
				*props.RightClickedColumn = col
			}
			changed = 1
		}
	}

	if props.ID != 0 && r.focusID == props.ID {
		changed |= r.handleTableKeys(props)
	}

	r.record(FrameOp{Kind: FrameOpTable, Bounds: props.Bounds, ID: props.ID})
	r.drawTableOps(props, rowH, headerH)
	return changed
}
func (r *runtime) MessageDialog(MessageDialogProps) int32 { return 0 }
func (r *runtime) ConfirmDialog(ConfirmDialogProps) int32 { return 0 }
func (r *runtime) PromptDialog(PromptDialogProps) int32   { return 0 }
func (r *runtime) BeginCanvas(canvas Canvas) CanvasResult {
	return CanvasResult{Active: true, World: Vector2{X: canvas.Bounds.X, Y: canvas.Bounds.Y}}
}
func (r *runtime) EndCanvas(Canvas) {}
func (r *runtime) BeginFrameBox(bounds Rectangle, padX, padY, gap int32) FrameBox {
	return FrameBox{Bounds: bounds, PadX: padX, PadY: padY, Gap: gap, CursorX: int32(bounds.X) + padX, CursorY: int32(bounds.Y) + padY}
}
func (r *runtime) FramePack(frame *FrameBox, side Side, size int32) Rectangle {
	if frame == nil {
		return Rectangle{}
	}
	out := frame.Bounds
	switch side {
	case SideTop:
		out.Y = float32(frame.CursorY)
		out.Height = float32(size)
		frame.CursorY += size + frame.Gap
	case SideBottom:
		out.Y = frame.Bounds.Y + frame.Bounds.Height - float32(size) - float32(frame.PadY)
		out.Height = float32(size)
	case SideLeft:
		out.X = float32(frame.CursorX)
		out.Width = float32(size)
		frame.CursorX += size + frame.Gap
	case SideRight:
		out.X = frame.Bounds.X + frame.Bounds.Width - float32(size) - float32(frame.PadX)
		out.Width = float32(size)
	}
	return out
}
func (r *runtime) GridCell(grid Grid, row, col, rowSpan, colSpan int32) Rectangle {
	if grid.Rows <= 0 || grid.Cols <= 0 {
		return Rectangle{}
	}
	x := grid.Bounds.X + float32(grid.PadX) + float32(col)*(cellW(grid)+float32(grid.GapX))
	y := grid.Bounds.Y + float32(grid.PadY) + float32(row)*(cellH(grid)+float32(grid.GapY))
	w := cellW(grid)*float32(colSpan) + float32(max32(0, colSpan-1)*grid.GapX)
	h := cellH(grid)*float32(rowSpan) + float32(max32(0, rowSpan-1)*grid.GapY)
	return Rectangle{X: x, Y: y, Width: w, Height: h}
}
func (r *runtime) Place(parent Rectangle, x, y, w, h int32) Rectangle {
	return Rectangle{X: parent.X + float32(x), Y: parent.Y + float32(y), Width: float32(w), Height: float32(h)}
}
func (r *runtime) SetCurrentTheme(themeID int32, darkMode int32) {
	r.currentThemeID = normalizeTheme(themeID)
	if darkMode != 0 {
		r.themeMode = ThemeModeDark
	} else {
		r.themeMode = ThemeModeLight
	}
}
func (r *runtime) SetThemeDarkMode(dark int32) {
	if dark != 0 {
		r.themeMode = ThemeModeDark
	} else {
		r.themeMode = ThemeModeLight
	}
}
func (r *runtime) SetThemeStyle(style ThemeStyle) {
	if style < ThemeStyleSystem || style > ThemeStyleMaterial {
		style = ThemeStyleSystem
	}
	r.themeStyle = style
}
func (r *runtime) SetThemeSource(source ThemeSource) {
	if source != ThemeSourceSystem {
		source = ThemeSourceApp
	}
	r.themeSource = source
}
func (r *runtime) SetThemeMode(mode ThemeMode) {
	if mode < ThemeModeSystem || mode > ThemeModeDark {
		mode = ThemeModeSystem
	}
	r.themeMode = mode
}

func (r *runtime) TextField(props TextFieldProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, props.CommitPressed, props.FocusID, props.MaxCodepoints, props.Secure)
	r.recordTextInput(FrameOpTextField, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, props.Font, props.Secure)
}

func (r *runtime) theme() themePalette {
	dark := r.effectiveDark()
	if r.themeSource == ThemeSourceSystem {
		if palette, ok := currentSystemTheme(dark); ok {
			return palette
		}
	}
	return themeCatalogPalette(r.currentThemeID, dark)
}

func (r *runtime) effectiveDark() bool {
	switch r.themeMode {
	case ThemeModeLight:
		return false
	case ThemeModeDark:
		return true
	}
	return systemPrefersDark()
}

func systemPrefersDark() bool {
	return systemThemePrefersDark()
}

func (r *runtime) record(op FrameOp) {
	r.ops = append(r.ops, op)
}

func (r *runtime) recordTextInput(kind FrameOpKind, bounds Rectangle, buf []byte, cursor *int32, focused *bool, focusID, font int32, secure bool) {
	text := string(buf[:zeroIndex(buf)])
	if secure {
		text = strings.Repeat("*", utf8.RuneCountInString(text))
	}
	theme := r.theme()
	border := theme.border
	if r.focusID == focusID || focused != nil && *focused {
		border = theme.focus
	}
	op := FrameOp{
		Kind:              kind,
		Bounds:            bounds,
		Text:              text,
		Color:             theme.background,
		BorderColor:       border,
		TextColor:         theme.text,
		SelectionColor:    theme.selectedHot,
		SelectedTextColor: theme.selectedText,
		CursorColor:       theme.focus,
		FontSize:          font,
		FocusID:           focusID,
		Focused:           r.focusID == focusID,
		Secure:            secure,
	}
	if cursor != nil {
		op.Cursor = *cursor
	}
	if focused != nil {
		op.Focused = *focused
	}
	if sel, ok := r.selection[focusID]; ok {
		start, end := selectionRange(sel)
		op.SelectionStart = int32(start)
		op.SelectionEnd = int32(end)
	}
	r.record(op)
}

func (r *runtime) pushLayout(props ColumnProps, horizontal bool, kind FrameOpKind) {
	bounds := r.layoutRect(props.Bounds)
	padding := float32(props.Padding)
	r.layout = append(r.layout, layoutFrame{
		bounds:     bounds,
		cursorX:    bounds.X + padding,
		cursorY:    bounds.Y + padding,
		gap:        float32(props.Gap),
		padding:    padding,
		horizontal: horizontal,
	})
	r.record(FrameOp{Kind: kind, Bounds: bounds, ID: int32(props.Key)})
}

func (r *runtime) layoutRect(bounds Rectangle) Rectangle {
	if len(r.layout) == 0 || bounds.X != 0 || bounds.Y != 0 {
		return bounds
	}
	frame := &r.layout[len(r.layout)-1]
	out := bounds
	out.X = frame.cursorX
	out.Y = frame.cursorY
	if out.Width <= 0 {
		out.Width = frame.bounds.Width - frame.padding*2
	}
	if out.Height <= 0 {
		out.Height = frame.bounds.Height - frame.padding*2
	}
	if frame.horizontal {
		frame.cursorX += out.Width + frame.gap
	} else {
		frame.cursorY += out.Height + frame.gap
	}
	return out
}

func (r *runtime) editText(bounds Rectangle, buf []byte, cursor *int32, focused *bool, commit *bool, focusID int32, maxCodepoints int32, secure bool) bool {
	if len(buf) == 0 {
		return false
	}
	r.registerField(focusID)
	if focused != nil {
		r.focusRefs[focusID] = focused
	}
	tapX, tapped := r.consumeTapPoint(bounds)
	if focusID != 0 && tapped {
		r.setFocus(focusID)
	}
	if commit != nil {
		*commit = false
	}
	if focused != nil && *focused {
		r.setFocus(focusID)
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
	sel := r.normalizedSelection(focusID, text, pos)
	if tapped {
		pos = cursorAtTap(text, bounds, tapX)
		sel = selection{Anchor: pos, Cursor: pos}
	}
	changed := false
	for _, event := range r.inputEvents {
		if event.text != "" {
			var inserted bool
			text, pos, inserted = insertText(text, pos, sel, event.text, textLimit(buf, maxCodepoints))
			if inserted {
				changed = true
				sel = selection{Anchor: pos, Cursor: pos}
			}
			continue
		}
		if event.shortcut {
			switch event.key {
			case KeyA:
				sel = selection{Anchor: 0, Cursor: len(text)}
			case KeyC:
				if !secure && sel.Anchor != sel.Cursor {
					start, end := selectionRange(sel)
					r.clipboard = text[start:end]
				}
			case KeyX:
				if !secure && sel.Anchor != sel.Cursor {
					start, end := selectionRange(sel)
					r.clipboard = text[start:end]
					text = text[:start] + text[end:]
					pos = start
					sel = selection{Anchor: pos, Cursor: pos}
					changed = true
				}
			case KeyV:
				var inserted bool
				text, pos, inserted = insertText(text, pos, sel, r.clipboard, textLimit(buf, maxCodepoints))
				if inserted {
					changed = true
					sel = selection{Anchor: pos, Cursor: pos}
				}
			}
			continue
		}
		switch event.key {
		case KeyTab:
			r.setFocus(r.nextFocus(focusID, event.shift))
			sel = selection{Anchor: pos, Cursor: pos}
		case KeyLeft:
			pos = prevRune(text, pos)
			sel = selection{Anchor: pos, Cursor: pos}
		case KeyRight:
			pos = nextRune(text, pos)
			sel = selection{Anchor: pos, Cursor: pos}
		case KeyHome:
			pos = 0
			sel = selection{Anchor: pos, Cursor: pos}
		case KeyEnd:
			pos = len(text)
			sel = selection{Anchor: pos, Cursor: pos}
		case KeyBackspace:
			if sel.Anchor != sel.Cursor {
				var deleted bool
				text, pos, deleted = deleteSelection(text, sel)
				if deleted {
					changed = true
				}
			} else if pos > 0 {
				prev := prevRune(text, pos)
				text = text[:prev] + text[pos:]
				pos = prev
				changed = true
			}
			sel = selection{Anchor: pos, Cursor: pos}
		case KeyDelete:
			if sel.Anchor != sel.Cursor {
				var deleted bool
				text, pos, deleted = deleteSelection(text, sel)
				if deleted {
					changed = true
				}
			} else if pos < len(text) {
				next := nextRune(text, pos)
				text = text[:pos] + text[next:]
				changed = true
			}
			sel = selection{Anchor: pos, Cursor: pos}
		case KeyEnter:
			if commit != nil {
				*commit = true
			}
		}
	}
	if len(r.inputEvents) > 0 {
		r.inputEvents = nil
	}
	clear(buf)
	copy(buf, text)
	*cursor = int32(pos)
	if sel.Anchor == sel.Cursor {
		delete(r.selection, focusID)
	} else {
		r.selection[focusID] = sel
	}
	if focused != nil {
		*focused = r.focusID == focusID
	}
	return changed
}

func (r *runtime) consumeTapPoint(bounds Rectangle) (float32, bool) {
	for i := range r.taps {
		if r.taps[i].consumed {
			continue
		}
		if pointInRect(r.taps[i].x, r.taps[i].y, bounds) {
			r.taps[i].consumed = true
			return r.taps[i].x, true
		}
	}
	return 0, false
}

func (r *runtime) consumeTap(bounds Rectangle) bool {
	for i := range r.taps {
		if r.taps[i].consumed {
			continue
		}
		if pointInRect(r.taps[i].x, r.taps[i].y, bounds) {
			r.taps[i].consumed = true
			return true
		}
	}
	return false
}

func (r *runtime) consumeMouseButtonPoint(button int32, bounds Rectangle) (float32, bool) {
	for i := range r.clicks {
		if r.clicks[i].consumed || r.clicks[i].button != button {
			continue
		}
		if pointInRect(r.clicks[i].x, r.clicks[i].y, bounds) {
			r.clicks[i].consumed = true
			return r.clicks[i].x, true
		}
	}
	return 0, false
}

func (r *runtime) consumeMouseButtonEvents(button int32, bounds Rectangle) []mouseClickEvent {
	var events []mouseClickEvent
	for i := range r.clicks {
		if r.clicks[i].consumed || r.clicks[i].button != button {
			continue
		}
		if pointInRect(r.clicks[i].x, r.clicks[i].y, bounds) {
			r.clicks[i].consumed = true
			events = append(events, r.clicks[i])
		}
	}
	return events
}

func pointInRect(x, y float32, bounds Rectangle) bool {
	return x >= bounds.X && y >= bounds.Y &&
		x < bounds.X+bounds.Width && y < bounds.Y+bounds.Height
}

func cursorAtTap(text string, bounds Rectangle, x float32) int {
	const padding = float32(10)
	const charWidth = float32(8)

	rel := x - bounds.X - padding
	if rel <= 0 {
		return 0
	}
	target := int(rel / charWidth)
	pos := 0
	for i := 0; i < target && pos < len(text); i++ {
		pos = nextRune(text, pos)
	}
	return pos
}

func NewVector2(x, y float32) Vector2 { return Vector2{X: x, Y: y} }
func NewRectangle(x, y, w, h float32) Rectangle {
	return Rectangle{X: x, Y: y, Width: w, Height: h}
}

func Key(text string) KeyID {
	h := fnv.New64a()
	_, _ = h.Write([]byte(text))
	return KeyID(h.Sum64())
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

func (r *runtime) registerField(focusID int32) {
	if focusID == 0 {
		return
	}
	for _, id := range r.fieldOrder {
		if id == focusID {
			return
		}
	}
	r.fieldOrder = append(r.fieldOrder, focusID)
}

func (r *runtime) nextFocus(current int32, reverse bool) int32 {
	order := r.prevOrder
	if len(order) == 0 {
		order = r.fieldOrder
	}
	if len(order) == 0 {
		return current
	}
	index := -1
	for i, id := range order {
		if id == current {
			index = i
			break
		}
	}
	if index < 0 {
		return order[0]
	}
	if reverse {
		return order[(index+len(order)-1)%len(order)]
	}
	return order[(index+1)%len(order)]
}

func (r *runtime) normalizedSelection(focusID int32, text string, pos int) selection {
	s, ok := r.selection[focusID]
	if !ok {
		return selection{Anchor: pos, Cursor: pos}
	}
	s.Anchor = clampCursor(text, s.Anchor)
	s.Cursor = clampCursor(text, s.Cursor)
	return s
}

func selectionRange(sel selection) (int, int) {
	if sel.Anchor < sel.Cursor {
		return sel.Anchor, sel.Cursor
	}
	return sel.Cursor, sel.Anchor
}

func deleteSelection(text string, sel selection) (string, int, bool) {
	start, end := selectionRange(sel)
	if start == end {
		return text, start, false
	}
	return text[:start] + text[end:], start, true
}

func insertText(text string, pos int, sel selection, value string, limit int) (string, int, bool) {
	if value == "" {
		return text, pos, false
	}
	if sel.Anchor != sel.Cursor {
		var deleted bool
		text, pos, deleted = deleteSelection(text, sel)
		_ = deleted
	}
	available := limit - len([]byte(text))
	if available <= 0 {
		return text, pos, false
	}
	value = trimUTF8Bytes(value, available)
	if value == "" {
		return text, pos, false
	}
	text = text[:pos] + value + text[pos:]
	return text, pos + len(value), true
}

func textLimit(buf []byte, maxCodepoints int32) int {
	limit := len(buf) - 1
	if maxCodepoints > 0 && int(maxCodepoints) < limit {
		limit = int(maxCodepoints)
	}
	if limit < 0 {
		return 0
	}
	return limit
}

func trimUTF8Bytes(text string, limit int) string {
	if len(text) <= limit {
		return text
	}
	if limit <= 0 {
		return ""
	}
	for limit > 0 && !utf8.RuneStart(text[limit]) {
		limit--
	}
	return text[:limit]
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

func cellW(grid Grid) float32 {
	return (grid.Bounds.Width - float32(grid.PadX*2) - float32(max32(0, grid.Cols-1)*grid.GapX)) / float32(grid.Cols)
}

func cellH(grid Grid) float32 {
	return (grid.Bounds.Height - float32(grid.PadY*2) - float32(max32(0, grid.Rows-1)*grid.GapY)) / float32(grid.Rows)
}

func TableCellRect(props TableViewProps, row, col int32) Rectangle {
	if len(props.Columns) == 0 || row < 0 || col < 0 || int(row) >= len(props.Rows) || int(col) >= len(props.Columns) {
		return Rectangle{}
	}
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	headerH := int32(30)
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	x := props.Bounds.X
	for c := int32(0); c < col; c++ {
		x += float32(tableColumnWidth(props, c))
	}
	return Rectangle{
		X:      x,
		Y:      props.Bounds.Y + float32(headerH) + float32(row*rowH-scroll),
		Width:  float32(tableColumnWidth(props, col)),
		Height: float32(rowH),
	}
}

func (r *runtime) handleTableKeys(props TableViewProps) int32 {
	if props.SelectedRow == nil {
		return 0
	}
	changed := int32(0)
	row := int32(0)
	if *props.SelectedRow >= 0 {
		row = clamp32(*props.SelectedRow, 0, int32(len(props.Rows)-1))
	}
	col := int32(0)
	if props.SelectedColumn != nil && *props.SelectedColumn >= 0 {
		col = clamp32(*props.SelectedColumn, 0, int32(len(props.Columns)-1))
	}
	for _, event := range r.inputEvents {
		if event.shortcut || event.text != "" {
			continue
		}
		switch event.key {
		case KeyUp:
			row = clamp32(row-1, 0, int32(len(props.Rows)-1))
			changed = 1
		case KeyDown:
			row = clamp32(row+1, 0, int32(len(props.Rows)-1))
			changed = 1
		case KeyLeft:
			col = clamp32(col-1, 0, int32(len(props.Columns)-1))
			changed = 1
		case KeyRight:
			col = clamp32(col+1, 0, int32(len(props.Columns)-1))
			changed = 1
		case KeyTab:
			if event.shift {
				if col > 0 {
					col--
				} else {
					col = int32(len(props.Columns) - 1)
					row = clamp32(row-1, 0, int32(len(props.Rows)-1))
				}
			} else if col < int32(len(props.Columns)-1) {
				col++
			} else {
				col = 0
				row = clamp32(row+1, 0, int32(len(props.Rows)-1))
			}
			changed = 1
		case KeyEnter, KeyF2:
			if props.ActivatedRow != nil {
				*props.ActivatedRow = row
			}
			if props.ActivatedColumn != nil {
				*props.ActivatedColumn = col
			}
			changed = 1
		case KeyEscape:
			if *props.SelectedRow >= 0 || props.SelectedColumn != nil && *props.SelectedColumn >= 0 {
				row = -1
				col = -1
				changed = 1
			}
		}
	}
	if changed != 0 {
		*props.SelectedRow = row
		if props.SelectedColumn != nil {
			*props.SelectedColumn = col
		}
		if row >= 0 {
			r.scrollTableSelectionIntoView(props)
		}
		r.inputEvents = nil
	}
	return changed
}

func (r *runtime) scrollTableSelectionIntoView(props TableViewProps) {
	if props.ScrollOffset == nil || props.SelectedRow == nil {
		return
	}
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	bodyH := int32(props.Bounds.Height) - 30
	if bodyH <= 0 {
		return
	}
	top := *props.SelectedRow * rowH
	bottom := top + rowH
	if top < *props.ScrollOffset {
		*props.ScrollOffset = top
	} else if bottom > *props.ScrollOffset+bodyH {
		*props.ScrollOffset = bottom - bodyH
	}
	maxScroll := max32(0, int32(len(props.Rows))*rowH-bodyH)
	*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
}

func (r *runtime) drawTableOps(props TableViewProps, rowH, headerH int32) {
	theme := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: theme.surface})
	selectedRow := int32(-1)
	selectedCol := int32(-1)
	if props.SelectedRow != nil {
		selectedRow = *props.SelectedRow
	}
	if props.SelectedColumn != nil {
		selectedCol = *props.SelectedColumn
	}
	font := Text12
	if rowH >= 28 {
		font = Text14
	}
	for c := range props.Columns {
		col := int32(c)
		rect := TableCellRect(TableViewProps{Bounds: props.Bounds, Columns: props.Columns, Rows: []UITableRow{{}}, ColumnWidths: props.ColumnWidths, RowHeight: rowH}, 0, col)
		rect.Y = props.Bounds.Y
		rect.Height = float32(headerH)
		fill := theme.button
		selected := false
		if selectedRow < 0 && selectedCol == col {
			fill = theme.selectedHot
			selected = true
		}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: rect, Color: fill, Row: -1, Column: col, Selected: selected})
		r.record(FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect), Text: elideText(props.Columns[c], rect.Width-12, font), Color: theme.text, FontSize: font, Row: -1, Column: col})
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	first := int32(0)
	if rowH > 0 {
		first = scroll / rowH
	}
	visible := int32(0)
	if rowH > 0 {
		visible = int32(props.Bounds.Height-float32(headerH))/rowH + 2
	}
	for i := int32(0); i < visible && first+i < int32(len(props.Rows)); i++ {
		row := first + i
		rowY := props.Bounds.Y + float32(headerH) + float32(row*rowH-scroll)
		rowRect := Rectangle{X: props.Bounds.X, Y: rowY, Width: props.Bounds.Width, Height: float32(rowH)}
		if row%2 == 1 {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: rowRect, Color: mixColor(theme.surface, theme.button, 0.16), Row: row})
		}
		if row == selectedRow && selectedCol < 0 {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: rowRect, Color: theme.selectedHot, Row: row, Column: -1, Selected: true})
		}
		for c := range props.Columns {
			col := int32(c)
			rect := TableCellRect(props, row, col)
			if rect.Y+rect.Height < props.Bounds.Y+float32(headerH) || rect.Y > props.Bounds.Y+props.Bounds.Height {
				continue
			}
			if row == selectedRow && col == selectedCol || selectedRow < 0 && col == selectedCol {
				r.record(FrameOp{Kind: FrameOpRect, Bounds: rect, Color: theme.selectedHot, Row: row, Column: col, Selected: true})
			}
			text := ""
			if int(row) < len(props.Rows) && c < len(props.Rows[row].Cells) {
				text = props.Rows[row].Cells[c]
			}
			r.record(FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect), Text: elideText(text, rect.Width-12, font), Color: theme.text, FontSize: font, Row: row, Column: col})
		}
	}
}

func tableTextBounds(rect Rectangle) Rectangle {
	return Rectangle{X: rect.X + 6, Y: rect.Y + 6, Width: rect.Width - 12, Height: rect.Height - 8}
}

func tableCellAt(props TableViewProps, body Rectangle, rowH int32, x, y float32) (int32, int32) {
	if rowH <= 0 || len(props.Columns) == 0 {
		return -1, -1
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	row := int32((y - body.Y + float32(scroll)) / float32(rowH))
	if row < 0 || int(row) >= len(props.Rows) {
		return -1, -1
	}
	col := tableColumnAtX(props, x)
	if col < 0 {
		return -1, -1
	}
	return row, col
}

func tableColumnAtX(props TableViewProps, x float32) int32 {
	if len(props.Columns) == 0 {
		return -1
	}
	cursor := props.Bounds.X
	for c := range props.Columns {
		w := float32(tableColumnWidth(props, int32(c)))
		if x >= cursor && x < cursor+w {
			return int32(c)
		}
		cursor += w
	}
	return -1
}

func tableColumnWidth(props TableViewProps, col int32) int32 {
	if col < 0 || int(col) >= len(props.Columns) {
		return 0
	}
	if int(col) < len(props.ColumnWidths) && props.ColumnWidths[col] > 0 {
		return props.ColumnWidths[col]
	}
	return int32(props.Bounds.Width) / int32(len(props.Columns))
}

func elideText(text string, maxWidth float32, font int32) string {
	if maxWidth <= 0 {
		return ""
	}
	maxRunes := int(maxWidth / float32(6*glyphScale(font)))
	runes := []rune(text)
	if len(runes) <= maxRunes {
		return text
	}
	if maxRunes <= 1 {
		return ""
	}
	return string(runes[:maxRunes-1]) + "…"
}

func clamp32(v, lo, hi int32) int32 {
	if hi < lo {
		return lo
	}
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}

func mixColor(a, b Color, t float32) Color {
	if t < 0 {
		t = 0
	}
	if t > 1 {
		t = 1
	}
	return Color{
		R: uint8(float32(a.R) + (float32(b.R)-float32(a.R))*t),
		G: uint8(float32(a.G) + (float32(b.G)-float32(a.G))*t),
		B: uint8(float32(a.B) + (float32(b.B)-float32(a.B))*t),
		A: uint8(float32(a.A) + (float32(b.A)-float32(a.A))*t),
	}
}

func max32(a, b int32) int32 {
	if a > b {
		return a
	}
	return b
}
