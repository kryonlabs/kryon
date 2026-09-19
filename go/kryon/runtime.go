package kryon

import (
	"fmt"
	"log"
	"os"
	"time"
)

type AppConfig struct {
	Title         string
	Width, Height int
	FPS           int
	Flags         uint
	MinWidth      int
	MinHeight     int
	// FrameClock supplies frame timestamps for deterministic playback. Nil uses
	// time.Now. It does not replace wall-clock timing of input or host services.
	FrameClock func() time.Time
}

type Font struct {
	Texture Texture2D
	ID      uint32
}

type KeyID uint64

type ThemeId int32

type ThemeSource int32

type ThemeMode int32

const (
	FlagVsyncHint       uint = 0x00000040
	FlagWindowResizable uint = 0x00000004

	KeyNull         int32 = 0
	KeyEscape       int32 = 256
	KeySpace        int32 = 32
	KeyEnter        int32 = 257
	KeyTab          int32 = 258
	KeyBackspace    int32 = 259
	KeyDelete       int32 = 261
	KeyRight        int32 = 262
	KeyLeft         int32 = 263
	KeyDown         int32 = 264
	KeyUp           int32 = 265
	KeyPageUp       int32 = 266
	KeyPageDown     int32 = 267
	KeyHome         int32 = 268
	KeyEnd          int32 = 269
	KeyF2           int32 = 291
	KeyLeftShift    int32 = 340
	KeyLeftControl  int32 = 341
	KeyLeftAlt      int32 = 342
	KeyRightShift   int32 = 344
	KeyRightControl int32 = 345
	KeyRightAlt     int32 = 346
	KeyA            int32 = 65
	KeyC            int32 = 67
	KeyV            int32 = 86
	KeyX            int32 = 88

	MouseButtonLeft   int32 = 0
	MouseButtonRight  int32 = 1
	MouseButtonMiddle int32 = 2

	FilterBilinear int32 = 1
)

const (
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
	ThemePlan9
	ThemeXfce
	ThemeSweet
	ThemeCount
)

const (
	ThemeSourceApp ThemeSource = iota
	ThemeSourceSystem
)

const (
	ThemeModeSystem ThemeMode = iota
	ThemeModeLight
	ThemeModeDark
)

const (
	Text8  int32 = 8
	Text12 int32 = 12
	Text14 int32 = 14
	Text16 int32 = 16
	Text18 int32 = 18
	Text20 int32 = 20
	Text24 int32 = 24
	Text32 int32 = 32
	Text48 int32 = 48

	THEME_SOURCE_APP    = 0
	THEME_SOURCE_SYSTEM = 1
	THEME_MODE_SYSTEM   = 0
	THEME_MODE_LIGHT    = 1
	THEME_MODE_DARK     = 2

	THEME_SKY      = 0
	THEME_OCEAN    = 1
	THEME_FOREST   = 2
	THEME_SUNSET   = 3
	THEME_LAVENDER = 4
	THEME_CHERRY   = 5
	THEME_DAWN     = 6
	THEME_SAGE     = 7
	THEME_INK      = 8
	THEME_MONO     = 9
	THEME_MINT     = 10
	THEME_COBALT   = 11
	THEME_PLAN9    = 12
	THEME_XFCE     = 13
	THEME_SWEET    = 14
	THEME_COUNT    = 15
)

const (
	IconNone = iota
	IconActivity
	IconAmen
	IconBackward
	IconC
	IconCalendar
	IconCheck
	IconEdit
	IconEye
	IconEyeOff
	IconFingerprint
	IconForward
	IconGear
	IconGlobe
	IconHome
	IconJupiter
	IconKryon
	IconLeft
	IconLightoff
	IconLighton
	IconLink
	IconManual
	IconMars
	IconMercury
	IconMoon
	IconMusic
	IconMute
	IconPause
	IconPencil
	IconPet
	IconPlay
	IconPlus
	IconProfile
	IconReturn
	IconRight
	IconRocket
	IconRoutine
	IconSaturn
	IconSave
	IconSound
	IconSound0
	IconSound1
	IconSound2
	IconSound3
	IconStack
	IconStat
	IconSun
	IconText
	IconTimeline
	IconTodos
	IconTrash
	IconVenus
	IconWeekly
	IconWrench
	IconX
	IconLanguageRay
	IconLanguageTcl
	IconLanguageUxn
	IconLanguageWasm
	IconLanguageWasm4
	IconPaymentsBtc
	IconPaymentsMonero
	IconPaymentsStripe
	IconPfpBambus
	IconPfpBird
	IconPfpBowl
	IconPfpBush
	IconPfpButterfly
	IconPfpCactus
	IconPfpCoffee
	IconPfpDragonfly
	IconPfpFireplace
	IconPfpFlower1
	IconPfpFlower2
	IconPfpFox
	IconPfpHeart
	IconPfpIncense
	IconPfpLotus
	IconPfpMountain
	IconPfpMushroom
	IconPfpPalm
	IconPfpPerson1
	IconPfpRainbow
	IconPfpTent
	IconPfpTree1
	IconPfpTree2
	IconPfpTree3
	IconPfpTree4
	IconPlatformsAppimage
	IconPlatformsBrowser
	IconPlatformsChromewebstore
	IconPlatformsDebian
	IconPlatformsDiscord
	IconPlatformsDroid
	IconPlatformsEsp32
	IconPlatformsFdroid
	IconPlatformsFedora
	IconPlatformsFlatpak
	IconPlatformsFreebsd
	IconPlatformsGithub
	IconPlatformsGlenda
	IconPlatformsIos
	IconPlatformsItch
	IconPlatformsMacos
	IconPlatformsMicrocontroller
	IconPlatformsPlaystore
	IconPlatformsSnap
	IconPlatformsSrht
	IconPlatformsTelegram
	IconPlatformsTux
	IconPlatformsWin
	IconProjInbe
	IconProjKryon
	IconProjWao
	IconTilesTile
	IconTilesTile2
	IconTilesTile3
	IconTilesTile4
	IconWorkbookClearFormatting
	IconWorkbookFillColor
	IconWorkbookTextColor
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

type ThemeColors struct {
	Background, Surface, SurfaceRaised, SurfaceSunken, Overlay Color
	Text, MutedText, DisabledText, Icon, MutedIcon             Color
	Border, BorderStrong, Divider, Focus, Selection            Color
	Accent, OnAccent, AccentHover, AccentPressed               Color
	Success, OnSuccess, Warning, OnWarning                     Color
	Danger, OnDanger, Info, OnInfo                             Color
	Link, LinkHover, Shadow                                    Color
}

type ThemeMetrics struct {
	RadiusSmall, RadiusMedium, RadiusLarge, RadiusPill             float32
	BorderWidth, FocusWidth, FocusGap                              float32
	Space1, Space2, Space3, Space4, Space5, Space6                 float32
	ControlHeightSmall, ControlHeightMedium, ControlHeightLarge    float32
	ControlPaddingSmall, ControlPaddingMedium, ControlPaddingLarge float32
	ControlGap                                                     float32
	FontSizeSmall, FontSizeMedium, FontSizeLarge                   float32
	IconSizeSmall, IconSizeMedium, IconSizeLarge                   float32
	ShadowOffsetY, ShadowBlur, DisabledOpacity                     float32
	TransitionFastMS, TransitionNormalMS                           float32
}

type Theme struct {
	Name    string
	Mode    ThemeMode
	Colors  ThemeColors
	Metrics ThemeMetrics
}

type ThemeFamily struct {
	Name  string
	Light Theme
	Dark  Theme
}

type RowProps = ColumnProps

type FlowProps = ColumnProps

// Accelerator describes one keyboard chord and the command returned when it
// is pressed. Modifier fields mirror the native C runtime's clean shortcut
// surface.
type Accelerator struct {
	Key   int32
	Ctrl  int32
	Shift int32
	Alt   int32
	ID    int32
}

type Runtime interface {
	GetAccessibilitySnapshot() []AccessibilityNode
	SetAccessibilitySink(AccessibilitySink)
	QueueAccessibilityAction(int32, uint64, AccessibilityAction) bool
	InstanceValue(typeID any, key uint64, create func() any) any
	SubmitTextComposition(KryTextCompositionPhase, string, int32, int32) int32
	PollTextComposition(*KryTextCompositionEvent) int32
	ClearTextComposition()
	Close()
	WindowShouldClose() bool
	BeginFrame()
	EndFrame()
	AcceleratorPressed(Accelerator) int32
	DispatchAccelerators([]Accelerator, ...int32) int32
	ClearBackground(Color)
	AppBackground()
	Background(Color)
	Text(TextProps)
	MeasureTextWidth(text string, font int32, typeface string) int32
	TextFormat(string, ...any) string
	Scale(int32) int32
	GetScreenWidth() int32
	GetScreenHeight() int32
	GetThemeBackground() Color
	GetThemeText() Color
	GetThemeIcon() Color
	LightenColor(Color, int32) Color
	DarkenColor(Color, int32) Color
	MergeStyle(Style, Style) Style
	NewVector2(any, any) Vector2
	Circle(int32, int32, int32, Color)
	Ring(int32, int32, int32, int32, Color)
	Box(Rectangle, Color, Color)
	Surface(Rectangle, Style)
	RectGradientH(int32, int32, int32, int32, Color, Color)
	Line(int32, int32, int32, int32, Color)
	Scroll(ScrollProps, func(Rectangle))
	Card(CardProps) bool
	Button(ButtonProps) bool
	ReadActivation(bounds Rectangle, id int32, enabled bool) Activation
	Selectable(SelectableProps) bool
	Checkbox(CheckboxProps) bool
	Bullet(Rectangle)
	Separator(SeparatorProps)
	DragDrop(DragDropProps) bool
	ColorPicker(ColorPickerProps) bool
	TabBar(TabBarProps) int32
	Progress(ProgressProps)
	Plot(PlotProps)
	Drag(DragProps) bool
	Slider(SliderProps) bool
	Input(InputProps) bool
	Dropdown(args ...any) bool
	GetSegmentedControlHeight(SegmentedControlProps) int32
	SegmentedControl(SegmentedControlProps) SegmentedControlResult
	StylePicker(StylePickerProps) bool
	Column(ColumnProps)
	Row(ColumnProps)
	Group(ColumnProps)
	Stack(ColumnProps)
	Screen(ColumnProps)
	Grid(GridProps)
	End()
	SetPageTitle(string)
	SetPageDescription(string)
	SetPageCanonicalURL(string)
	SetPageThemeColor(Color)
	GetRoutePath() string
	GetRouteHash() string
	GetRouteVersion() int32
	PushRoute(string)
	ReplaceRoute(string)
	Page(PageProps)
	Section(SectionProps)
	Heading(HeadingProps)
	ParagraphText(ParagraphTextProps)
	Link(LinkProps) bool
	Flow(FlowProps)
	TextField(TextFieldProps)
	Key(text string) KeyID
	Fade(Color, float32) Color
	GetThemeSurface() Color
	GetThemeBorder() Color
	GetThemeButton() Color
	GetThemeButtonHover() Color
	GetThemeLink() Color
	GetThemePrimary() Color
	GetThemeOnPrimary() Color
	GetThemeSurfaceVariant() Color
	GetThemeScheme() DefaultScheme
	SetTheme(Theme)
	SetThemeFamily(ThemeFamily)
	GetThemeFamily() ThemeFamily
	GetTheme() Theme
	Bevel(x, y, w, h int32, light, dark Color)
	Icon(id, x, y, size int32, iconType int32, tint Color)
	Image(props ImageProps)
	Paragraph(spec ParagraphSpec, x int32, y *int32)
	Toggle(ToggleProps) bool
	Modal(ModalProps) int32
	TitleBar(TitleBarProps) int32
	NavigationBar(props NavigationBarProps)
	Toolbar(props ToolbarProps) ToolbarResult
	Menu(props MenuProps) MenuResult
	CanvasGrid(bounds Rectangle, step int32, color Color)
	Toast(props ToastProps)
	TextArea(props TextAreaProps) bool
	Radio(props RadioProps) int32
	Spinbox(props SpinboxProps) bool
	Fieldset(props FieldsetProps)
	PanedView(props PanedViewProps) int32
	Collapsible(props CollapsibleProps) int32
	TreeView(props TreeViewProps) int32
	ListBox(props ListBoxProps) int32
	TableView(props TableViewProps) int32
	SetCurrentTheme(themeID int32, darkMode int32)
	SetThemeSource(source ThemeSource)
	SetThemeMode(mode ThemeMode)
	GetThemeMode() ThemeMode
}

type runtime struct {
	config            AppConfig
	closed            bool
	frames            int
	frameStarted      time.Time
	frameTimeSet      bool
	frameDeltaMS      float32
	elapsedTime       time.Duration
	instances         map[instanceKey]*instanceEntry
	focusID           int32
	autoFocusID       int32
	clipboard         string
	inputEvents       []inputEvent
	textInputHandoff  int32
	deferredTextCount int
	compositionEvents []KryTextCompositionEvent
	preedit           map[int32]KryTextCompositionEvent
	taps              []tapEvent
	clicks            []mouseClickEvent
	mousePos          Vector2
	mouseWheel        float32
	mouseDown         map[int32]bool
	mousePressed      map[int32]bool
	mouseReleased     map[int32]bool
	keyDown           map[int32]bool
	chars             []rune
	fieldOrder        []int32
	prevOrder         []int32
	popupFocus        map[int32]popupFocusOwner
	treeHeaders       []treeHeaderNav
	prevTreeHeaders   []treeHeaderNav
	focusRefs         map[int32]*bool
	selection         map[int32]selection
	layout            []layoutFrame
	ops               []FrameOp
	textLayouts       textLayoutCache
	accessibilitySink AccessibilitySink
	accessibility     accessibilityState
	pageTitle         string
	pageDescription   string
	pageCanonicalURL  string
	pageThemeColor    Color
	routePath         string
	routeHash         string
	routeVersion      int32
	lastTableClick    tableClick
	tableDrag         tableDrag
	tableResize       tableResize
	lastTabClick      tabClick
	lastNumericClick  numericClick
	tabDrag           tabDrag
	tabScroll         map[int32]int32
	tabBarsSeen       map[int32]bool
	openMenus         map[int32]int32
	openSubmenus      map[int32]int32
	menuNavigation    map[int32]*menuNavigation
	contextMenus      map[int32]Vector2
	openDropdowns     map[int32]bool
	dropdownHighlight map[int32]int32
	dropdownOffsets   map[int32]*int32
	dropdownGestures  map[int32]PopupGesture
	dropdownsSeen     map[int32]bool
	popupPanels       map[int32]popupInputPanel
	popupInputScopes  []popupInputToken
	popupInputOrder   uint64
	paintLayers       []paintLayer
	paintLayerScopes  []paintLayerScope
	paintLayerFrame   uint64
	popupScopes       []popupScope
	openPopups        map[int32]*bool
	popupsSeen        map[int32]bool
	tooltipPopupsSeen map[int32]bool
	selectableText    KeyID
	textSelection     selectableTextState
	drag              scalarDrag
	slider            scalarDrag
	numericInputs     map[numericInputKey]*numericInputState
	numericNextToken  int32
	dragDrop          dragDropState
	toastMessage      string
	toastClassName    int32
	toastUntil        time.Time
	currentThemeID    ThemeId
	themeSource       ThemeSource
	themeMode         ThemeMode
	defaultTheme      bool
	activeTheme       *Theme
	activeThemeFamily *ThemeFamily
	disabledStack     []bool
	scrollClips       []Rectangle
	scrollDragOffset  *int32
	scrollDragGrab    float32
	scrollDragOwner   popupInputOwner
	canvases          []canvasScopeState
	disabledCount     int32
}

type themePalette struct {
	background   Color
	surface      Color
	text         Color
	circle       Color
	button       Color
	buttonHover  Color
	icon         Color
	link         Color
	linkHover    Color
	textDisabled Color
	selected     Color
	selectedHot  Color
	selectedText Color
	border       Color
	focus        Color
}

type layoutFrame struct {
	accessibilityOwner int
	bounds             Rectangle
	cursorX            float32
	cursorY            float32
	gap                float32
	padding            float32
	horizontal         bool
	gridCursor         GridCursor
	noLayout           bool
	center             bool
	textFont           int32
	textColor          Color
	textColorSet       bool
	textDisabled       bool
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

type tableDrag struct {
	active   bool
	id       int32
	startRow int32
	startCol int32
	owner    popupInputOwner
}

type tableResize struct {
	active     bool
	id         int32
	column     int32
	startX     float32
	startWidth int32
	owner      popupInputOwner
}

type tabClick struct {
	id     int32
	index  int32
	when   time.Time
	bounds Rectangle
}

type numericClick struct {
	key  numericInputKey
	x, y float32
	when time.Time
}

type tabDrag struct {
	active bool
	id     int32
	from   int32
	bounds Rectangle
}

type scalarDrag struct {
	active bool
	token  int32
	lastX  float32
	owner  popupInputOwner
}

type numericInputKey struct {
	kind, widgetID, component int32
}

type numericInputState struct {
	token   int32
	text    []byte
	cursor  int32
	focused bool
}

const (
	numericEditDragContinuous int32 = 3 + iota
	numericEditDragDiscrete
	numericEditSliderContinuous
	numericEditSliderDiscrete
)

type dragDropState struct {
	active   bool
	sourceID int32
	typeName string
	data     []byte
}

type selection struct {
	Anchor int
	Cursor int
}

const menuMaxDepth int32 = 8

type menuNavigation struct {
	Top  int32
	Path []int
}

func New(config AppConfig) Runtime {
	ensureDefaultTextFont()
	if config.Width <= 0 {
		config.Width = 640
	}
	if config.Height <= 0 {
		config.Height = 480
	}
	r := &runtime{
		config:         config,
		focusRefs:      map[int32]*bool{},
		selection:      map[int32]selection{},
		mouseDown:      map[int32]bool{},
		mousePressed:   map[int32]bool{},
		mouseReleased:  map[int32]bool{},
		keyDown:        map[int32]bool{},
		openMenus:      map[int32]int32{},
		openSubmenus:   map[int32]int32{},
		menuNavigation: map[int32]*menuNavigation{},
		contextMenus:   map[int32]Vector2{},
		openDropdowns:  map[int32]bool{},
		currentThemeID: ThemeMono,
		themeSource:    ThemeSourceSystem,
		themeMode:      ThemeModeSystem,
	}
	r.SetThemeFamily(ThemeFamily{Name: "Default", Light: ThemeDefaultLight(), Dark: ThemeDefaultDark()})
	r.defaultTheme = true
	return r
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
	r.queueModifiedKey(key, false, true)
}

func (r *runtime) queueModifiedKey(key int32, shift, shortcut bool) {
	r.inputEvents = append(r.inputEvents, inputEvent{
		key:      key,
		shift:    shift,
		shortcut: shortcut,
	})
	r.keyDown[key] = true
}

func (r *runtime) QueueTap(x, y float32) {
	r.QueueMouseButton(MouseButtonLeft, x, y)
}

func (r *runtime) QueueMouseButton(button int32, x, y float32) {
	r.QueueMouseButtonDown(button, x, y)
	r.QueueMouseButtonUp(button, x, y)
}

func (r *runtime) QueueMouseButtonDown(button int32, x, y float32) {
	r.mousePos = Vector2{X: x, Y: y}
	r.mouseDown[button] = true
	r.mousePressed[button] = true
	r.clicks = append(r.clicks, mouseClickEvent{button: button, x: x, y: y, when: time.Now()})
	if button == MouseButtonLeft {
		if os.Getenv("KRYON_DEBUG_TAPS") != "" {
			log.Printf("tap queued at (%.0f,%.0f)", x, y)
		}
		r.taps = append(r.taps, tapEvent{x: x, y: y})
	}
}

func (r *runtime) QueueMouseMove(x, y float32) {
	r.mousePos = Vector2{X: x, Y: y}
}

func (r *runtime) QueueMouseButtonUp(button int32, x, y float32) {
	r.mousePos = Vector2{X: x, Y: y}
	r.mouseDown[button] = false
	r.mouseReleased[button] = true
}

func (r *runtime) QueueMouseWheel(delta float32) {
	r.mouseWheel += delta
}

func (r *runtime) MousePosition() Vector2 {
	return r.mousePos
}

func (r *runtime) MouseButtonPressed(button int32) bool {
	return r.mousePressed[button]
}

func (r *runtime) MouseButtonDown(button int32) bool {
	return r.mouseDown[button]
}

func (r *runtime) MouseButtonReleased(button int32) bool {
	return r.mouseReleased[button]
}

func (r *runtime) MouseWheelMove() float32 {
	return r.mouseWheel
}

func (r *runtime) KeyPressed(key int32) bool {
	return r.keyDown[key]
}

func (r *runtime) KeyDown(key int32) bool {
	return r.keyDown[key]
}

func (r *runtime) AcceleratorPressed(accelerator Accelerator) int32 {
	if r.contentDisabled() || r.popupKeyboardCaptures() {
		return 0
	}
	if accelerator.Ctrl != 0 {
		ctrl := r.keyDown[KeyLeftControl] || r.keyDown[KeyRightControl]
		if !ctrl {
			for _, event := range r.inputEvents {
				if event.key == accelerator.Key && event.shortcut {
					ctrl = true
					break
				}
			}
		}
		if !ctrl {
			return 0
		}
	}
	if accelerator.Shift != 0 && !(r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift]) {
		return 0
	}
	if accelerator.Alt != 0 && !(r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt]) {
		return 0
	}
	if !r.keyDown[accelerator.Key] {
		return 0
	}
	return accelerator.ID
}

func (r *runtime) DispatchAccelerators(accelerators []Accelerator, count ...int32) int32 {
	if len(count) > 0 {
		limit := clamp32(count[0], 0, int32(len(accelerators)))
		accelerators = accelerators[:limit]
	}
	for _, accelerator := range accelerators {
		if id := r.AcceleratorPressed(accelerator); id != 0 {
			return id
		}
	}
	return 0
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

func (r *runtime) ClipboardText() string { return r.clipboard }

func (r *runtime) SetSelection(focusID, anchor, cursor int32) {
	r.selection[focusID] = selection{Anchor: int(anchor), Cursor: int(cursor)}
}

func (r *runtime) Selection(focusID int32) (anchor, cursor int32, ok bool) {
	s, ok := r.selection[focusID]
	return int32(s.Anchor), int32(s.Cursor), ok
}

func (r *runtime) Close() {
	r.closed = true
	r.instances = nil
}

func (r *runtime) WindowShouldClose() bool { return r.closed || r.frames > 0 }

func (r *runtime) BeginFrame() {
	r.beginAccessibilityFrame()
	r.applyThemeFamily()
	now := time.Now()
	if r.config.FrameClock != nil {
		now = r.config.FrameClock()
	}
	r.frameDeltaMS = 0
	if r.frameTimeSet {
		delta := now.Sub(r.frameStarted)
		r.frameDeltaMS = float32(delta.Seconds() * 1000)
		r.elapsedTime += delta
	}
	r.frameStarted = now
	r.frameTimeSet = true
	r.expireInstances()
	if len(r.popupInputScopes) != 0 {
		panic("unclosed popup input scope at frame boundary")
	}
	if r.textInputHandoff != 0 && r.textInputHandoff != r.focusID {
		r.inputEvents = r.inputEvents[min(r.deferredTextCount, len(r.inputEvents)):]
	}
	r.textInputHandoff = 0
	r.deferredTextCount = 0
	r.resetPaintLayers()
	r.autoFocusID = 0x40000000
	if r.dropdownsSeen == nil {
		r.dropdownsSeen = make(map[int32]bool)
	}
	clear(r.dropdownsSeen)
	if r.popupsSeen == nil {
		r.popupsSeen = make(map[int32]bool)
	}
	clear(r.popupsSeen)
	if r.tooltipPopupsSeen == nil {
		r.tooltipPopupsSeen = make(map[int32]bool)
	}
	clear(r.tooltipPopupsSeen)
	if r.tabBarsSeen == nil {
		r.tabBarsSeen = make(map[int32]bool)
	}
	clear(r.tabBarsSeen)
	r.scrollClips = r.scrollClips[:0]
	r.canvases = r.canvases[:0]
	r.disabledStack = r.disabledStack[:0]
	r.disabledCount = 0
	r.fieldOrder = r.fieldOrder[:0]
	r.treeHeaders = r.treeHeaders[:0]
	clear(r.focusRefs)
	r.layout = r.layout[:0]
	r.ops = r.ops[:0]
}

func (r *runtime) EndFrame() {
	if len(r.popupScopes) != 0 {
		panic("unclosed popup scope at frame boundary")
	}
	for id, open := range r.openPopups {
		if !r.popupsSeen[id] {
			if open != nil {
				openResult := PopupPolicy_PopupOpenFor(*open, false, true, true)
				*open = openResult.Open
			}
			delete(r.openPopups, id)
			r.closePopupInput(id)
		}
	}
	for id := range r.openDropdowns {
		if !r.dropdownsSeen[id] {
			r.closeDropdown(id)
		}
	}
	for id := range r.tabScroll {
		if !r.tabBarsSeen[id] {
			delete(r.tabScroll, id)
		}
	}
	r.appendPaintLayers(func(id int32) bool {
		return r.openDropdowns[id] ||
			r.openPopups[id] != nil && *r.openPopups[id] || r.tooltipPopupsSeen[id]
	})
	r.prunePopupInput()
	for id, owner := range r.popupFocus {
		if owner.seen != r.paintLayerFrame {
			delete(r.popupFocus, id)
		}
	}
	r.recordToast()
	r.prevOrder = append(r.prevOrder[:0], r.fieldOrder...)
	// A focused widget may be disabled, missing, or behind a popup. Route any
	// unclaimed Tab after all eligible destinations have been declared.
	for _, event := range r.inputEvents {
		if r.textInputHandoff == 0 && event.key == KeyTab && !event.shortcut {
			r.setFocus(r.nextFocus(r.focusID, event.shift))
		}
	}
	r.prevTreeHeaders = append(r.prevTreeHeaders[:0], r.treeHeaders...)
	r.taps = nil
	r.clicks = nil
	r.mouseWheel = 0
	r.mousePressed = map[int32]bool{}
	r.mouseReleased = map[int32]bool{}
	r.keyDown = map[int32]bool{}
	r.chars = nil
	if r.textInputHandoff == 0 || r.textInputHandoff != r.focusID {
		r.inputEvents = nil
	} else {
		r.deferredTextCount = len(r.inputEvents)
	}
	r.ClearTextComposition()
	for id := range r.preedit {
		_, registered := r.popupFocus[id]
		if !registered || id != r.focusID || r.popupFocusCaptures(id) {
			delete(r.preedit, id)
		}
	}
	r.frames++
	r.endAccessibilityFrame()
	if r.accessibilitySink != nil {
		r.accessibilitySink(r.GetAccessibilitySnapshot())
	}
}

func (r *runtime) DisabledScope(disabled bool) {
	r.disabledStack = append(r.disabledStack, disabled)
	if disabled {
		r.disabledCount++
	}
}

func (r *runtime) DisabledEndScope() {
	if len(r.disabledStack) == 0 {
		return
	}
	last := len(r.disabledStack) - 1
	if r.disabledStack[last] {
		r.disabledCount--
	}
	r.disabledStack = r.disabledStack[:last]
}

func (r *runtime) contentDisabled() bool { return r.disabledCount > 0 }

func (r *runtime) SetFocus(id int32) { r.setFocus(id) }

func (r *runtime) Focus() int32 { return r.focusID }

func (r *runtime) setFocus(id int32) {
	if id != 0 {
		r.selectableText = 0
		r.textSelection = selectableTextState{}
	}
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

func (r *runtime) AppBackground() {
	style := r.appStyle()
	r.Background(Primitive_PrimitiveAppBackgroundColor(style.Background, r.GetThemeBackground()))
}

func (r *runtime) appStyle() Style {
	return unpackStyle(ResolveActiveStyle(StyleData{},
		StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindApp()),
		int32(ButtonStateNormal)))
}

func (r *runtime) appAmbientColor() Color {
	return Primitive_PrimitiveAppBackgroundColor(r.appStyle().Background, r.GetThemeBackground())
}

func (r *runtime) Background(c Color) {
	r.record(FrameOp{Kind: FrameOpBackground,
		Bounds: Primitive_PrimitiveBackgroundBounds(r.GetScreenWidth(), r.GetScreenHeight()),
		Color:  c})
}

func packRGBA(c Color) uint32 {
	return uint32(c.R)<<24 | uint32(c.G)<<16 | uint32(c.B)<<8 | uint32(c.A)
}

func unpackRGBA(c uint32) Color {
	return Color{uint8(c >> 24), uint8(c >> 16), uint8(c >> 8), uint8(c)}
}

// ReadActivation samples host input for a shared widget declaration.
func (r *runtime) ReadActivation(bounds Rectangle, id int32, enabled bool) Activation {
	enabled = enabled && !r.contentDisabled()
	activated, focused := r.focusablePress(bounds, id, !enabled)
	hovered := enabled && r.pointerCanReach(bounds)
	return Activation{Activated: activated, Focused: focused, Hovered: hovered,
		Pressed: hovered && r.mouseDown[MouseButtonLeft] || activated}
}

// focusablePress keeps pointer focus, Enter/Space activation, Tab traversal,
// disabled scopes, and popup keyboard ownership consistent across controls.
func (r *runtime) focusablePress(bounds Rectangle, id int32, disabled bool) (pressed, focused bool) {
	enabled := !disabled && !r.contentDisabled()
	accessibility := r.takeAccessibilityActivation(id)
	if enabled {
		r.registerField(id)
		pressed = r.consumeTap(bounds) || accessibility
	}
	if pressed && id > 0 {
		r.setFocus(id)
	}
	focused = enabled && id > 0 && r.focusID == id && !r.popupFocusCaptures(id)
	if !focused {
		return pressed, false
	}
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut && r.focusID == id {
			switch event.key {
			case KeyEnter, KeySpace:
				pressed, handled = true, true
			case KeyTab:
				r.setFocus(r.nextFocus(id, event.shift))
				handled = true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return pressed, r.focusID == id
}

func (r *runtime) record(op FrameOp) {
	op = r.canvasOperation(op)
	if op.Kind == FrameOpText && r.recordAccessibleText(op.Text) {
		op.Role = "presentation"
	}
	if len(r.scrollClips) > 0 {
		scrollClip := r.scrollClips[len(r.scrollClips)-1]
		if op.HasClip {
			op.Clip = intersectRectangles(op.Clip, scrollClip)
		} else {
			op.Clip = scrollClip
		}
		op.HasClip = true
	}
	if r.contentDisabled() {
		if !op.Disabled {
			op.Color = r.Fade(op.Color, 0.45)
			op.BorderColor = r.Fade(op.BorderColor, 0.45)
			op.TextColor = r.Fade(op.TextColor, 0.45)
		}
		op.Disabled = true
	}
	r.ops = append(r.ops, op)
}

func intersectRectangles(a, b Rectangle) Rectangle {
	left := max(a.X, b.X)
	top := max(a.Y, b.Y)
	right := min(a.X+a.Width, b.X+b.Width)
	bottom := min(a.Y+a.Height, b.Y+b.Height)
	return Rectangle{X: left, Y: top, Width: max(float32(0), right-left), Height: max(float32(0), bottom-top)}
}

func (r *runtime) pointerCanReach(bounds Rectangle) bool {
	return !r.contentDisabled() &&
		!r.popupCaptures(r.mousePos.X, r.mousePos.Y) &&
		pointInRect(r.mousePos.X, r.mousePos.Y, r.scrollClip(bounds))
}

func (r *runtime) hasTap(bounds Rectangle) bool {
	bounds = r.scrollClip(bounds)
	if r.contentDisabled() {
		return false
	}
	for i := range r.taps {
		if !r.taps[i].consumed && !r.popupCaptures(r.taps[i].x, r.taps[i].y) &&
			pointInRect(r.taps[i].x, r.taps[i].y, bounds) {
			return true
		}
	}
	return false
}

func (r *runtime) consumeTapPoint(bounds Rectangle) (float32, bool) {
	point, found := r.consumeTapPosition(bounds)
	return point.X, found
}

func (r *runtime) consumeTapPosition(bounds Rectangle) (Vector2, bool) {
	bounds = r.scrollClip(bounds)
	if r.contentDisabled() {
		return Vector2{}, false
	}
	for i := range r.taps {
		if r.taps[i].consumed || r.popupCaptures(r.taps[i].x, r.taps[i].y) {
			continue
		}
		if pointInRect(r.taps[i].x, r.taps[i].y, bounds) {
			r.taps[i].consumed = true
			return Vector2{X: r.taps[i].x, Y: r.taps[i].y}, true
		}
	}
	return Vector2{}, false
}

func (r *runtime) consumeTap(bounds Rectangle) bool {
	bounds = r.scrollClip(bounds)
	if r.contentDisabled() {
		return false
	}
	for i := range r.taps {
		if r.taps[i].consumed || r.popupCaptures(r.taps[i].x, r.taps[i].y) {
			continue
		}
		if pointInRect(r.taps[i].x, r.taps[i].y, bounds) {
			r.taps[i].consumed = true
			if os.Getenv("KRYON_DEBUG_TAPS") != "" {
				log.Printf("tap (%.0f,%.0f) consumed by bounds=%v", r.taps[i].x, r.taps[i].y, bounds)
			}
			return true
		}
	}
	return false
}

func (r *runtime) consumeMouseButtonPoint(button int32, bounds Rectangle) (float32, bool) {
	bounds = r.scrollClip(bounds)
	if r.contentDisabled() {
		return 0, false
	}
	for i := range r.clicks {
		if r.clicks[i].consumed || r.clicks[i].button != button || r.popupCaptures(r.clicks[i].x, r.clicks[i].y) {
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
	bounds = r.scrollClip(bounds)
	if r.contentDisabled() {
		return nil
	}
	var events []mouseClickEvent
	for i := range r.clicks {
		if r.clicks[i].consumed || r.clicks[i].button != button || r.popupCaptures(r.clicks[i].x, r.clicks[i].y) {
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

func NewVector2(x, y float32) Vector2 { return Vector2{X: x, Y: y} }

func NewRectangle(x, y, w, h float32) Rectangle {
	return Rectangle{X: x, Y: y, Width: w, Height: h}
}

func (r *runtime) registerField(focusID int32) {
	if focusID == 0 {
		return
	}
	r.registerPopupFocus(focusID)
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
	eligible := make([]int32, 0, len(order))
	for _, id := range order {
		if !r.popupFocusCaptures(id) {
			eligible = append(eligible, id)
		}
	}
	order = eligible
	index := -1
	for i, id := range order {
		if id == current {
			index = i
			break
		}
	}
	direction := int32(1)
	if reverse {
		direction = -1
	}
	traversal := Focus_FocusTraversalFor(int32(index), int32(len(order)), direction)
	if traversal.Clear {
		return 0
	}
	return order[traversal.Index]
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

func valueOr32(v *int32, fallback int32) int32 {
	if v == nil {
		return fallback
	}
	return *v
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

func min32(a, b int32) int32 {
	if a < b {
		return a
	}
	return b
}

// tapDebug formats one debug line for a widget's tap test (empty when the
// env gate is off).
func (r *runtime) tapDebug(bounds Rectangle, pressed bool, label string) string {
	if os.Getenv("KRYON_DEBUG_TAPS") == "" {
		return ""
	}
	var taps []string
	for i := range r.taps {
		if !r.taps[i].consumed {
			taps = append(taps, fmt.Sprintf("(%.0f,%.0f)", r.taps[i].x, r.taps[i].y))
		}
	}
	return fmt.Sprintf("checkbox %q bounds=%v pressed=%v taps=%v", label, bounds, pressed, taps)
}
