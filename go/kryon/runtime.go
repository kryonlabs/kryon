package kryon

import (
	"fmt"
	"hash/fnv"
	"log"
	"math"
	"os"
	"reflect"
	"strconv"
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

type dragFloatProps struct {
	Bounds     Rectangle
	ID         int32
	ClassName  int32
	Label      string
	Values     []float32
	ValueCount int32
	Speed      float32
	Min        float32
	Max        float32
	Format     string
	Disabled   bool
}

type dragIntProps struct {
	Bounds     Rectangle
	ID         int32
	ClassName  int32
	Label      string
	Values     []int32
	ValueCount int32
	Speed      float32
	Min        int32
	Max        int32
	Format     string
	Disabled   bool
}

type dragFloatRangeProps struct {
	Bounds     Rectangle
	ID         int32
	ClassName  int32
	Label      string
	CurrentMin *float32
	CurrentMax *float32
	Speed      float32
	Min        float32
	Max        float32
	Format     string
	FormatMax  string
	Disabled   bool
}

type dragIntRangeProps struct {
	Bounds     Rectangle
	ID         int32
	ClassName  int32
	Label      string
	CurrentMin *int32
	CurrentMax *int32
	Speed      float32
	Min        int32
	Max        int32
	Format     string
	FormatMax  string
	Disabled   bool
}

type sliderFloatProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float32
	ValueCount int32
	Min        float32
	Max        float32
	Format     string
	Disabled   bool
	ClassName  int32
}

type sliderIntProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []int32
	ValueCount int32
	Min        int32
	Max        int32
	Format     string
	Disabled   bool
	ClassName  int32
}

type sliderAngleProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Value      *float32
	MinDegrees float32
	MaxDegrees float32
	Format     string
	Disabled   bool
	ClassName  int32
}

type inputFloatProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float32
	ValueCount int32
	Step       float32
	StepFast   float32
	Format     string
	Disabled   bool
}

type inputIntProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []int32
	ValueCount int32
	Step       int32
	StepFast   int32
	Format     string
	Disabled   bool
}

type inputDoubleProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float64
	ValueCount int32
	Step       float64
	StepFast   float64
	Format     string
	Disabled   bool
}

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
	bounds       Rectangle
	cursorX      float32
	cursorY      float32
	gap          float32
	padding      float32
	horizontal   bool
	gridCursor   GridCursor
	noLayout     bool
	center       bool
	textFont     int32
	textColor    Color
	textColorSet bool
	textDisabled bool
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
func (r *runtime) ClipboardText() string        { return r.clipboard }
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
		if event.key == KeyTab && !event.shortcut {
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
	r.inputEvents = nil
	r.ClearTextComposition()
	for id := range r.preedit {
		_, registered := r.popupFocus[id]
		if !registered || id != r.focusID || r.popupFocusCaptures(id) {
			delete(r.preedit, id)
		}
	}
	r.frames++
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
func (r *runtime) SetFocus(id int32)     { r.setFocus(id) }
func (r *runtime) Focus() int32          { return r.focusID }
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
func (r *runtime) AppBackground() {
	style := r.appStyle()
	r.Background(Primitive_PrimitiveAppBackgroundColor(style.Background, r.GetThemeBackground()))
}
func (r *runtime) appStyle() Style {
	return unpackStyle(ResolveActiveStyle(StyleData{Fields: uint32(StyleOpacity), Opacity: 1},
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
func (r *runtime) Text(props TextProps) {
	r.textWithFont(props, 0)
}
func (r *runtime) textWithFont(props TextProps, fontID uint32) {
	var inheritedFont int32
	var inheritedColor Color
	var inheritedColorSet bool
	var inheritedDisabled bool
	for i := len(r.layout) - 1; i >= 0; i-- {
		context := r.layout[i]
		if context.textFont > 0 || context.textColorSet {
			inheritedFont = context.textFont
			inheritedColor = context.textColor
			inheritedColorSet = context.textColorSet
			inheritedDisabled = context.textDisabled
			break
		}
	}
	props.Disabled = props.Disabled || (r.contentDisabled() && !inheritedDisabled)
	textState := int32(ButtonStateNormal)
	if props.Disabled {
		textState = int32(ButtonStateDisabled)
	}
	style := unpackStyle(ResolveActiveStyle(packStyle(Style{Opacity: 1}),
		StyleSheet_StyleTextFacts(0, props.ClassName, StyleSheet_StyleKindText(), textState),
		textState))
	colorSet := style.Fields&StyleForeground != 0
	requestedFont := int32(0)
	if style.Fields&StyleFontSize != 0 {
		requestedFont = int32(style.FontSize)
	}
	if style.Fields&StyleTypeface != 0 {
		if selected := registeredTypeface(style.Typeface); selected != 0 {
			fontID = selected
		}
	}
	letterSpacing := int32(0)
	if style.Fields&StyleLetterSpacing != 0 {
		letterSpacing = styleLength(style.LetterSpacing)
	}
	appearance := Text_ResolveTextStyle(requestedFont, inheritedFont, Text16,
		packRGBA(style.Foreground), packRGBA(inheritedColor), 0xffffffff,
		inheritedColorSet, colorSet, props.Disabled, inheritedDisabled, letterSpacing)
	font, spacing := appearance.Font, appearance.LetterSpacing
	color := unpackRGBA(Surface_Opacity(appearance.Color, style.Opacity))
	measure := func(text string) int {
		width := 0
		for _, line := range strings.Split(text, "\n") {
			count := len([]rune(line))
			width = max(width, runtimeTextWidthWithFont(line, font, fontID)+max(count-1, 0)*int(spacing))
		}
		return width
	}
	bounded := props.Bounds.Width > 0
	bounds := props.Bounds
	props.Wrap = TextWrap(Text_TextWrapPolicy(bounds.Width, int32(props.Wrap)))
	var measuredWidth float32
	if !bounded {
		measuredWidth = float32(measure(props.Text))
	}
	bounds.Width = Text_TextExtent(bounds.Width, measuredWidth)
	lines := []string{props.Text}
	if bounded && props.Wrap == TextWrapAuto {
		lines = wrapRuntimeTextMeasured(props.Text, bounds.Width, measure)
	}
	lineHeight := float32(textHeight(font, fontID) + 2)
	contentHeight := max(float32(textHeight(font, fontID)), float32(len(lines))*lineHeight-2)
	bounds.Height = Text_TextExtent(bounds.Height, contentHeight)
	if bounds.X == 0 && bounds.Y == 0 {
		bounds = r.layoutRect(bounds)
	}
	var textKey KeyID
	var textSelected bool
	if props.Selectable && props.Text != "" && !props.Disabled && !inheritedDisabled {
		textKey = Key(fmt.Sprintf("%g:%g:%s", bounds.X, bounds.Y, props.Text))
		if r.consumeTap(bounds) {
			r.selectableText = textKey
		}
		textSelected = r.selectableText == textKey
		if textSelected && !r.contentDisabled() && !r.popupKeyboardCaptures() {
			for _, event := range r.inputEvents {
				if event.shortcut && event.key == KeyC {
					r.clipboard = props.Text
				}
			}
		}
	}
	startY := bounds.Y + Text_TextAlignmentOffset(bounds.Height, contentHeight, int32(props.VerticalAlign))
	for i, line := range lines {
		y := startY + float32(i)*lineHeight
		// A partially visible line is clipped, not discarded. C draws the
		// same text box even when its font metrics exceed the box height.
		if y >= bounds.Y+bounds.Height {
			break
		}
		lineWidth := float32(measure(line))
		x := bounds.X + Text_TextAlignmentOffset(bounds.Width, lineWidth, int32(props.Align))
		op := FrameOp{Kind: FrameOpText,
			Bounds: Rectangle{X: x, Y: y, Width: lineWidth, Height: float32(textHeight(font, fontID))},
			Clip:   bounds, HasClip: true, Text: line, Color: color, FontSize: font,
			FontID: fontID, Disabled: props.Disabled || inheritedDisabled, LetterSpacing: spacing}
		if textSelected {
			op.ID = int32(textKey)
			op.Selected = true
			op.SelectionStart = 0
			op.SelectionEnd = int32(len(line))
		}
		r.record(op)
	}
}
func (r *runtime) TextFormat(format string, args ...any) string { return fmt.Sprintf(format, args...) }
func (r *runtime) Scale(px int32) int32                         { return px }
func (r *runtime) GetScreenWidth() int32                        { return int32(r.config.Width) }
func (r *runtime) GetScreenHeight() int32                       { return int32(r.config.Height) }
func (r *runtime) GetThemeBackground() Color                    { return r.theme().background }
func (r *runtime) GetThemeText() Color                          { return r.theme().text }
func (r *runtime) GetThemeIcon() Color                          { return r.theme().icon }
func (r *runtime) FancyEffectsEnabled() int32                   { return 1 }
func (r *runtime) NewVector2(x, y any) Vector2                  { return NewVector2(number32(x), number32(y)) }
func (r *runtime) Circle(centerX, centerY, radius int32, color Color) {
	r.record(FrameOp{Kind: FrameOpCircle, Bounds: Primitive_PrimitiveCircleBounds(centerX, centerY, radius), Color: color})
}
func (r *runtime) Ring(centerX, centerY, innerRadius, outerRadius int32, color Color) {
	r.record(FrameOp{Kind: FrameOpRing, Bounds: Primitive_PrimitiveRingBounds(centerX, centerY, outerRadius), Radius: float32(innerRadius), Color: color})
}
func (r *runtime) Box(bounds Rectangle, fill Color, border Color) {
	r.record(FrameOp{Kind: FrameOpRect,
		Bounds:      Primitive_PrimitiveRectBounds(int32(bounds.X), int32(bounds.Y), int32(bounds.Width), int32(bounds.Height)),
		Color:       fill,
		BorderColor: border})
}
func (r *runtime) Surface(bounds Rectangle, style Style) {
	style = mergeStyle(unpackStyle(defaultStyleFrame(StyleSheet_StyleKindSurface()).Value), style)
	r.record(FrameOp{Kind: FrameOpSurface, Bounds: bounds,
		Material: style.Material, FocusColor: style.Focus, AmbientColor: r.appAmbientColor(),
		BackgroundEnd: style.BackgroundEnd, HasBackgroundEnd: style.Fields&StyleBackgroundEnd != 0,
		Color: style.Background, BorderColor: style.Border, Radius: style.Radius,
		BorderWidth: style.BorderWidth, Opacity: style.Opacity})
}
func (r *runtime) RectGradientH(x, y, w, h int32, left, right Color) {
	r.record(FrameOp{
		Kind:           FrameOpRect,
		Bounds:         Primitive_PrimitiveRectBounds(x, y, w, h),
		Color:          left,
		SecondaryColor: right,
	})
}
func (r *runtime) Line(x1, y1, x2, y2 int32, color Color) {
	line := Primitive_PrimitiveLineFor(x1, y1, x2, y2)
	r.record(FrameOp{
		Kind:   FrameOpLine,
		Bounds: Rectangle{X: float32(line.X1), Y: float32(line.Y1), Width: float32(line.X2 - line.X1), Height: float32(line.Y2 - line.Y1)},
		Color:  color,
	})
}
func (r *runtime) ScrollScope(bounds Rectangle, contentHeight int32, offset *int32) Rectangle {
	clip := r.scrollClip(bounds)
	trackMetricFrame := styleMetricFrame(0, StyleSheet_StyleKindScroll(), StyleSheet_StyleAny())
	thumbMetricFrame := styleMetricFrame(0, StyleSheet_StyleKindScrollThumb(), StyleSheet_StyleAny())
	trackFrame := resolveButtonFrameForKind(r.theme(), true, r.activeTheme,
		ButtonProps{Size: ControlSizeSmall, Pill: true}, ButtonStateNormal,
		false, 0, 0, 0, StyleSheet_StyleKindScroll())
	metrics := Scroll_ScrollMetricsFor(1, trackMetricFrame, thumbMetricFrame)
	if offset != nil {
		maximum := max32(0, contentHeight-int32(bounds.Height))
		*offset = clamp32(*offset, 0, maximum)
		if r.pointerCanReach(clip) && r.mouseWheel != 0 {
			*offset = clamp32(*offset-int32(r.mouseWheel*float32(metrics.DefaultWheelStep)), 0, maximum)
			r.mouseWheel = 0
		}
		if maximum > 0 && bounds.Width > float32(metrics.ScrollbarWidth) && bounds.Height > 0 {
			trackW := float32(metrics.ScrollbarWidth)
			track := Rectangle{X: bounds.X + bounds.Width - trackW, Y: bounds.Y, Width: trackW, Height: bounds.Height}
			thumbH := min(bounds.Height, max(float32(metrics.ThumbMinHeight), bounds.Height*bounds.Height/float32(contentHeight)))
			travel := bounds.Height - thumbH
			thumbY := bounds.Y
			if travel > 0 {
				thumbY += travel * float32(*offset) / float32(maximum)
			}
			thumbInset := float32(metrics.ThumbInset)
			if thumbInset < 0 {
				thumbInset = 0
			}
			if thumbInset*2 > track.Width {
				thumbInset = track.Width / 2
			}
			thumbRect := Rectangle{X: track.X + thumbInset, Y: thumbY,
				Width: track.Width - thumbInset*2, Height: thumbH}
			if !r.contentDisabled() && r.mousePressed[MouseButtonLeft] && r.consumeTap(track) {
				r.scrollDragOffset = offset
				r.scrollDragGrab = thumbH / 2
				if r.mousePos.Y >= thumbY && r.mousePos.Y < thumbY+thumbH {
					r.scrollDragGrab = r.mousePos.Y - thumbY
				}
			}
			if r.scrollDragOffset == offset {
				if r.contentDisabled() {
					r.scrollDragOffset = nil
				} else if travel > 0 && (r.mouseDown[MouseButtonLeft] || r.mousePressed[MouseButtonLeft]) {
					*offset = clamp32(int32((r.mousePos.Y-bounds.Y-r.scrollDragGrab)*float32(maximum)/travel), 0, maximum)
				}
				if r.mouseReleased[MouseButtonLeft] {
					r.scrollDragOffset = nil
				}
			}
			thumbY = bounds.Y + travel*float32(*offset)/float32(maximum)
			thumbRect.Y = thumbY
			thumbState := ButtonStateNormal
			if r.scrollDragOffset == offset {
				thumbState = ButtonStatePressed
			} else if r.pointerCanReach(thumbRect) {
				thumbState = ButtonStateHover
			}
			thumbFrame := resolveButtonFrameForKind(r.theme(), true, r.activeTheme,
				ButtonProps{Tone: ButtonToneAccent, Emphasis: ButtonEmphasisFilled,
					Size: ControlSizeSmall, Pill: true},
				thumbState, false, 0, 0, 0, StyleSheet_StyleKindScrollThumb())
			r.record(styleFrameRectOp(track, Rectangle{}, trackFrame))
			r.record(styleFrameRectOp(thumbRect, track, thumbFrame))
			bounds.Width -= trackW
			clip = r.scrollClip(bounds)
		}
	}
	r.scrollClips = append(r.scrollClips, clip)
	if offset != nil {
		bounds.Y -= float32(*offset)
	}
	bounds.Height = float32(max32(0, contentHeight))
	return bounds
}
func (r *runtime) Scroll(props ScrollProps, body func(Rectangle)) {
	content := r.ScrollScope(props.Bounds, props.ContentHeight, props.Offset)
	defer r.ScrollEndScope()
	if body != nil {
		body(content)
	}
}
func (r *runtime) ScrollEndScope() {
	if len(r.scrollClips) > 0 {
		r.scrollClips = r.scrollClips[:len(r.scrollClips)-1]
	}
}
func (r *runtime) scrollClip(bounds Rectangle) Rectangle {
	if len(r.scrollClips) == 0 {
		return bounds
	}
	clip := r.scrollClips[len(r.scrollClips)-1]
	x, y := max(bounds.X, clip.X), max(bounds.Y, clip.Y)
	return Rectangle{X: x, Y: y, Width: max(float32(0), min(bounds.X+bounds.Width, clip.X+clip.Width)-x), Height: max(float32(0), min(bounds.Y+bounds.Height, clip.Y+clip.Height)-y)}
}
func (r *runtime) Button(props ButtonProps) bool {
	if props.Arrow {
		props.Label = string(rune(Button_ButtonArrowGlyph(props.Direction)))
		if props.Size == ControlSizeMedium {
			props.Size = ControlSizeSmall
		}
	}
	if props.Info {
		props.Label = "i"
		props.Circle = true
		props.IconOnly = false
		if props.Size == ControlSizeMedium {
			props.Size = ControlSizeSmall
		}
		if props.Bounds.Width <= 0 {
			props.Bounds.Width = 18
		}
		if props.Bounds.Height <= 0 {
			props.Bounds.Height = props.Bounds.Width
		}
	}
	if props.Split {
		activated := int32(0)
		if props.ActivatedID == nil {
			props.ActivatedID = &activated
		}
		*props.ActivatedID = 0
		open := int32(0)
		if props.Open == nil {
			props.Open = &open
		}
		action := r.resolveButtonProps(props)
		menuID := action.ID + 1
		if action.ID == 0 {
			action.ID = r.resolveFocusID(0)
			menuID = r.resolveFocusID(0)
		}
		layout := Button_ButtonResolveSplitLayout(action.Bounds.Width, action.Bounds.Height)
		fullBounds := action.Bounds
		fullBounds.Width = layout.Width
		action.Bounds.Width = layout.ActionWidth
		menu := action
		menu.Bounds = Rectangle{X: fullBounds.X + layout.MenuOffset,
			Y: action.Bounds.Y, Width: layout.MenuWidth, Height: action.Bounds.Height}
		menu.Label = "Open menu"
		menu.ID = menuID
		menu.IconType = IconNone
		menu.IconOnly = true
		menu.Square = true
		menu.Menu = false
		menu.Split = false
		clicked := r.surfaceButtonAt(action, fullBounds, false)
		*props.Open = boolInt(Button_ButtonToggleMenuOpen(*props.Open != 0, r.surfaceButtonAt(menu, fullBounds, true)))
		divider := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
			props.ClassName, StyleSheet_StyleKindButton(), StyleSheet_StyleAny()).Value)
		r.record(FrameOp{Kind: FrameOpLine,
			Bounds: Rectangle{X: menu.Bounds.X, Y: menu.Bounds.Y + layout.DividerInset,
				Height: menu.Bounds.Height - 2*layout.DividerInset}, Color: divider.Border})
		if *props.Open != 0 {
			*props.ActivatedID = r.Menu(MenuProps{
				ID: props.MenuID, ClassName: props.ClassName, Mode: MenuModePopup,
				Bounds:    NewRectangle(fullBounds.X, fullBounds.Y+fullBounds.Height, 0, 0),
				Items:     props.Items,
				ItemCount: props.ItemCount,
			}).ActivatedID
			*props.Open = boolInt(Button_ButtonCloseMenuAfterActivation(*props.Open != 0,
				*props.ActivatedID))
		}
		return clicked
	}
	if props.Menu {
		activated := int32(0)
		if props.ActivatedID == nil {
			props.ActivatedID = &activated
		}
		*props.ActivatedID = 0
		open := int32(0)
		if props.Open == nil {
			props.Open = &open
		}
		props.IconType = IconNone
		props.IconPlacement = IconPlacementTrailing
		props = r.resolveSurfaceButtonProps(props, true)
		props.Bounds = r.layoutRect(props.Bounds)
		*props.Open = boolInt(Button_ButtonToggleMenuOpen(*props.Open != 0,
			r.surfaceButtonAt(props, Rectangle{}, true)))
		if *props.Open == 0 {
			return false
		}
		*props.ActivatedID = r.Menu(MenuProps{
			ID: props.MenuID, ClassName: props.ClassName, Mode: MenuModePopup,
			Bounds:    NewRectangle(props.Bounds.X, props.Bounds.Y+props.Bounds.Height, 0, 0),
			Items:     props.Items,
			ItemCount: props.ItemCount,
		}).ActivatedID
		*props.Open = boolInt(Button_ButtonCloseMenuAfterActivation(*props.Open != 0, *props.ActivatedID))
		return *props.ActivatedID != 0
	}
	props = r.resolveButtonProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	return r.buttonAt(props)
}

func cardButtonProps(props CardProps) ButtonProps {
	return ButtonProps{
		Bounds:    props.Bounds,
		ID:        props.ID,
		ClassName: props.ClassName,
		Disabled:  props.Disabled,
		Selected:  props.Selected,
		State:     props.State,
	}
}

func (r *runtime) Card(props CardProps) bool {
	button := r.resolveSurfaceButtonPropsForKind(cardButtonProps(props), false, StyleSheet_StyleKindCard())
	button.Bounds = r.layoutRect(button.Bounds)
	button.Label = ""
	if !props.Clickable {
		frame, _ := r.surfaceButtonFrameForKind(button, Rectangle{}, false, StyleSheet_StyleKindCard())
		r.record(frame)
		return false
	}
	frame, pressed := r.surfaceButtonFrameForKind(button, Rectangle{}, false, StyleSheet_StyleKindCard())
	r.record(frame)
	return pressed
}

func (r *runtime) CardScope(props CardProps) {
	button := r.resolveSurfaceButtonPropsForKind(cardButtonProps(props), false, StyleSheet_StyleKindCard())
	button.Bounds = r.layoutRect(button.Bounds)
	button.Label = ""
	frame, _ := r.surfaceButtonFrameForKind(button, Rectangle{}, false, StyleSheet_StyleKindCard())
	r.record(frame)
	r.layout = append(r.layout, layoutFrame{
		bounds: frame.Button.ContentBounds,
		center: true, textFont: frame.Button.Font,
		textColor: unpackRGBA(frame.Button.Foreground), textColorSet: true,
		textDisabled: frame.Disabled,
	})
}

func (r *runtime) ButtonScope(props ButtonProps) {
	label := props.Label
	props = r.resolveButtonProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	props.Label = ""
	frame, _ := r.surfaceButtonFrame(props, Rectangle{}, false)
	r.record(frame)
	r.layout = append(r.layout, layoutFrame{
		bounds: frame.Button.ContentBounds,
		center: true, textFont: frame.Button.Font,
		textColor: unpackRGBA(frame.Button.Foreground), textColorSet: true,
		textDisabled: frame.Disabled,
	})
	if label != "" {
		r.Text(TextProps{Text: label, Wrap: TextWrapNone})
	}
}

func boolInt(value bool) int32 {
	if value {
		return 1
	}
	return 0
}

// buttonAt applies the canonical Button interaction and paint contract to an
// already-laid-out rectangle. Composite widgets use it for embedded buttons
// without advancing their parent's layout a second time.
func (r *runtime) buttonAt(props ButtonProps) bool {
	return r.surfaceButtonAt(props, Rectangle{}, false)
}

func (r *runtime) surfaceButtonAt(props ButtonProps, surfaceBounds Rectangle, disclosure bool) bool {
	frame, pressed := r.surfaceButtonFrame(props, surfaceBounds, disclosure)
	if props.Invisible {
		return pressed
	}
	if props.Swatch {
		frame.Color = props.SwatchColor
	}
	r.record(frame)
	if image, ok := buttonImageProps(frame.Button.Props); ok {
		tint := unpackRGBA(frame.Button.Foreground)
		if tint.A == 0 {
			tint = White
		}
		r.record(FrameOp{Kind: FrameOpImage, Bounds: image.Bounds, Text: image.AssetPath, Color: tint, Disabled: frame.Disabled})
	}
	return pressed
}

func buttonImageProps(props ButtonProps) (ImageProps, bool) {
	if props.ImageAssetPath == "" && props.ImageBounds.Width <= 0 && props.ImageBounds.Height <= 0 {
		return ImageProps{}, false
	}
	bounds := props.ImageBounds
	if bounds.Width <= 0 && bounds.Height <= 0 {
		bounds = props.Bounds
	}
	return ImageProps{
		AssetPath: props.ImageAssetPath,
		Bounds:    bounds,
		Source:    props.ImageSource,
		Origin:    props.ImageOrigin,
		Rotation:  props.ImageRotation,
		Fit:       ImageFit(props.ImageFit),
	}, true
}

// Resolve input and animation once. A composed button uses this same frame
// for its surface and inherited content instead of resolving a static style.
func (r *runtime) surfaceButtonFrame(props ButtonProps, surfaceBounds Rectangle, disclosure bool) (FrameOp, bool) {
	return r.surfaceButtonFrameForKind(props, surfaceBounds, disclosure, StyleSheet_StyleKindButton())
}

func (r *runtime) surfaceButtonFrameForKind(props ButtonProps, surfaceBounds Rectangle, disclosure bool, styleKind int32) (FrameOp, bool) {
	return r.surfaceButtonFrameForRoleKind(props, surfaceBounds, disclosure, styleKind, StyleSheet_StyleAny())
}

func (r *runtime) surfaceButtonFrameForRoleKind(props ButtonProps, surfaceBounds Rectangle, disclosure bool, styleKind int32, role int32) (FrameOp, bool) {
	props = r.resolveSurfaceButtonPropsForRoleKind(props, disclosure, styleKind, role)
	props.ID = r.resolveFocusID(props.ID)
	input := r.Button_ReadButtonInput(props.Bounds, props.ID, int32(props.State),
		props.Disabled, props.Loading, props.Selected)
	metrics := r.themeMetrics()
	props.Disabled = input.Flags.Disabled
	props.Loading = input.Flags.Loading
	props.Selected = input.Flags.Selected
	motion := r.Button_AdvanceButtonMotion(uint64(uint32(props.ID)),
		int32(props.State), input, Surface_DefaultMotionEnabled(),
		r.frameDeltaMS, metrics.TransitionNormalMS, metrics.TransitionFastMS)
	appearance := resolveMinimalControlRoleFrame(props, ButtonState(input.Interaction.State),
		props.State == ButtonStateAuto, motion.Hover.Value, motion.Press.Value,
		motion.Focus.Value, styleKind, role)
	resolved := Button_BuildFrame(props, input, appearance, motion,
		surfaceBounds, packRGBA(r.appAmbientColor()), 1,
		int32(appearance.Value.FontSize), Text16)
	frame := FrameOp{Kind: FrameOpButton, Button: resolved,
		Bounds: resolved.Props.Bounds, SurfaceBounds: surfaceBounds, Text: resolved.Props.Label, ID: resolved.Props.ID,
		FontID:     registeredTypeface(resolved.Appearance.Value.Typeface),
		Disclosure: disclosure, ElapsedMS: float64(r.elapsedTime) / float64(time.Millisecond),
		Disabled: resolved.Props.Disabled, Pressed: input.Interaction.Pressed,
		Focused: input.Interaction.Focused, Hovered: input.Interaction.Hovered}
	return frame, input.Activated
}

func (r *runtime) resolveFocusID(id int32) int32 {
	if id != 0 {
		return id
	}
	id = r.autoFocusID
	r.autoFocusID++
	return id
}

func (r *runtime) resolveButtonProps(props ButtonProps) ButtonProps {
	return r.resolveSurfaceButtonProps(props, false)
}

func (r *runtime) resolveSurfaceButtonProps(props ButtonProps, disclosure bool) ButtonProps {
	return r.resolveSurfaceButtonPropsForKind(props, disclosure, StyleSheet_StyleKindButton())
}

func (r *runtime) resolveSurfaceButtonPropsForKind(props ButtonProps, disclosure bool, styleKind int32) ButtonProps {
	return r.resolveSurfaceButtonPropsForRoleKind(props, disclosure, styleKind, StyleSheet_StyleAny())
}

func (r *runtime) resolveSurfaceButtonPropsForRoleKind(props ButtonProps, disclosure bool, styleKind int32, role int32) ButtonProps {
	props.Disabled = props.Disabled || r.contentDisabled()
	style := unpackStyle(resolveMinimalControlRoleState(props, props.State, styleKind, role))
	font := Style_ResolveFont(0, int32(style.FontSize), Text16)
	metrics := defaultThemeMetrics()
	if r.activeTheme != nil {
		metrics = r.activeTheme.Metrics
	}
	height := Style_SizeValue(int32(props.Size), metrics.ControlHeightSmall,
		metrics.ControlHeightMedium, metrics.ControlHeightLarge)
	availableWidth := float32(0)
	if props.FullWidth && props.Bounds.Width <= 0 {
		right := float32(r.GetScreenWidth())
		if len(r.layout) > 0 {
			parent := r.layout[len(r.layout)-1].bounds
			if parent.Width > 0 {
				right = parent.X + parent.Width
			}
		}
		availableWidth = right - props.Bounds.X
	}
	props.Bounds = r.Button_MeasureButton(props, style, height, font, availableWidth, 1, disclosure)
	return props
}

func (r *runtime) MeasureTextWidth(text string, font int32, typeface string) int32 {
	return int32(runtimeTextWidthWithFont(text, font, registeredTypeface(typeface)))
}

func resolveButtonStyle(theme themePalette, dark bool, active *Theme, props ButtonProps, state ButtonState) Style {
	return unpackStyle(resolveButtonFrame(theme, dark, active, props, state, false, 0, 0, 0).Value)
}

func resolveButtonStyleForKind(theme themePalette, dark bool, active *Theme, props ButtonProps, state ButtonState, styleKind int32) Style {
	return unpackStyle(resolveButtonFrameForKind(theme, dark, active, props, state, false, 0, 0, 0, styleKind).Value)
}

func resolveButtonFrame(theme themePalette, dark bool, active *Theme, props ButtonProps, state ButtonState,
	automatic bool, hover, press, focusAmount float32) StyleFrame {
	return resolveButtonFrameForKind(theme, dark, active, props, state, automatic,
		hover, press, focusAmount, StyleSheet_StyleKindButton())
}

func resolveButtonFrameForKind(theme themePalette, dark bool, active *Theme, props ButtonProps, state ButtonState,
	automatic bool, hover, press, focusAmount float32, styleKind int32) StyleFrame {
	if styleKind == 0 {
		styleKind = StyleSheet_StyleKindButton()
	}
	return resolveMinimalControlFrame(props, state, automatic, hover, press,
		focusAmount, styleKind)
}

func minimalControlStyleData() StyleData {
	return StyleData{
		Fields:   uint32(StyleOpacity | StyleFontSize | StyleIconSize | StyleMaterial),
		Opacity:  1,
		FontSize: 16,
		IconSize: 20,
		Material: MaterialFlat,
	}
}

func resolveMinimalControlState(props ButtonProps, state ButtonState, styleKind int32) StyleData {
	return resolveMinimalControlRoleState(props, state, styleKind, StyleSheet_StyleAny())
}

func resolveMinimalControlRoleState(props ButtonProps, state ButtonState, styleKind int32, role int32) StyleData {
	facts := StyleSheet_StyleControlFacts(styleKind, props.ID, props.ClassName,
		int32(props.Tone), int32(props.Emphasis), int32(props.Size), int32(state))
	facts.Role = role
	value := ResolveActiveStyle(minimalControlStyleData(), facts, int32(state))
	return value
}

func resolveMinimalControlFrame(props ButtonProps, state ButtonState, automatic bool,
	hover, press, focusAmount float32, styleKind int32) StyleFrame {
	return resolveMinimalControlRoleFrame(props, state, automatic, hover, press,
		focusAmount, styleKind, StyleSheet_StyleAny())
}

func resolveMinimalControlRoleFrame(props ButtonProps, state ButtonState, automatic bool,
	hover, press, focusAmount float32, styleKind int32, role int32) StyleFrame {
	flags := Style_ResolveFlags(int32(props.State), props.Disabled, props.Loading, props.Selected)
	frame := StyleFrame{Value: resolveMinimalControlRoleState(props, state, styleKind, role)}
	frame.Fill = Surface_FillState(frame.Value.Fields, frame.Value.Background, frame.Value.BackgroundEnd)
	if automatic && !flags.Disabled && !flags.Loading && !flags.Selected &&
		(hover > 0 || press > 0 || focusAmount > 0) {
		normal := frame.Value
		if state != ButtonStateNormal {
			normal = resolveMinimalControlRoleState(props, ButtonStateNormal, styleKind, role)
		}
		hoverStyle, pressStyle, focusStyle := normal, normal, normal
		if hover > 0 {
			hoverStyle = resolveMinimalControlRoleState(props, ButtonStateHover, styleKind, role)
		}
		if press > 0 {
			pressStyle = resolveMinimalControlRoleState(props, ButtonStatePressed, styleKind, role)
		}
		if focusAmount > 0 {
			focusStyle = resolveMinimalControlRoleState(props, ButtonStateFocus, styleKind, role)
		}
		frame = Style_TransitionFrame(frame.Value, normal, hoverStyle,
			pressStyle, focusStyle, hover, press, focusAmount)
	}
	return frame
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
	if enabled {
		r.registerField(id)
		pressed = r.consumeTap(bounds)
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

func (r *runtime) Selectable(props SelectableProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	selected := props.Selected != nil && *props.Selected != 0
	pressed, focused := r.focusablePress(props.Bounds, props.ID, props.Disabled)
	toggle := Selectable_SelectableToggleFor(selected, pressed, props.Selected != nil)
	selected = toggle.Selected
	if toggle.Changed && props.Selected != nil {
		if toggle.Selected {
			*props.Selected = 1
		} else {
			*props.Selected = 0
		}
	}
	state := ButtonStateNormal
	if props.Disabled {
		state = ButtonStateDisabled
	} else if pressed {
		state = ButtonStatePressed
	} else if selected {
		state = ButtonStateSelected
	}
	face := simpleStyleFrameWithClassRole(ButtonToneNeutral, state,
		props.Disabled, selected, props.ClassName,
		StyleSheet_StyleKindSelectable(), StyleSheet_StyleAny())
	style := unpackStyle(face.Value)
	labelInset := style.PaddingX
	if labelInset <= 0 {
		labelInset = 8
	}
	font, fontID := styleTextFace(style, Text14)
	paint := Selectable_SelectablePaintFor(SelectableSpec{
		Bounds:     props.Bounds,
		Selected:   selected,
		Pressed:    pressed,
		Disabled:   props.Disabled,
		Face:       face,
		LabelInset: labelInset,
	})
	if paint.DrawFill {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.Bounds, Color: unpackRGBA(paint.FillColor), Opacity: style.Opacity, Disabled: props.Disabled})
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: paint.LabelX, Y: props.Bounds.Y + 6, Width: props.Bounds.Width - labelInset*2, Height: props.Bounds.Height}, Text: props.Label, Color: unpackRGBA(paint.TextColor), Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: props.ID, Disabled: props.Disabled, Pressed: pressed, Selected: selected, Focused: focused})
	return pressed
}

func (r *runtime) Checkbox(props CheckboxProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	disabled := props.Disabled || (props.Value == nil && props.Flags == nil)
	input := r.ReadActivation(props.Bounds, props.ID, !disabled)
	checked := false
	changed := false
	if props.Flags != nil {
		state := Checkbox_CheckboxFlagApply(uint32(*props.Flags), uint32(props.FlagsValue), input.Activated)
		checked = state.Checked
		changed = state.Changed
		if state.Changed {
			*props.Flags = int32(state.Flags)
		}
	} else if props.Value != nil {
		state := Checkbox_CheckboxValueApply(*props.Value != 0, input.Activated, true)
		checked = state.Checked
		changed = state.Changed
		if state.Changed {
			if state.Checked {
				*props.Value = 1
			} else {
				*props.Value = 0
			}
		}
	}
	state := checkboxButtonState(input.Hovered, input.Pressed, input.Focused, disabled)
	box := checkboxStyleFrame(ButtonToneNeutral, state, disabled, checked, props.ClassName, Checkbox_CheckboxBoxRoleForTone(ButtonToneNeutral))
	active := checkboxStyleFrame(ButtonToneAccent, state, disabled, checked, props.ClassName, Checkbox_CheckboxBoxRoleForTone(ButtonToneAccent))
	label := checkboxStyleFrame(ButtonToneNeutral, state, disabled, checked, props.ClassName, Checkbox_CheckboxLabelRole())
	paint := Checkbox_CheckboxPaintFor(CheckboxSpec{
		Bounds:  props.Bounds,
		Checked: checked,
		Enabled: !disabled,
		Hovered: input.Hovered,
		Pressed: input.Pressed,
		Focused: input.Focused,
		Scale:   1,
		Box:     box,
		Active:  active,
		Label:   label,
	})
	labelStyle := unpackStyle(label.Value)
	labelFont, labelFontID := styleTextFace(labelStyle, Text14)
	fill := unpackRGBA(box.Value.Background)
	if paint.ShowFill {
		fill = unpackRGBA(paint.FillColor)
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.BoxBounds, Color: fill, BorderColor: unpackRGBA(paint.BorderColor), ID: props.ID, Disabled: disabled, Pressed: input.Pressed, Selected: checked, Focused: input.Focused})
	if paint.ShowMark {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: paint.CheckStart.X, Y: paint.CheckStart.Y, Width: paint.CheckMiddle.X - paint.CheckStart.X, Height: paint.CheckMiddle.Y - paint.CheckStart.Y}, Color: unpackRGBA(paint.MarkColor), ID: props.ID})
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: paint.CheckMiddle.X, Y: paint.CheckMiddle.Y, Width: paint.CheckEnd.X - paint.CheckMiddle.X, Height: paint.CheckEnd.Y - paint.CheckMiddle.Y}, Color: unpackRGBA(paint.MarkColor), ID: props.ID})
	}
	labelX := Checkbox_CheckboxLabelXFor(paint.SlotBounds, 1, label)
	labelY := Checkbox_CheckboxLabelYFor(props.Bounds, float32(labelFont))
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: labelX, Y: labelY, Width: props.Bounds.Width - (labelX - props.Bounds.X), Height: props.Bounds.Height}, Text: props.Label, Color: unpackRGBA(paint.LabelColor), Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, Disabled: disabled})
	return changed
}

func checkboxStyleFrame(tone ButtonTone, state ButtonState, disabled, selected bool, className int32, role int32) StyleFrame {
	props := ButtonProps{
		ClassName: className,
		Tone:      tone,
		Emphasis:  ButtonEmphasisOutline,
		Size:      ControlSizeMedium,
		Disabled:  disabled,
		Selected:  selected,
	}
	if tone == ButtonToneAccent {
		props.Emphasis = ButtonEmphasisFilled
	}
	return resolveMinimalControlRoleFrame(props, state, false, 0, 0, 0,
		StyleSheet_StyleKindCheckbox(), role)
}

func checkboxButtonState(hovered, pressed, focused, disabled bool) ButtonState {
	switch {
	case disabled:
		return ButtonStateDisabled
	case pressed:
		return ButtonStatePressed
	case focused:
		return ButtonStateFocus
	case hovered:
		return ButtonStateHover
	default:
		return ButtonStateNormal
	}
}

func (r *runtime) Bullet(bounds Rectangle) {
	bounds = r.layoutRect(bounds)
	frame := simpleStyleFrameWithRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		StyleSheet_StyleKindSeparator(), Separator_SeparatorBulletRole())
	paint := Separator_BulletPaintFor(bounds, frame)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.Bounds, Color: unpackRGBA(paint.Color)})
}

func (r *runtime) Separator(props SeparatorProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	state := ButtonStateNormal
	if props.Disabled {
		state = ButtonStateDisabled
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state,
		props.Disabled, false, props.ClassName, StyleSheet_StyleKindSeparator(), Separator_SeparatorLabelRole())
	if props.Label == "" {
		lineFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state,
			props.Disabled, false, props.ClassName, StyleSheet_StyleKindSeparator(), Separator_SeparatorLineRole())
		paint := Separator_SeparatorLineFor(props.Bounds, props.Vertical, lineFrame)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: paint.Line, Color: unpackRGBA(paint.Color)})
		return
	}
	labelStyle := unpackStyle(frame.Value)
	font, fontID := styleTextFace(labelStyle, Text14)
	if font <= 0 {
		font = Text14
	}
	labelWidth := float32(runtimeTextWidthWithFont(props.Label, font, fontID))
	paint := Separator_SeparatorLabelPaintFor(props.Bounds, labelWidth, props.Label != "", font, 1, frame)
	lineFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state,
		props.Disabled, false, props.ClassName, StyleSheet_StyleKindSeparator(), Separator_SeparatorLineRole())
	paint.LineColor = lineFrame.Value.Background
	if paint.ShowText {
		r.record(FrameOp{Kind: FrameOpText, Bounds: paint.Text, Text: props.Label, Color: unpackRGBA(paint.TextColor), Opacity: labelStyle.Opacity, FontSize: font, FontID: fontID, Disabled: props.Disabled})
	}
	if paint.ShowLine {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: paint.Line, Color: unpackRGBA(paint.LineColor), Disabled: props.Disabled})
	}
}

func (r *runtime) DragDrop(props DragDropProps) bool {
	if props.Role == DragDropRoleTarget {
		if props.AcceptedSize != nil {
			*props.AcceptedSize = 0
		}
		bounds := r.layoutRect(props.Bounds)
		matches := DragDrop_DragDropTargetMatches(r.dragDrop.active, props.Type != "", r.dragDrop.typeName == props.Type)
		disabled := props.Disabled || r.contentDisabled()
		hot := DragDrop_DragDropTargetHot(props.Disabled, r.contentDisabled(), r.pointerCanReach(bounds))
		if matches {
			frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
				if disabled {
					return ButtonStateDisabled
				}
				if hot {
					return ButtonStateHover
				}
				return ButtonStateNormal
			}(), disabled, hot, props.ClassName, StyleSheet_StyleKindDragDropTarget(), StyleSheet_StyleAny())
			op := styleFrameRectOp(bounds, Rectangle{}, frame)
			op.ID = props.ID
			op.Disabled = disabled
			op.Hovered = hot
			op.Selected = hot
			r.record(op)
		}
		if !DragDrop_DragDropTargetAccepts(props.Disabled, r.contentDisabled(), matches, hot, r.mouseReleased[MouseButtonLeft]) {
			return false
		}
		size := int(props.OutputSize)
		if size <= 0 || size > len(props.Output) {
			size = len(props.Output)
		}
		if size > len(r.dragDrop.data) {
			size = len(r.dragDrop.data)
		}
		size = int(DragDrop_DragDropCopySize(int32(len(r.dragDrop.data)), int32(size)))
		copy(props.Output[:size], r.dragDrop.data[:size])
		if props.AcceptedSize != nil {
			*props.AcceptedSize = int32(size)
		}
		r.dragDrop = dragDropState{}
		r.mouseReleased[MouseButtonLeft] = false
		return true
	}
	if DragDrop_DragDropShouldClearSource(r.dragDrop.active, r.dragDrop.sourceID, props.ID, r.mouseDown[MouseButtonLeft], r.mouseReleased[MouseButtonLeft]) {
		r.dragDrop = dragDropState{}
	}
	size := int(props.DataSize)
	if size <= 0 || size > len(props.Data) {
		size = len(props.Data)
	}
	valid := DragDrop_DragDropSourceValid(props.Disabled, r.contentDisabled(), props.Type != "", int32(size), int32(len(props.Data)), len(props.Data) > 0)
	if !valid {
		return false
	}
	bounds := r.layoutRect(props.Bounds)
	if DragDrop_DragDropSourceStarts(valid, r.pointerCanReach(bounds), r.mousePressed[MouseButtonLeft]) {
		r.dragDrop = dragDropState{active: true, sourceID: props.ID, typeName: props.Type, data: append([]byte(nil), props.Data[:size]...)}
	}
	return DragDrop_DragDropSourceReturnsActive(r.dragDrop.active, r.dragDrop.sourceID, props.ID, r.mouseDown[MouseButtonLeft], r.mouseReleased[MouseButtonLeft])
}

func (r *runtime) listBoxMultiSelect(props ListBoxProps) int32 {
	count := int(props.ItemCount)
	if count <= 0 || count > len(props.Items) {
		count = len(props.Items)
	}
	if count > len(props.Selected) {
		count = len(props.Selected)
	}
	if count == 0 {
		if props.SelectedCount != nil {
			*props.SelectedCount = 0
		}
		return -1
	}
	bounds := r.layoutRect(props.Bounds)
	disabled := props.Disabled || r.contentDisabled()
	defaultItemFrame := listBoxMultiItemMetricFrame(props.ClassName, disabled)
	rowHeight := ListBoxMulti_ListBoxMultiRowHeight(props.RowHeight, 1, defaultItemFrame)
	if !disabled {
		r.registerField(props.ID)
	}
	clicked := int32(-1)
	rangeAnchor := int32(-1)
	if !disabled {
		for i := 0; i < count; i++ {
			row := ListBoxMulti_ListBoxMultiRowBounds(bounds, int32(i), rowHeight)
			if r.consumeTap(row) {
				clicked = int32(i)
				if props.ID > 0 {
					r.setFocus(props.ID)
				}
				break
			}
		}
	}
	focused := !disabled && props.ID > 0 && r.focusID == props.ID && !r.popupFocusCaptures(props.ID)
	control := r.keyDown[KeyLeftControl] || r.keyDown[KeyRightControl]
	shift := r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift]
	if focused && clicked < 0 {
		cursor := int32(-1)
		selectedFirst := int32(-1)
		if props.Anchor != nil && *props.Anchor >= 0 && int(*props.Anchor) < count {
			cursor = *props.Anchor
		} else {
			for i := 0; i < count; i++ {
				if props.Selected[i] != 0 {
					selectedFirst = int32(i)
					break
				}
			}
		}
		cursor = ListBoxMulti_ListBoxMultiFocusedRow(cursor, selectedFirst, int32(count))
		input := ListBoxMulti_ListBoxMultiInputFor(
			r.keyDown[KeyHome], r.keyDown[KeyEnd], r.keyDown[KeyUp],
			r.keyDown[KeyDown], r.keyDown[KeySpace], r.keyDown[KeyEnter])
		nav := ListBoxMulti_ListBoxMultiNavigate(int32(count), cursor, control, shift, input)
		clicked = nav.Clicked
		control = nav.Control
		shift = nav.Shift
		rangeAnchor = nav.RangeAnchor
		if nav.AnchorChanged {
			if props.Anchor != nil {
				*props.Anchor = nav.Anchor
			}
		}
	}
	if clicked >= 0 {
		anchor := int32(-1)
		if props.Anchor != nil {
			anchor = *props.Anchor
		}
		for row := 0; row < count; row++ {
			if ListBoxMulti_ListBoxMultiSelectionForRow(int32(row),
				props.Selected[row] != 0, clicked, int32(count), anchor,
				control, shift, rangeAnchor) {
				props.Selected[row] = 1
			} else {
				props.Selected[row] = 0
			}
		}
		if props.Anchor != nil {
			*props.Anchor = ListBoxMulti_ListBoxMultiAnchorAfterClick(anchor,
				clicked, int32(count), control, shift, rangeAnchor)
		}
	}
	selectedCount := int32(0)
	focusRow := int32(-1)
	if focused {
		if props.Anchor != nil && *props.Anchor >= 0 && int(*props.Anchor) < count {
			focusRow = *props.Anchor
		} else {
			for i := 0; i < count; i++ {
				if props.Selected[i] != 0 {
					focusRow = int32(i)
					break
				}
			}
			if focusRow < 0 {
				focusRow = 0
			}
		}
	}
	listFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		if focused {
			return ButtonStateFocus
		}
		return ButtonStateNormal
	}(), disabled, false, props.ClassName, StyleSheet_StyleKindListBoxMulti(), StyleSheet_StyleAny())
	listOp := styleFrameRectOp(bounds, Rectangle{}, listFrame)
	listOp.ID = props.ID
	listOp.Disabled = disabled
	listOp.Focused = focused
	r.record(listOp)
	for i := 0; i < count; i++ {
		selected := props.Selected[i] != 0
		if selected {
			selectedCount++
		}
		row := ListBoxMulti_ListBoxMultiRowBounds(bounds, int32(i), rowHeight)
		rowFocused := focusRow == int32(i)
		hovered := !disabled && pointInRect(r.mousePos.X, r.mousePos.Y, row)
		pressed := int32(i) == clicked
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if disabled {
				return ButtonStateDisabled
			}
			if hovered {
				return ButtonStateHover
			}
			if selected {
				return ButtonStateSelected
			}
			if rowFocused {
				return ButtonStateFocus
			}
			return ButtonStateNormal
		}(), disabled, selected, props.ClassName, StyleSheet_StyleKindListBoxMultiItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		if selected || hovered || disabled || rowFocused {
			op := styleFrameRectOp(row, bounds, itemFrame)
			op.ID = props.ID
			op.Row = int32(i)
			op.Selected = selected
			op.Hovered = hovered
			op.Pressed = pressed
			op.Focused = rowFocused
			op.Disabled = disabled
			if rowFocused {
				op.BorderColor = op.FocusColor
			}
			r.record(op)
		}
		labelX := itemStyle.PaddingX
		if labelX <= 0 {
			labelX = 8
		}
		labelY := itemStyle.PaddingY
		if labelY <= 0 {
			labelY = 4
		}
		font, fontID := styleTextFace(itemStyle, Text14)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + labelX, Y: row.Y + labelY, Width: row.Width - labelX*2, Height: row.Height - labelY*2}, Text: props.Items[i], Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: font, FontID: fontID, ID: props.ID, Row: int32(i), Selected: selected, Disabled: disabled, Pressed: pressed, Focused: rowFocused})
	}
	if props.SelectedCount != nil {
		*props.SelectedCount = selectedCount
	}
	return clicked
}

func (r *runtime) colorEdit(props ColorPickerProps, channels int) bool {
	if len(props.Values) < channels || int(props.ValueCount) > 0 && int(props.ValueCount) < channels {
		return false
	}
	return r.sliderFloat(sliderFloatProps{Bounds: props.Bounds, ID: props.ID, ClassName: props.ClassName, Label: props.Label, Values: props.Values[:channels], ValueCount: int32(channels), Min: 0, Max: 1, Format: "%.3f", Disabled: props.Disabled}, false)
}

func colorFromFloats(values []float32, channels int) Color {
	component := [4]float32{0, 0, 0, 1}
	for i := 0; i < channels && i < len(values); i++ {
		component[i] = values[i]
	}
	return ColorPicker_ColorPickerColorFor(component[0], component[1], component[2], component[3], int32(channels))
}

func (r *runtime) colorPickerFloat(props ColorPickerProps, channels int) bool {
	if len(props.Values) < channels || int(props.ValueCount) > 0 && int(props.ValueCount) < channels {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	disabled := props.Disabled || r.contentDisabled()
	pickerFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, props.ClassName, StyleSheet_StyleKindColorPicker(), StyleSheet_StyleAny())
	layout := ColorPicker_ColorPickerLayoutFor(props.Bounds, int32(channels), 1, pickerFrame)
	changed := false
	for i := 0; i < channels; i++ {
		row := ColorPicker_ColorPickerChannelBounds(props.Bounds, int32(i), int32(channels), 1, pickerFrame)
		changed = r.sliderFloat(sliderFloatProps{Bounds: row, ID: ColorPicker_ColorPickerChannelIdFor(props.ID, int32(i)), ClassName: props.ClassName, Values: props.Values[i : i+1], ValueCount: 1, Min: 0, Max: 1, Format: "%.3f", Disabled: props.Disabled}, false) || changed
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, props.ClassName, StyleSheet_StyleKindColorPickerSwatch(), StyleSheet_StyleAny())
	style := unpackStyle(frame.Value)
	op := styleFrameRectOp(layout.SwatchBounds, props.Bounds, frame)
	op.ID = props.ID
	op.Color = colorFromFloats(props.Values, channels)
	op.Disabled = disabled
	r.record(op)
	if props.Label != "" {
		labelInset := style.PaddingX
		if labelInset <= 0 {
			labelInset = 6
		}
		font, fontID := styleTextFace(style, Text14)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: layout.SwatchBounds.X + labelInset, Y: layout.SwatchBounds.Y + (layout.SwatchBounds.Height-float32(font))/2, Width: layout.SwatchBounds.Width - labelInset*2, Height: float32(font)}, Text: props.Label, Color: style.Foreground, Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: props.ID, Disabled: disabled})
	}
	return changed
}

func (r *runtime) ColorPicker(props ColorPickerProps) bool {
	channels := 3
	if props.ValueCount >= 4 {
		channels = 4
	}
	if props.Picker {
		return r.colorPickerFloat(props, channels)
	}
	return r.colorEdit(props, channels)
}

// TabBar is the canonical tab implementation. It owns sizing, scrolling,
// focus, keyboard navigation, closing and pointer interaction for every tab
// surface; narrower helpers only adapt their props to this function.
func (r *runtime) TabBar(props TabBarProps) int32 {
	resetIndex := func(value *int32) {
		if value != nil {
			*value = -1
		}
	}
	resetIndex(props.ClosedIndex)
	resetIndex(props.DoubleClickedIndex)
	resetIndex(props.ReorderedFromIndex)
	resetIndex(props.ReorderedToIndex)
	resetIndex(props.MiddleClickedIndex)
	if props.SelectedTabBounds != nil {
		*props.SelectedTabBounds = Rectangle{}
	}
	count := int(props.Count)
	if count <= 0 || count > len(props.Tabs) {
		count = len(props.Tabs)
	}
	if count == 0 {
		return -1
	}
	bounds := r.layoutRect(props.Bounds)
	if bounds.Width <= 0 || bounds.Height <= 0 {
		return -1
	}
	disabled := props.Disabled || r.contentDisabled()
	if !disabled && props.ID > 0 {
		r.registerField(props.ID)
	}
	focused := !disabled && props.ID > 0 && r.focusID == props.ID && !r.popupFocusCaptures(props.ID)
	barFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, props.ClassName, StyleSheet_StyleKindTabBar(), StyleSheet_StyleAny())
	var tabGap int32
	selected := props.SelectedIndex
	if selected < 0 || int(selected) >= count {
		selected = 0
	}
	nextEnabled := func(from, direction int32) int32 {
		for step := 1; step <= count; step++ {
			i := (from + direction*int32(step)) % int32(count)
			if i < 0 {
				i += int32(count)
			}
			if !props.Tabs[i].Disabled {
				return i
			}
		}
		return from
	}
	clicked := int32(-1)
	if focused {
		remaining := r.inputEvents[:0]
		for _, event := range r.inputEvents {
			handled := false
			if !event.shortcut {
				switch event.key {
				case KeyLeft, KeyUp:
					clicked, handled = nextEnabled(selected, -1), true
				case KeyRight, KeyDown:
					clicked, handled = nextEnabled(selected, 1), true
				case KeyHome:
					clicked, handled = nextEnabled(-1, 1), true
				case KeyEnd:
					clicked, handled = nextEnabled(0, -1), true
				case KeyDelete, KeyBackspace:
					if props.Tabs[selected].Closeable && props.ClosedIndex != nil {
						*props.ClosedIndex, handled = selected, true
					}
				}
			}
			if handled && clicked >= 0 {
				selected = clicked
			}
			if !handled {
				remaining = append(remaining, event)
			}
		}
		r.inputEvents = remaining
	}
	tabFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		func() ButtonState {
			if disabled {
				return ButtonStateDisabled
			}
			return ButtonStateNormal
		}(), disabled, false, props.ClassName, StyleSheet_StyleKindTab(), StyleSheet_StyleAny())
	metricCloseFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		func() ButtonState {
			if disabled {
				return ButtonStateDisabled
			}
			return ButtonStateNormal
		}(), disabled, false, props.ClassName, StyleSheet_StyleKindTabClose(), StyleSheet_StyleAny())
	tabStyle := unpackStyle(tabFrame.Value)
	font, fontID := styleTextFace(tabStyle, Text12)
	metrics := TabBar_TabBarDefaultMetrics(props.MinTabWidth, props.MaxTabWidth,
		1, barFrame, tabFrame, metricCloseFrame)
	tabGap = metrics.Gap
	widths := make([]float32, count)
	totalWidth := int32(0)
	for i := 0; i < count; i++ {
		labelWidth := int32(runtimeTextWidthWithFont(props.Tabs[i].Label, font, fontID))
		w := TabBar_TabBarTabWidth(labelWidth, props.Tabs[i].Label != "", false, props.Tabs[i].Closeable, metrics)
		widths[i] = float32(w)
		totalWidth += w
	}
	totalWidth = TabBar_TabBarTotalWidth(totalWidth, int32(count), tabGap)
	scrollState := TabBar_TabBarScrollFor(bounds.Width, totalWidth, 0)
	equalTabs := scrollState.EqualTabs
	if equalTabs {
		for i := range widths {
			widths[i] = bounds.Width / float32(count)
		}
		totalWidth = int32(bounds.Width)
	}
	localScroll := int32(0)
	scroll := props.ScrollOffset
	if scroll == nil {
		if props.ID > 0 {
			if r.tabScroll == nil {
				r.tabScroll = make(map[int32]int32)
			}
			if r.tabBarsSeen == nil {
				r.tabBarsSeen = make(map[int32]bool)
			}
			localScroll = r.tabScroll[props.ID]
			r.tabBarsSeen[props.ID] = true
		}
		scroll = &localScroll
	}
	scrollState = TabBar_TabBarScrollFor(bounds.Width, totalWidth, *scroll)
	maxScroll := scrollState.MaxScroll
	*scroll = scrollState.Scroll
	if !disabled && !equalTabs && r.pointerCanReach(bounds) && r.mouseWheel != 0 {
		*scroll = min(maxScroll, max(0, *scroll-int32(r.mouseWheel*42)))
		r.mouseWheel = 0
	}
	if equalTabs {
		*scroll = 0
	}
	if props.FocusSelected && !equalTabs {
		x := bounds.X - float32(*scroll)
		for i := int32(0); i < selected; i++ {
			x += widths[i] + float32(tabGap)
		}
		*scroll = TabBar_TabBarRevealScroll(x, widths[selected], bounds, *scroll, maxScroll)
	}
	barPaint := TabBar_TabBarPaintFor(barFrame, barFrame, barFrame, 1)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: unpackRGBA(barPaint.BarColor), BorderColor: unpackRGBA(barPaint.BarBorderColor), BorderWidth: barFrame.Value.BorderWidth, Radius: barFrame.Value.Radius})
	x := bounds.X - float32(*scroll)
	for i := 0; i < count; i++ {
		item := props.Tabs[i]
		tab := Rectangle{X: x, Y: bounds.Y, Width: widths[i], Height: bounds.Height}
		x += widths[i] + float32(tabGap)
		itemDisabled := disabled || item.Disabled
		isSelected := int32(i) == selected
		closeWidth := float32(0)
		closeBounds := Rectangle{}
		closed := false
		if item.Closeable {
			closeWidth = min(24, tab.Width)
			closeBounds = Rectangle{X: tab.X + tab.Width - closeWidth, Y: tab.Y, Width: closeWidth, Height: tab.Height}
			closed = !itemDisabled && r.consumeTap(intersectRectangles(closeBounds, bounds))
			if closed && props.ClosedIndex != nil {
				*props.ClosedIndex = int32(i)
			}
		}
		body := Rectangle{X: tab.X, Y: tab.Y, Width: max(0, tab.Width-closeWidth), Height: tab.Height}
		pressed := !itemDisabled && !closed && r.consumeTap(intersectRectangles(body, bounds))
		if pressed {
			selected, clicked = int32(i), int32(i)
			if props.ID > 0 {
				r.setFocus(props.ID)
				focused = true
			}
			now := time.Now()
			if props.DoubleClickedIndex != nil && r.lastTabClick.id == props.ID && r.lastTabClick.bounds == bounds &&
				r.lastTabClick.index == int32(i) && now.Sub(r.lastTabClick.when) <= 450*time.Millisecond {
				*props.DoubleClickedIndex = int32(i)
			}
			r.lastTabClick = tabClick{id: props.ID, index: int32(i), when: now, bounds: bounds}
		}
		isSelected = int32(i) == selected
		if isSelected && props.SelectedTabBounds != nil {
			*props.SelectedTabBounds = tab
		}
		if !itemDisabled {
			if _, middle := r.consumeMouseButtonPoint(MouseButtonMiddle, intersectRectangles(tab, bounds)); middle && props.MiddleClickedIndex != nil {
				*props.MiddleClickedIndex = int32(i)
			}
		}
		hovered := !itemDisabled && r.pointerCanReach(intersectRectangles(tab, bounds))
		tabState := ButtonStateNormal
		if itemDisabled {
			tabState = ButtonStateDisabled
		} else if pressed {
			tabState = ButtonStatePressed
		} else if hovered {
			tabState = ButtonStateHover
		} else if isSelected {
			tabState = ButtonStateSelected
		}
		tabFrame := simpleStyleFrameWithClassRole(func() ButtonTone {
			if isSelected {
				return ButtonToneAccent
			}
			return ButtonToneNeutral
		}(), tabState, itemDisabled, isSelected, props.ClassName, StyleSheet_StyleKindTab(), StyleSheet_StyleAny())
		closeState := ButtonStateNormal
		if itemDisabled {
			closeState = ButtonStateDisabled
		}
		if item.Closeable && !itemDisabled && r.pointerCanReach(closeBounds) {
			closeState = ButtonStateHover
		}
		closeFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, closeState, itemDisabled,
			false, props.ClassName, StyleSheet_StyleKindTabClose(), StyleSheet_StyleAny())
		paint := TabBar_TabBarPaintFor(barFrame, tabFrame, closeFrame, 1)
		r.recordButton(FrameOp{Kind: FrameOpButton, Button: ButtonFrame{Props: ButtonProps{ClassName: props.ClassName}}, Opacity: 1,
			BorderWidth: paint.BorderWidth, Radius: paint.Radius,
			AmbientColor: unpackRGBA(paint.BarColor), FocusColor: unpackRGBA(paint.FocusColor), Bounds: tab, Clip: bounds, HasClip: true,
			Text: fitTabLabelWithFont(item.Label, tab.Width-closeWidth-12, font, fontID), Color: unpackRGBA(paint.TabColor),
			BorderColor: unpackRGBA(paint.TabBorderColor), TextColor: unpackRGBA(paint.TextColor), FontSize: font, FontID: fontID, ID: props.ID,
			Disabled: itemDisabled, Pressed: isSelected, Focused: focused && isSelected, Row: int32(i)})
		if item.Closeable {
			r.record(FrameOp{Kind: FrameOpText, Bounds: closeBounds, Clip: bounds, HasClip: true, Text: "×", Color: unpackRGBA(paint.CloseColor),
				FontSize: font, FontID: fontID, Disabled: itemDisabled, Pressed: closed, Row: int32(i)})
		}
	}
	if !disabled && props.ReorderedFromIndex != nil && props.ReorderedToIndex != nil {
		token := props.ID
		if r.mousePressed[MouseButtonLeft] && pointInRect(r.mousePos.X, r.mousePos.Y, bounds) {
			x = bounds.X - float32(*scroll)
			for i, width := range widths {
				if r.mousePos.X >= x && r.mousePos.X < x+width {
					r.tabDrag = tabDrag{active: true, id: token, from: int32(i), bounds: bounds}
					break
				}
				x += width + float32(tabGap)
			}
		}
		if r.tabDrag.active && r.tabDrag.id == token && r.tabDrag.bounds == bounds && r.mouseReleased[MouseButtonLeft] {
			x = bounds.X - float32(*scroll)
			to := r.tabDrag.from
			for i, width := range widths {
				if r.mousePos.X < x+width/2 {
					to = int32(i)
					break
				}
				to = int32(i)
				x += width + float32(tabGap)
			}
			if to != r.tabDrag.from {
				*props.ReorderedFromIndex, *props.ReorderedToIndex = r.tabDrag.from, to
				clicked = -1
			}
			r.tabDrag = tabDrag{}
		}
	}
	if props.ScrollOffset == nil && props.ID > 0 {
		r.tabScroll[props.ID] = *scroll
	}
	return clicked
}

// fitTabLabel truncates with an ellipsis until the label measures within
// maxWidth (rune-safe; measurement falls back to a width estimate when no
// font face is loaded, e.g. headless tests).
func fitTabLabel(label string, maxWidth float32, fontSize int32) string {
	return fitTabLabelWithFont(label, maxWidth, fontSize, 0)
}

func fitTabLabelWithFont(label string, maxWidth float32, fontSize int32, fontID uint32) string {
	if maxWidth <= 8 {
		return ""
	}
	runes := []rune(label)
	for len(runes) > 1 {
		s := string(runes)
		if w, ok := measureFontText(s, fontSize, fontID); ok {
			if w.X <= maxWidth {
				return s
			}
		} else if float32(len(runes))*float32(fontSize)*0.6 <= maxWidth {
			return s
		}
		runes = runes[:len(runes)-1]
		if w, ok := measureFontText(string(runes)+"\u2026", fontSize, fontID); ok && w.X <= maxWidth {
			return string(runes) + "\u2026"
		}
	}
	return string(runes)
}
func (r *runtime) Progress(props ProgressProps) {
	bounds := r.layoutRect(props.Bounds)
	labelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindProgress(), Progress_ProgressLabelRole())
	labelStyle := unpackStyle(labelFrame.Value)
	font, fontID := styleTextFace(labelStyle, Text14)
	labelW := float32(runtimeTextWidthWithFont(props.Label, font, fontID))
	labelLineHeight := float32(font)
	paint := Progress_ProgressPaintFor(bounds, props.Min, props.Max, props.Value,
		labelW, labelLineHeight, 1,
		simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
			false, false, props.ClassName, StyleSheet_StyleKindProgress(), Progress_ProgressTrackRole()),
		simpleStyleFrameWithClassRole(ButtonToneAccent, ButtonStateNormal,
			false, true, props.ClassName, StyleSheet_StyleKindProgress(), Progress_ProgressFillRole()),
		labelFrame)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: unpackRGBA(paint.TrackColor), BorderColor: unpackRGBA(paint.BorderColor), BorderWidth: paint.BorderWidth, Radius: paint.Radius})
	if bounds.Width > 0 && bounds.Height > 0 && paint.Layout.Ratio > 0 {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.Layout.FillBounds, Color: unpackRGBA(paint.FillColor), Radius: paint.Radius, Selected: true})
	}
	if props.Label != "" {
		textColor := unpackRGBA(paint.LabelColor)
		if paint.Layout.LabelOnFill {
			textColor = unpackRGBA(paint.FilledLabelColor)
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: paint.Layout.LabelX, Y: paint.Layout.LabelY, Width: labelW, Height: labelLineHeight}, Text: props.Label, Color: textColor, Opacity: labelStyle.Opacity, FontSize: font, FontID: fontID})
	}
}

func (r *runtime) Plot(props PlotProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	plotFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindPlot(), StyleSheet_StyleAny())
	markFrame := simpleStyleFrameWithClassRole(ButtonToneAccent, ButtonStateSelected,
		false, true, props.ClassName, StyleSheet_StyleKindPlotMark(), StyleSheet_StyleAny())
	plotStyle := unpackStyle(plotFrame.Value)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: plotStyle.Background, BorderColor: plotStyle.Border, BorderWidth: plotStyle.BorderWidth})
	if count == 0 {
		return
	}
	offset := int(props.Offset) % count
	offset = int(Plot_PlotOffset(int32(count), int32(offset)))
	minValue, maxValue := props.ScaleMin, props.ScaleMax
	if minValue >= maxValue {
		minValue, maxValue = props.Values[offset], props.Values[offset]
		for i := 1; i < count; i++ {
			v := props.Values[(offset+i)%count]
			if v < minValue {
				minValue = v
			}
			if v > maxValue {
				maxValue = v
			}
		}
		if minValue == maxValue {
			minValue -= 0.5
			maxValue += 0.5
		}
	}
	plotRange := Plot_PlotRangeFor(props.ScaleMin, props.ScaleMax, minValue, maxValue)
	if props.Mode == PlotBars {
		for i := 0; i < count; i++ {
			bar := Plot_PlotHistogramBar(props.Bounds, int32(i), int32(count), props.Values[(offset+i)%count], plotRange, markFrame)
			r.record(FrameOp{Kind: FrameOpRect, Bounds: bar.Bounds, Color: unpackRGBA(bar.Color), Row: int32(i)})
		}
	} else if count == 1 {
		line := Plot_PlotSingleLine(props.Bounds, props.Values[offset], plotRange, markFrame)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: line.Bounds, Color: unpackRGBA(line.Color)})
	} else {
		for i := 1; i < count; i++ {
			line := Plot_PlotLineSegment(props.Bounds, int32(i), int32(count), props.Values[(offset+i-1)%count], props.Values[(offset+i)%count], plotRange, markFrame)
			r.record(FrameOp{Kind: FrameOpLine, Bounds: line.Bounds, Color: unpackRGBA(line.Color), Row: int32(i - 1)})
		}
	}
	labelWidth := float32(0)
	font, fontID := styleTextFace(plotStyle, Text14)
	if props.Label != "" {
		labelWidth = float32(runtimeTextWidthWithFont(props.Label, font, fontID))
	}
	overlayWidth := float32(0)
	if props.Overlay != "" {
		overlayWidth = float32(runtimeTextWidthWithFont(props.Overlay, font, fontID))
	}
	text := Plot_PlotTextPaintFor(props.Bounds, labelWidth, overlayWidth, 1, plotFrame, props.Label != "", props.Overlay != "")
	if props.Label != "" {
		r.record(FrameOp{Kind: FrameOpText, Bounds: text.LabelBounds, Text: props.Label, Color: unpackRGBA(text.TextColor), Opacity: plotStyle.Opacity, FontSize: font, FontID: fontID})
	}
	if props.Overlay != "" {
		r.record(FrameOp{Kind: FrameOpText, Bounds: text.OverlayBounds, Text: props.Overlay, Color: unpackRGBA(text.TextColor), Opacity: plotStyle.Opacity, FontSize: font, FontID: fontID})
	}
}

func (r *runtime) dragDelta(token, focusID int32, bounds Rectangle, disabled bool) (float32, bool) {
	disabled = disabled || r.contentDisabled()
	if r.drag.active && r.popupInputOwnerCaptures(r.drag.owner) {
		r.drag = scalarDrag{}
	}
	if disabled && r.drag.active && r.drag.token == token {
		r.drag = scalarDrag{}
	}
	if !disabled && r.mousePressed[MouseButtonLeft] && r.consumeTap(bounds) {
		r.drag = scalarDrag{active: true, token: token, lastX: r.mousePos.X, owner: r.currentPopupInputOwner()}
		if focusID > 0 {
			r.setFocus(focusID)
		}
	}
	if r.drag.active && r.drag.token == token && r.mouseDown[MouseButtonLeft] {
		delta := r.mousePos.X - r.drag.lastX
		r.drag.lastX = r.mousePos.X
		return delta, delta != 0
	}
	if r.drag.active && r.drag.token == token && r.mouseReleased[MouseButtonLeft] {
		r.drag = scalarDrag{}
	}
	return 0, false
}

func (r *runtime) dragFloatKeyboard(focusID int32, speed, minimum, maximum, value float32) (float32, bool) {
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			direction := r.sliderKeyboardDirection(false, event.key)
			input := Drag_DragKeyboardInputFor(direction,
				event.key == KeyHome, event.key == KeyEnd,
				r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt],
				event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift])
			step := Drag_DragKeyboardValue(next, speed, minimum, maximum, input)
			if step.Changed {
				next, handled = step.Value, true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return next, next != value
}

func (r *runtime) dragIntKeyboard(focusID int32, speed float32, minimum, maximum, value int32) (int32, bool) {
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			direction := r.sliderKeyboardDirection(false, event.key)
			input := Drag_DragKeyboardInputFor(direction,
				event.key == KeyHome, event.key == KeyEnd,
				r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt],
				event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift])
			step := Drag_DragDiscreteKeyboardValue(next, speed, minimum, maximum, input)
			if step.Changed {
				next, handled = step.Value, true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return next, next != value
}

func (r *runtime) dragFloat(props dragFloatProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	speed := Drag_DragEffectiveSpeed(props.Speed)
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), false)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempFloat(cell,
			numericInputKey{kind: numericEditDragContinuous, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		if enabled {
			if next, keyboardChanged := r.dragFloatKeyboard(focusID, speed, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				changed = true
			}
		}
		if delta, dragged := r.dragDelta(Drag_DragComponentTokenFor(props.ID, int32(i)), focusID, cell, props.Disabled); dragged {
			step := Drag_DragDeltaValue(props.Values[i], delta, speed, props.Min, props.Max)
			changed = changed || step.Changed
			props.Values[i] = step.Value
		}
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, props.Values[i]), !enabled, focused, props.ClassName, props.ID, int32(i))
	}
	r.drawDragLabel(props.Bounds, props.Label, props.ClassName)
	return changed
}

func (r *runtime) dragInt(props dragIntProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	speed := Drag_DragEffectiveSpeed(props.Speed)
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), true)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempInt(cell,
			numericInputKey{kind: numericEditDragDiscrete, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		if enabled {
			if next, keyboardChanged := r.dragIntKeyboard(focusID, speed, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				changed = true
			}
		}
		if delta, dragged := r.dragDelta(Drag_DragComponentTokenFor(props.ID, int32(i)), focusID, cell, props.Disabled); dragged {
			step := Drag_DragDiscreteDeltaValue(props.Values[i], delta, speed, props.Min, props.Max)
			changed = changed || step.Changed
			props.Values[i] = step.Value
		}
		format := props.Format
		if format == "" {
			format = "%d"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, props.Values[i]), !enabled, focused, props.ClassName, props.ID, int32(i))
	}
	r.drawDragLabel(props.Bounds, props.Label, props.ClassName)
	return changed
}

func (r *runtime) dragFloatRange(props dragFloatRangeProps) bool {
	if props.CurrentMin == nil || props.CurrentMax == nil {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	speed := Drag_DragEffectiveSpeed(props.Speed)
	changed := false
	values := [2]*float32{props.CurrentMin, props.CurrentMax}
	formats := [2]string{props.Format, props.FormatMax}
	for i := 0; i < 2; i++ {
		focusID := sliderFocusID(props.ID, int32(i), false)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/2, Y: props.Bounds.Y, Width: props.Bounds.Width / 2, Height: props.Bounds.Height}
		low, high := props.Min, props.Max
		if i == 0 && *props.CurrentMax < high {
			high = *props.CurrentMax
		}
		if i == 1 && *props.CurrentMin > low {
			low = *props.CurrentMin
		}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
			if next, keyboardChanged := r.dragFloatKeyboard(focusID, speed, low, high, *values[i]); keyboardChanged {
				*values[i] = next
				changed = true
			}
		}
		if delta, dragged := r.dragDelta(Drag_DragComponentTokenFor(props.ID, int32(i)), focusID, cell, props.Disabled); dragged {
			step := Drag_DragDeltaValue(*values[i], delta, speed, low, high)
			changed = changed || step.Changed
			*values[i] = step.Value
		}
		format := formats[i]
		if format == "" {
			format = props.Format
		}
		if format == "" {
			format = "%.3f"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, *values[i]), !enabled, focused, props.ClassName, props.ID, int32(i))
	}
	if *props.CurrentMin > *props.CurrentMax {
		*props.CurrentMin = *props.CurrentMax
	}
	r.drawDragLabel(props.Bounds, props.Label, props.ClassName)
	return changed
}

func (r *runtime) dragIntRange(props dragIntRangeProps) bool {
	if props.CurrentMin == nil || props.CurrentMax == nil {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	speed := Drag_DragEffectiveSpeed(props.Speed)
	changed := false
	values := [2]*int32{props.CurrentMin, props.CurrentMax}
	formats := [2]string{props.Format, props.FormatMax}
	for i := 0; i < 2; i++ {
		focusID := sliderFocusID(props.ID, int32(i), true)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/2, Y: props.Bounds.Y, Width: props.Bounds.Width / 2, Height: props.Bounds.Height}
		low, high := props.Min, props.Max
		if i == 0 && *props.CurrentMax < high {
			high = *props.CurrentMax
		}
		if i == 1 && *props.CurrentMin > low {
			low = *props.CurrentMin
		}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
			if next, keyboardChanged := r.dragIntKeyboard(focusID, speed, low, high, *values[i]); keyboardChanged {
				*values[i] = next
				changed = true
			}
		}
		if delta, dragged := r.dragDelta(Drag_DragComponentTokenFor(props.ID, int32(i)), focusID, cell, props.Disabled); dragged {
			step := Drag_DragDiscreteDeltaValue(*values[i], delta, speed, low, high)
			changed = changed || step.Changed
			*values[i] = step.Value
		}
		format := formats[i]
		if format == "" {
			format = props.Format
		}
		if format == "" {
			format = "%d"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, *values[i]), !enabled, focused, props.ClassName, props.ID, int32(i))
	}
	if *props.CurrentMin > *props.CurrentMax {
		*props.CurrentMin = *props.CurrentMax
	}
	r.drawDragLabel(props.Bounds, props.Label, props.ClassName)
	return changed
}

func (r *runtime) Drag(props DragProps) bool {
	count := props.ValueCount
	if count <= 0 {
		if props.Kind == NumericInt {
			count = int32(len(props.IntValues))
		} else {
			count = int32(len(props.FloatValues))
		}
	}
	if props.Mode == DragRange {
		if props.Kind == NumericInt {
			return r.dragIntRange(dragIntRangeProps{
				Bounds:     props.Bounds,
				ID:         props.ID,
				ClassName:  props.ClassName,
				Label:      props.Label,
				CurrentMin: props.IntMin,
				CurrentMax: props.IntMax,
				Speed:      props.Speed,
				Min:        int32(props.Min),
				Max:        int32(props.Max),
				Format:     props.Format,
				FormatMax:  props.FormatMax,
				Disabled:   props.Disabled,
			})
		}
		return r.dragFloatRange(dragFloatRangeProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			ClassName:  props.ClassName,
			Label:      props.Label,
			CurrentMin: props.FloatMin,
			CurrentMax: props.FloatMax,
			Speed:      props.Speed,
			Min:        float32(props.Min),
			Max:        float32(props.Max),
			Format:     props.Format,
			FormatMax:  props.FormatMax,
			Disabled:   props.Disabled,
		})
	}
	if props.Kind == NumericInt {
		return r.dragInt(dragIntProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			ClassName:  props.ClassName,
			Label:      props.Label,
			Values:     props.IntValues,
			ValueCount: count,
			Speed:      props.Speed,
			Min:        int32(props.Min),
			Max:        int32(props.Max),
			Format:     props.Format,
			Disabled:   props.Disabled,
		})
	}
	return r.dragFloat(dragFloatProps{
		Bounds:     props.Bounds,
		ID:         props.ID,
		ClassName:  props.ClassName,
		Label:      props.Label,
		Values:     props.FloatValues,
		ValueCount: count,
		Speed:      props.Speed,
		Min:        float32(props.Min),
		Max:        float32(props.Max),
		Format:     props.Format,
		Disabled:   props.Disabled,
	})
}

func (r *runtime) drawDragCell(bounds Rectangle, text string, disabled, focused bool, className, id, component int32) {
	pressed := r.drag.active && r.drag.token == Drag_DragComponentTokenFor(id, component)
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	} else if pressed {
		state = ButtonStatePressed
	} else if focused {
		state = ButtonStateFocus
	}
	props := ButtonProps{
		Bounds:    bounds,
		Label:     text,
		ID:        id,
		ClassName: className,
		Tone:      ButtonToneNeutral,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
		Disabled:  disabled,
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false,
		className, StyleSheet_StyleKindDragValue(), StyleSheet_StyleAny())
	button := Button_BuildFrame(props, ButtonInput{}, frame, InteractionMotion{},
		Rectangle{}, packRGBA(r.appAmbientColor()), 1, Text14, Text14)
	style := unpackStyle(button.Appearance.Value)
	font, fontID := styleTextFace(style, Text14)
	r.recordButton(FrameOp{Kind: FrameOpButton, Button: button,
		Opacity: style.Opacity, BorderWidth: style.BorderWidth, Radius: style.Radius,
		Material: MaterialKind(style.Material), FillStates: styleFill(style),
		FillStatesValid: true, AmbientColor: r.appAmbientColor(), FocusColor: style.Focus,
		Bounds: bounds, Text: text, Color: style.Background, BorderColor: style.Border,
		TextColor: style.Foreground, FontSize: font, FontID: fontID, ID: id, Row: component,
		Disabled: disabled, Pressed: pressed, Focused: focused})
}

func (r *runtime) drawDragLabel(bounds Rectangle, label string, className int32) {
	if label == "" {
		return
	}
	style := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false,
		false, className, StyleSheet_StyleKindDrag(), StyleSheet_StyleAny()).Value)
	font, fontID := styleTextFace(style, Text14)
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y - float32(font) - 4, Width: bounds.Width - 12, Height: float32(font)}, Text: label, Color: style.Foreground, Opacity: style.Opacity, FontSize: font, FontID: fontID})
}

func (r *runtime) numericTempEdit(bounds Rectangle, key numericInputKey, focusID int32, formatted string, disabled bool) (*numericInputState, bool, bool) {
	enabled := !disabled && !r.contentDisabled()
	control := r.keyDown[KeyLeftControl] || r.keyDown[KeyRightControl]
	pressed := enabled && r.mousePressed[MouseButtonLeft] && r.hasTap(bounds)
	now := time.Now()
	dx := r.mousePos.X - r.lastNumericClick.x
	dy := r.mousePos.Y - r.lastNumericClick.y
	doubleClick := pressed && r.lastNumericClick.key == key &&
		now.Sub(r.lastNumericClick.when) <= 300*time.Millisecond &&
		dx >= -6 && dx <= 6 && dy >= -6 && dy <= 6
	activate := pressed && (control || doubleClick) && r.consumeTap(bounds)
	if pressed {
		r.lastNumericClick = numericClick{key: key, x: r.mousePos.X, y: r.mousePos.Y, when: now}
		if activate {
			r.lastNumericClick = numericClick{}
		}
	}
	state := r.numericInputs[key]
	if state == nil && !activate {
		return nil, false, false
	}
	if state == nil {
		state = r.numericInputState(key, formatted)
	}
	if !enabled && state.focused {
		state.focused = false
		if r.focusID == focusID {
			r.setFocus(0)
		}
	}
	if activate {
		r.setNumericInputText(key, formatted)
		state.focused = true
		r.setFocus(focusID)
		r.drag = scalarDrag{}
		r.slider = scalarDrag{}
	}
	if !state.focused {
		return state, false, false
	}
	commit := false
	textChanged := r.editText(bounds, state.text, &state.cursor, &state.focused, &commit, focusID, textEditOptions{maxCodepoints: 63})
	r.recordTextInput(FrameOpTextField, bounds, state.text, &state.cursor, &state.focused, focusID, Text14, false, false)
	if commit {
		state.focused = false
		r.setFocus(focusID)
	}
	return state, textChanged, state.focused
}

func (r *runtime) numericTempFloat(bounds Rectangle, key numericInputKey, focusID int32, values []float32, index int, format string, disabled bool) (bool, bool) {
	if format == "" {
		format = "%.3f"
	}
	state, textChanged, editing := r.numericTempEdit(bounds, key, focusID, fmt.Sprintf(format, values[index]), disabled)
	if textChanged {
		text := string(state.text[:zeroIndex(state.text)])
		if parsed, err := strconv.ParseFloat(text, 32); err == nil && !math.IsInf(parsed, 0) && !math.IsNaN(parsed) && values[index] != float32(parsed) {
			values[index] = float32(parsed)
			return true, editing
		}
	}
	return false, editing
}

func (r *runtime) numericTempInt(bounds Rectangle, key numericInputKey, focusID int32, values []int32, index int, format string, disabled bool) (bool, bool) {
	if format == "" {
		format = "%d"
	}
	state, textChanged, editing := r.numericTempEdit(bounds, key, focusID, fmt.Sprintf(format, values[index]), disabled)
	if textChanged {
		text := string(state.text[:zeroIndex(state.text)])
		if parsed, err := strconv.ParseInt(text, 0, 32); err == nil && values[index] != int32(parsed) {
			values[index] = int32(parsed)
			return true, editing
		}
	}
	return false, editing
}

func (r *runtime) sliderRatio(token, focusID int32, bounds Rectangle, disabled, vertical bool) (float32, bool) {
	disabled = disabled || r.contentDisabled()
	if r.slider.active && r.popupInputOwnerCaptures(r.slider.owner) {
		r.slider = scalarDrag{}
	}
	if disabled && r.slider.active && r.slider.token == token {
		r.slider = scalarDrag{}
	}
	pressed := !disabled && r.mousePressed[MouseButtonLeft] && r.consumeTap(bounds)
	if pressed {
		r.slider = scalarDrag{active: true, token: token, owner: r.currentPopupInputOwner()}
		if focusID > 0 {
			r.setFocus(focusID)
		}
	}
	if r.slider.active && r.slider.token == token && (pressed || r.mouseDown[MouseButtonLeft]) {
		var ratio float32
		if vertical {
			if bounds.Height > 0 {
				ratio = (bounds.Y + bounds.Height - r.mousePos.Y) / bounds.Height
			}
		} else if bounds.Width > 0 {
			ratio = (r.mousePos.X - bounds.X) / bounds.Width
		}
		if ratio < 0 {
			ratio = 0
		} else if ratio > 1 {
			ratio = 1
		}
		return ratio, true
	}
	if r.slider.active && r.slider.token == token && r.mouseReleased[MouseButtonLeft] {
		r.slider = scalarDrag{}
	}
	return 0, false
}

func sliderFocusID(id, component int32, integer bool) int32 {
	return Slider_SliderFocusIdFor(id, component, integer)
}

func (r *runtime) sliderKeyboardDirection(vertical bool, key int32) int32 {
	if vertical {
		if key == KeyUp {
			return 1
		}
		if key == KeyDown {
			return -1
		}
	} else {
		if key == KeyRight {
			return 1
		}
		if key == KeyLeft {
			return -1
		}
	}
	return 0
}

func (r *runtime) sliderFloatKeyboard(focusID int32, vertical bool, minimum, maximum, value float32) (float32, bool) {
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			direction := r.sliderKeyboardDirection(vertical, event.key)
			step := Slider_SliderKeyboardValue(next, minimum, maximum,
				direction, event.key == KeyHome, event.key == KeyEnd,
				r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt],
				event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift])
			if step.Changed {
				next, handled = step.Value, true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return next, next != value
}

func (r *runtime) sliderIntKeyboard(focusID int32, vertical bool, minimum, maximum, value int32) (int32, bool) {
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			direction := r.sliderKeyboardDirection(vertical, event.key)
			step := Slider_SliderDiscreteKeyboardValue(next, minimum, maximum,
				direction, event.key == KeyHome, event.key == KeyEnd,
				r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt],
				event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift])
			if step.Changed {
				next, handled = step.Value, true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return next, next != value
}

func (r *runtime) drawSliderCell(bounds Rectangle, ratio float32, text string, disabled, vertical, focused bool, className, id, component int32) {
	hovered := !disabled && pointInRect(r.mousePos.X, r.mousePos.Y, bounds)
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	} else if focused {
		state = ButtonStateFocus
	} else if hovered {
		state = ButtonStateHover
	}
	trackFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false, className, StyleSheet_StyleKindSlider(), Slider_SliderTrackRole())
	activeFrame := simpleStyleFrameWithClassRole(ButtonToneAccent, state, disabled, true, className, StyleSheet_StyleKindSlider(), Slider_SliderFillRole())
	labelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false, className, StyleSheet_StyleKindSlider(), Slider_SliderLabelRole())
	trackStyle := unpackStyle(trackFrame.Value)
	activeStyle := unpackStyle(activeFrame.Value)
	labelStyle := unpackStyle(labelFrame.Value)
	trackOp := styleFrameRectOp(bounds, Rectangle{}, trackFrame)
	trackOp.ID = id
	trackOp.Row = component
	trackOp.Disabled = disabled
	trackOp.Focused = focused
	trackOp.Hovered = hovered
	if focused {
		trackOp.BorderColor = trackOp.FocusColor
	}
	r.record(trackOp)
	if vertical {
		fill := Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height*(1-ratio), Width: bounds.Width, Height: bounds.Height * ratio}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: fill, Color: activeStyle.Background, BorderColor: activeStyle.Border, ID: id, Row: component, Selected: true, Disabled: disabled})
		y := bounds.Y + bounds.Height*(1-ratio)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: y, Width: bounds.Width}, Color: trackStyle.Foreground, ID: id, Row: component})
	} else {
		fill := Rectangle{X: bounds.X, Y: bounds.Y, Width: bounds.Width * ratio, Height: bounds.Height}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: fill, Color: activeStyle.Background, BorderColor: activeStyle.Border, ID: id, Row: component, Selected: true, Disabled: disabled})
		x := bounds.X + bounds.Width*ratio
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: x, Y: bounds.Y, Height: bounds.Height}, Color: trackStyle.Foreground, ID: id, Row: component})
	}
	labelFont, labelFontID := styleTextFace(labelStyle, Text14)
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y + (bounds.Height-float32(labelFont))/2, Width: bounds.Width - 12, Height: float32(labelFont)}, Text: text, Color: labelStyle.Foreground, Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, ID: id, Row: component})
}

func (r *runtime) drawSliderLabel(bounds Rectangle, label string, className, id int32) {
	if label != "" {
		style := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false, className, StyleSheet_StyleKindSlider(), Slider_SliderLabelRole()).Value)
		font, fontID := styleTextFace(style, Text14)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y - float32(font) - 4, Width: bounds.Width - 12, Height: float32(font)}, Text: label, Color: style.Foreground, Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: id})
	}
}

func (r *runtime) sliderFloat(props sliderFloatProps, vertical bool) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), false)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempFloat(cell,
			numericInputKey{kind: numericEditSliderContinuous, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		ratio := Slider_SliderRatio(props.Values[i], props.Min, props.Max)
		if enabled {
			if next, keyboardChanged := r.sliderFloatKeyboard(focusID, vertical, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				ratio = Slider_SliderRatio(next, props.Min, props.Max)
				changed = true
			}
		}
		if next, active := r.sliderRatio(Slider_SliderFocusIdFor(props.ID, int32(i), false), focusID, cell, props.Disabled, vertical); active && props.Max > props.Min {
			ratio = next
			value := Slider_SliderValue(props.Min, props.Max, ratio)
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawSliderCell(cell, ratio, fmt.Sprintf(format, props.Values[i]), props.Disabled, vertical, focused, props.ClassName, props.ID, int32(i))
	}
	r.drawSliderLabel(props.Bounds, props.Label, props.ClassName, props.ID)
	return changed
}

func (r *runtime) sliderInt(props sliderIntProps, vertical bool) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), true)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempInt(cell,
			numericInputKey{kind: numericEditSliderDiscrete, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		ratio := Slider_SliderDiscreteRatio(props.Values[i], props.Min, props.Max)
		if enabled {
			if next, keyboardChanged := r.sliderIntKeyboard(focusID, vertical, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				ratio = Slider_SliderDiscreteRatio(next, props.Min, props.Max)
				changed = true
			}
		}
		if next, active := r.sliderRatio(Slider_SliderFocusIdFor(props.ID, int32(i), true), focusID, cell, props.Disabled, vertical); active && props.Max > props.Min {
			ratio = next
			value := Slider_SliderDiscreteValue(props.Min, props.Max, ratio)
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%d"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawSliderCell(cell, ratio, fmt.Sprintf(format, props.Values[i]), props.Disabled, vertical, focused, props.ClassName, props.ID, int32(i))
	}
	r.drawSliderLabel(props.Bounds, props.Label, props.ClassName, props.ID)
	return changed
}

func (r *runtime) sliderAngle(props sliderAngleProps) bool {
	if props.Value == nil {
		return false
	}
	degrees := *props.Value * 57.29577951308232
	format := props.Format
	if format == "" {
		format = "%.0f deg"
	}
	values := []float32{degrees}
	changed := r.sliderFloat(sliderFloatProps{Bounds: props.Bounds, ID: props.ID, Label: props.Label, Values: values, ValueCount: 1, Min: props.MinDegrees, Max: props.MaxDegrees, Format: format, Disabled: props.Disabled, ClassName: props.ClassName}, false)
	if changed {
		*props.Value = values[0] * 0.017453292519943295
	}
	return changed
}

func (r *runtime) Slider(props SliderProps) bool {
	count := props.ValueCount
	if count <= 0 {
		if props.Kind == NumericInt {
			count = int32(len(props.IntValues))
		} else {
			count = int32(len(props.FloatValues))
		}
	}
	if props.Angle {
		return r.sliderAngle(sliderAngleProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Value:      props.FloatValue,
			MinDegrees: float32(props.Min),
			MaxDegrees: float32(props.Max),
			Format:     props.Format,
			Disabled:   props.Disabled,
			ClassName:  props.ClassName,
		})
	}
	if props.Kind == NumericInt {
		slider := sliderIntProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Values:     props.IntValues,
			ValueCount: count,
			Min:        int32(props.Min),
			Max:        int32(props.Max),
			Format:     props.Format,
			Disabled:   props.Disabled,
			ClassName:  props.ClassName,
		}
		if props.Vertical {
			return r.sliderInt(slider, true)
		}
		return r.sliderInt(slider, false)
	}
	slider := sliderFloatProps{
		Bounds:     props.Bounds,
		ID:         props.ID,
		Label:      props.Label,
		Values:     props.FloatValues,
		ValueCount: count,
		Min:        float32(props.Min),
		Max:        float32(props.Max),
		Format:     props.Format,
		Disabled:   props.Disabled,
		ClassName:  props.ClassName,
	}
	if props.Vertical {
		return r.sliderFloat(slider, true)
	}
	return r.sliderFloat(slider, false)
}

func (r *runtime) numericInputState(key numericInputKey, formatted string) *numericInputState {
	if r.numericInputs == nil {
		r.numericInputs = make(map[numericInputKey]*numericInputState)
		r.numericNextToken = 0x60000000
	}
	state := r.numericInputs[key]
	if state == nil {
		if r.numericNextToken > 0x7fffffff-3 {
			panic("numeric input identity space exhausted")
		}
		state = &numericInputState{text: make([]byte, 64), token: r.numericNextToken}
		r.numericNextToken += 3
		r.numericInputs[key] = state
	}
	if !state.focused {
		clear(state.text)
		copy(state.text, formatted)
		state.cursor = int32(len(formatted))
	}
	return state
}

func (r *runtime) setNumericInputText(key numericInputKey, formatted string) {
	state := r.numericInputs[key]
	if state == nil {
		return
	}
	clear(state.text)
	copy(state.text, formatted)
	state.cursor = int32(len(formatted))
}

func (r *runtime) numericInputCell(bounds Rectangle, key numericInputKey, formatted string, disabled bool, stepEnabled bool) (string, int32, bool, bool) {
	state := r.numericInputState(key, formatted)
	token := state.token
	field := bounds
	minus, plus := bounds, bounds
	if stepEnabled {
		buttonWidth := float32(24)
		field.Width -= buttonWidth * 2
		minus = Rectangle{X: field.X + field.Width, Y: bounds.Y, Width: buttonWidth, Height: bounds.Height}
		plus = Rectangle{X: minus.X + buttonWidth, Y: bounds.Y, Width: buttonWidth, Height: bounds.Height}
	}
	textChanged := false
	if disabled {
		state.focused = false
	} else {
		var commit bool
		textChanged = r.editText(field, state.text, &state.cursor, &state.focused, &commit, token, textEditOptions{maxCodepoints: 63})
	}
	r.recordTextInput(FrameOpTextField, field, state.text, &state.cursor, &state.focused, token, Text14, false, false)
	if !stepEnabled {
		return string(state.text[:zeroIndex(state.text)]), 0, false, textChanged
	}
	minusPressed := r.buttonAt(ButtonProps{Bounds: minus, Label: "-", Size: ControlSizeSmall,
		ID: token + 1, Disabled: disabled})
	plusPressed := r.buttonAt(ButtonProps{Bounds: plus, Label: "+", Size: ControlSizeSmall,
		ID: token + 2, Disabled: disabled})
	if !minusPressed && !plusPressed {
		return string(state.text[:zeroIndex(state.text)]), 0, false, textChanged
	}
	direction := int32(1)
	if minusPressed {
		direction = -1
	}
	fast := r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift]
	return string(state.text[:zeroIndex(state.text)]), direction, fast, textChanged
}

func (r *runtime) inputFloat(props inputFloatProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	changed := false
	for i := 0; i < count; i++ {
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		key := numericInputKey{kind: 0, widgetID: props.ID, component: int32(i)}
		text, direction, fast, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.Step != 0)
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseFloat(text, 32); err == nil {
				value, valid = float32(parsed), true
			}
		}
		if direction != 0 {
			step := Input_InputContinuousStepValue(value, props.Step, props.StepFast, direction, fast)
			value, valid = step.Value, true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label, 0, props.ID)
	return changed
}

func (r *runtime) inputInt(props inputIntProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	changed := false
	for i := 0; i < count; i++ {
		format := props.Format
		if format == "" {
			format = "%d"
		}
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		key := numericInputKey{kind: 1, widgetID: props.ID, component: int32(i)}
		text, direction, fast, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.Step != 0)
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseInt(text, 0, 32); err == nil {
				value, valid = int32(parsed), true
			}
		}
		if direction != 0 {
			step := Input_InputDiscreteStepValue(value, props.Step, props.StepFast, direction, fast)
			value, valid = step.Value, true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label, 0, props.ID)
	return changed
}

func (r *runtime) inputDouble(props inputDoubleProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	changed := false
	for i := 0; i < count; i++ {
		format := props.Format
		if format == "" {
			format = "%.6f"
		}
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		key := numericInputKey{kind: 2, widgetID: props.ID, component: int32(i)}
		text, direction, fast, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.Step != 0)
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseFloat(text, 64); err == nil {
				value, valid = parsed, true
			}
		}
		if direction != 0 {
			step := Input_InputStepValue(value, props.Step, props.StepFast, direction, fast)
			value, valid = step.Value, true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label, 0, props.ID)
	return changed
}

func (r *runtime) Input(props InputProps) bool {
	count := props.ValueCount
	if count <= 0 {
		switch props.Kind {
		case NumericInt:
			count = int32(len(props.IntValues))
		case NumericDouble:
			count = int32(len(props.DoubleValues))
		default:
			count = int32(len(props.FloatValues))
		}
	}
	switch props.Kind {
	case NumericInt:
		return r.inputInt(inputIntProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Values:     props.IntValues,
			ValueCount: count,
			Step:       int32(props.Step),
			StepFast:   int32(props.StepFast),
			Format:     props.Format,
			Disabled:   props.Disabled,
		})
	case NumericDouble:
		return r.inputDouble(inputDoubleProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Values:     props.DoubleValues,
			ValueCount: count,
			Step:       props.Step,
			StepFast:   props.StepFast,
			Format:     props.Format,
			Disabled:   props.Disabled,
		})
	default:
		return r.inputFloat(inputFloatProps{
			Bounds:     props.Bounds,
			ID:         props.ID,
			Label:      props.Label,
			Values:     props.FloatValues,
			ValueCount: count,
			Step:       float32(props.Step),
			StepFast:   float32(props.StepFast),
			Format:     props.Format,
			Disabled:   props.Disabled,
		})
	}
}

func (r *runtime) Dropdown(args ...any) bool {
	if len(args) == 1 {
		switch p := args[0].(type) {
		case DropdownProps:
			return r.dropdownFromProps(p)
		case *DropdownProps:
			if p == nil {
				return false
			}
			return r.dropdownFromProps(*p)
		}
	}
	if len(args) < 6 {
		return false
	}
	id, ok := anyInt32(args[0])
	if !ok {
		return false
	}
	x, ok := anyInt32(args[1])
	if !ok {
		return false
	}
	y, ok := anyInt32(args[2])
	if !ok {
		return false
	}
	w, ok := anyInt32(args[3])
	if !ok {
		return false
	}
	h, ok := anyInt32(args[4])
	if !ok {
		return false
	}
	options := args[5]
	rest := args[6:]
	labels := labelsOf(options)
	count := int32(len(labels))
	selected := dropdownSelected(rest...)
	if len(rest) > 0 {
		if v, ok := anyInt32(rest[0]); ok && v >= 0 && v < count {
			count = v
		}
	}
	if int(count) < len(labels) {
		labels = labels[:count]
	}
	if !r.contentDisabled() && selected != nil && len(labels) > 0 {
		*selected = clamp32(*selected, 0, int32(len(labels)-1))
	}
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(y), Width: float32(w), Height: float32(h)})
	return r.dropdownAt(id, bounds, labels, selected)
}

func (r *runtime) dropdownFromProps(p DropdownProps) bool {
	p.Bounds = r.layoutRect(p.Bounds)
	if len(p.Items) > 0 {
		count := len(p.Items)
		if p.OptionCount > 0 && int(p.OptionCount) < count {
			count = int(p.OptionCount)
		}
		labels := make([]string, count)
		for i := range labels {
			labels[i] = p.Items[i].Label
		}
		r.DisabledScope(p.Disabled)
		defer r.DisabledEndScope()
		return r.dropdownOptionsAt(p.ID, p.Bounds, labels, p.Items[:count], p.SelectedIndex, p.ClassName)
	}
	n := p.OptionCount
	if n <= 0 || n > int32(len(p.Options)) {
		n = int32(len(p.Options))
	}
	opts := p.Options[:n]
	r.DisabledScope(p.Disabled)
	defer r.DisabledEndScope()
	return r.dropdownAt(p.ID, p.Bounds, opts, p.SelectedIndex, p.ClassName)
}

func (r *runtime) GetSegmentedControlHeight(props SegmentedControlProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	return r.segmentedControlHeight(props)
}

func (r *runtime) SegmentedControl(props SegmentedControlProps) SegmentedControlResult {
	props.Bounds = r.layoutRect(props.Bounds)
	selected := int32(-1)
	if props.SelectedIndex != nil {
		selected = *props.SelectedIndex
	}
	result := SegmentedControlResult{
		SelectedIndex: selected,
		ClickedIndex:  -1,
		Height:        r.segmentedControlHeight(props),
	}
	count := r.segmentedControlCount(props)
	metrics := r.segmentedControlMetrics(props)
	if count <= 0 || props.Bounds.Width <= 0 || metrics.RowHeight <= 0 {
		return result
	}
	font, fontID := r.segmentedControlTextFace(props.ClassName)
	rowStart := 0
	rowWidth := int32(0)
	rowCount := 0
	y := int32(props.Bounds.Y)
	for i := 0; i <= count; i++ {
		endRow := i == count
		itemWidth := int32(0)
		if !endRow {
			itemWidth = r.segmentedOptionWidth(props.Options[i], font, fontID, metrics)
		}
		nextWidth := SegmentedControl_SegmentedNextRowWidth(rowWidth, itemWidth, metrics.Gap)
		if !endRow && !SegmentedControl_SegmentedShouldWrap(props.Wrap, rowWidth, nextWidth, int32(props.Bounds.Width)) {
			rowWidth = nextWidth
			rowCount++
			continue
		}
		if rowCount > 0 {
			row := SegmentedControl_SegmentedRowFor(props.Bounds.X, props.Bounds.Width, y,
				int32(rowStart), int32(rowCount), rowWidth, props.Wrap, metrics)
			for j := 0; j < rowCount; j++ {
				index := rowStart + j
				option := props.Options[index]
				bounds := Rectangle{
					X:      float32(row.X + int32(j)*(row.ButtonWidth+metrics.Gap)),
					Y:      float32(row.Y),
					Width:  float32(row.ButtonWidth),
					Height: float32(metrics.RowHeight),
				}
				pressed := r.segmentedButtonAt(ButtonProps{
					Bounds:    bounds,
					ID:        SegmentedControl_SegmentedFocusIdFor(props.ID, int32(index)),
					Label:     option.Label,
					Pill:      true,
					Selected:  int32(index) == selected,
					Disabled:  option.Disabled || r.contentDisabled(),
					ClassName: props.ClassName,
				}, font)
				if pressed {
					selection := SegmentedControl_SegmentedSelectionFor(
						selected, int32(index), props.SelectedIndex != nil)
					result.ClickedIndex = int32(index)
					result.SelectedIndex = selection.SelectedIndex
					if selection.Changed {
						result.Changed = 1
					}
					if selection.Changed && props.SelectedIndex != nil {
						*props.SelectedIndex = selection.SelectedIndex
					}
					selected = selection.SelectedIndex
				}
			}
			y += metrics.RowHeight + metrics.Gap
		}
		rowStart = i
		rowWidth = itemWidth
		rowCount = 1
	}
	return result
}

func (r *runtime) segmentedButtonAt(props ButtonProps, font int32) bool {
	frame, pressed := r.surfaceButtonFrameForKind(props, Rectangle{}, false,
		StyleSheet_StyleKindSegment())
	if font > 0 {
		frame.Button.Font = font
		frame.Button.Appearance.Value.FontSize = float32(font)
	} else if frame.Button.Appearance.Value.FontSize > 0 {
		frame.Button.Font = int32(frame.Button.Appearance.Value.FontSize + 0.5)
	}
	r.record(frame)
	return pressed
}

func (r *runtime) segmentedControlHeight(props SegmentedControlProps) int32 {
	count := r.segmentedControlCount(props)
	metrics := r.segmentedControlMetrics(props)
	if count <= 0 || metrics.RowHeight <= 0 {
		return 0
	}
	if props.Bounds.Width <= 0 || !props.Wrap {
		return metrics.RowHeight
	}
	font, fontID := r.segmentedControlTextFace(props.ClassName)
	rows := int32(1)
	rowWidth := int32(0)
	for i := 0; i < count; i++ {
		itemWidth := r.segmentedOptionWidth(props.Options[i], font, fontID, metrics)
		nextWidth := SegmentedControl_SegmentedNextRowWidth(rowWidth, itemWidth, metrics.Gap)
		if SegmentedControl_SegmentedShouldWrap(props.Wrap, rowWidth, nextWidth, int32(props.Bounds.Width)) {
			rows++
			rowWidth = itemWidth
		} else {
			rowWidth = nextWidth
		}
	}
	return SegmentedControl_SegmentedHeightForRows(rows, metrics.RowHeight, metrics.Gap)
}

func (r *runtime) segmentedControlCount(props SegmentedControlProps) int {
	count := len(props.Options)
	if props.OptionCount > 0 && int(props.OptionCount) < count {
		count = int(props.OptionCount)
	}
	return count
}

func (r *runtime) segmentedControlMetrics(props SegmentedControlProps) SegmentedMetrics {
	control := r.segmentedControlFrame(props.ClassName)
	segment := r.segmentFrame(props.ClassName)
	return SegmentedControl_SegmentedDefaultMetrics(props.Height,
		props.MinItemWidth, props.MaxItemWidth, 1, control, segment)
}

func (r *runtime) segmentedControlFont(className ...int32) int32 {
	size, _ := r.segmentedControlTextFace(className...)
	return size
}

func (r *runtime) segmentedControlTextFace(className ...int32) (int32, uint32) {
	styleClass := int32(0)
	if len(className) > 0 {
		styleClass = className[0]
	}
	style := unpackStyle(r.segmentFrame(styleClass).Value)
	return styleTextFace(style, Text14)
}

func (r *runtime) segmentedControlFrame(className int32) StyleFrame {
	props := ButtonProps{ClassName: int32(0)}
	props.ClassName = className
	return simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindSegmentedControl(),
		StyleSheet_StyleAny())
}

func (r *runtime) segmentFrame(className int32) StyleFrame {
	props := ButtonProps{ClassName: int32(0)}
	props.ClassName = className
	return simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindSegment(),
		StyleSheet_StyleAny())
}

func (r *runtime) segmentedOptionWidth(option SegmentOption, font int32, fontID uint32, metrics SegmentedMetrics) int32 {
	return SegmentedControl_SegmentedItemWidth(int32(runtimeTextWidthWithFont(option.Label, font, fontID)), metrics)
}

func (r *runtime) dropdownKeyboardAvailable(id int32) bool {
	if _, openPopup := r.popupPanels[id]; openPopup {
		return !r.popupKeyboardCapturesOwner(id, true)
	}
	return !r.popupFocusCaptures(id)
}

func (r *runtime) dropdownAt(id int32, bounds Rectangle, labels []string, selected *int32, className ...int32) bool {
	return r.dropdownOptionsAt(id, bounds, labels, nil, selected, className...)
}

func (r *runtime) dropdownOptionsAt(id int32, bounds Rectangle, labels []string, items []DropdownOption, selected *int32, className ...int32) bool {
	styleClass := int32(0)
	if len(className) > 0 {
		styleClass = className[0]
	}
	triggerFrame := StyleFrame{Value: packStyle(r.dropdownStyle(0, false,
		ButtonStateNormal, styleClass))}
	contentMetrics := Dropdown_Content(triggerFrame.Value, 1)
	triggerMetrics := Dropdown_DropdownTriggerMetricsFor(1, triggerFrame)
	disabledRow := func(index int32) bool {
		return index >= 0 && int(index) < len(items) && items[index].Disabled
	}
	if r.dropdownsSeen == nil {
		r.dropdownsSeen = make(map[int32]bool)
	}
	r.dropdownsSeen[id] = true
	if r.contentDisabled() {
		r.closeDropdown(id)
	}
	pointerActivate := r.consumeTap(bounds)
	if !r.contentDisabled() && id > 0 {
		r.registerField(id)
		if pointerActivate {
			r.setFocus(id)
		}
	}
	previousOpen := r.openDropdowns[id]
	keyboardAvailable := r.dropdownKeyboardAvailable(id)
	panel := r.dropdownPanel(bounds, len(labels), styleClass)
	outside := r.mousePressed[MouseButtonLeft] && !pointInRect(r.mousePos.X, r.mousePos.Y, bounds) && !pointInRect(r.mousePos.X, r.mousePos.Y, panel)
	for _, tap := range r.taps {
		outside = outside || (!pointInRect(tap.x, tap.y, bounds) && !pointInRect(tap.x, tap.y, panel))
	}
	triggerInput := Dropdown_DropdownTriggerInputFor(r.keyDown[KeyEnter],
		r.keyDown[335], r.keyDown[KeySpace], r.keyDown[KeyDown])
	openDecision := Dropdown_DropdownOpenDecisionFor(previousOpen,
		r.contentDisabled(), int32(len(labels)), id > 0 && r.focusID == id,
		!r.popupKeyboardCaptures(), pointerActivate, triggerInput,
		bounds.Height, keyboardAvailable && r.keyDown[KeyEscape], false, outside)
	open := openDecision.Open
	pressed := openDecision.Changed
	if openDecision.Opened {
		for other := range r.openDropdowns {
			if other != id {
				r.closeDropdown(other)
			}
		}
		if r.dropdownHighlight == nil {
			r.dropdownHighlight = make(map[int32]int32)
		}
		chosen := int32(0)
		if selected != nil {
			chosen = *selected
		}
		r.dropdownHighlight[id] = Dropdown_ClampIndex(chosen, int32(len(labels)))
		if offset := r.dropdownOffsets[id]; offset != nil {
			*offset = 0
		}
		delete(r.dropdownGestures, id)
	}
	r.openDropdowns[id] = open
	changed := false
	navigating := !pressed && keyboardAvailable && (r.keyDown[KeyUp] || r.keyDown[KeyDown] || r.keyDown[KeyHome] || r.keyDown[KeyEnd])
	if open && (pressed || navigating) {
		nav := Dropdown_StartNavigation(r.dropdownHighlight[id], int32(len(labels)),
			navigating && r.keyDown[KeyUp], navigating && r.keyDown[KeyDown],
			navigating && r.keyDown[KeyHome], navigating && r.keyDown[KeyEnd])
		for nav.Searching {
			nav = Dropdown_ScanNavigation(nav, !disabledRow(nav.Index))
		}
		if r.dropdownHighlight == nil {
			r.dropdownHighlight = make(map[int32]int32)
		}
		r.dropdownHighlight[id] = nav.Result
	}
	highlight := r.dropdownHighlight[id]
	if open && Dropdown_CanCommit(highlight >= 0 && int(highlight) < len(labels) && !disabledRow(highlight),
		pressed, keyboardAvailable, r.keyDown[KeyEnter] || r.keyDown[335], false, false, false, false) {
		if selected != nil {
			changed = *selected != highlight
			*selected = highlight
		}
		openDecision := Dropdown_DropdownOpenAfterCommit(open, true)
		open = openDecision.Open
	}
	if !open {
		r.closeDropdown(id)
	}
	focused := !r.contentDisabled() && id > 0 && r.focusID == id
	foreground := r.dropdownTrigger(id, bounds, open, focused, styleClass)
	hasIcon := selected != nil && *selected >= 0 && int(*selected) < len(items) && items[*selected].IconType != IconNone
	triggerContent := Dropdown_DropdownTriggerContentFor(bounds, contentMetrics, triggerMetrics, hasIcon)
	if hasIcon {
		r.record(FrameOp{Kind: FrameOpIcon, Bounds: triggerContent.IconBounds, IconType: items[*selected].IconType, Color: foreground, ID: id})
	}
	r.record(FrameOp{Kind: FrameOpText, Clip: triggerContent.ClipBounds, HasClip: true, Bounds: triggerContent.TextBounds, Text: selectedLabel(labels, selected), Color: foreground, FontSize: int32(contentMetrics.Font), ID: id, Row: -1})
	r.dropdownChevron(id, triggerContent, triggerMetrics.IndicatorSize, open, foreground)
	if !open {
		return changed
	}
	itemH := bounds.Height
	layer := r.beginPaintLayer(id)
	input := r.beginPopupInput(id, panel)
	defer func() {
		r.endPopupInput(input)
		r.endPaintLayer(layer)
	}()
	surface := r.dropdownSurface(panel, Dropdown_DropdownPanelRole(), false, ButtonStateNormal, styleClass)
	surface.ID = id
	r.record(surface)
	menuMetrics := Dropdown_DropdownMenuMetricsFor(1,
		r.dropdownRoleFrame(Dropdown_DropdownPanelRole(), styleClass),
		r.dropdownRoleFrame(Dropdown_DropdownOptionRole(), styleClass),
		r.dropdownRoleFrame(Dropdown_DropdownScrollbarRole(), styleClass))
	menuLayout := Dropdown_MenuLayoutFor(panel, int32(len(labels)), int32(itemH),
		menuMetrics.PaddingTop, menuMetrics.PaddingBottom,
		menuMetrics.ScrollbarWidth, menuMetrics.ScrollbarGap)
	if r.dropdownOffsets == nil {
		r.dropdownOffsets = make(map[int32]*int32)
	}
	offset := r.dropdownOffsets[id]
	if offset == nil {
		offset = new(int32)
		r.dropdownOffsets[id] = offset
	}
	viewport := menuLayout.ContentBounds
	contentHeight := menuLayout.ContentHeight
	maximum := menuLayout.MaxScroll
	if pressed || navigating {
		*offset = Dropdown_RevealRow(*offset, r.dropdownHighlight[id], itemH, viewport.Height, maximum)
	}
	if r.dropdownGestures == nil {
		r.dropdownGestures = make(map[int32]PopupGesture)
	}
	track := Dropdown_ScrollbarTrackBounds(menuLayout.ScrollbarBounds, menuMetrics.ScrollbarTrackInset)
	if maximum > 0 && !r.contentDisabled() && r.mousePressed[MouseButtonLeft] && r.consumeTap(track) {
		thumbH := min(track.Height, max(float32(menuMetrics.ScrollbarWidth), track.Height*track.Height/float32(contentHeight)))
		thumbY := track.Y
		travel := track.Height - thumbH
		if travel > 0 {
			thumbY += travel * float32(*offset) / float32(maximum)
		}
		r.scrollDragOffset = offset
		r.scrollDragGrab = thumbH / 2
		if r.mousePos.Y >= thumbY && r.mousePos.Y < thumbY+thumbH {
			r.scrollDragGrab = r.mousePos.Y - thumbY
		}
	}
	scrollbar := r.scrollDragOffset == offset || maximum > 0 && pointInRect(r.mousePos.X, r.mousePos.Y, track)
	gesture := Dropdown_PopupDragGesture(r.dropdownGestures[id], *offset, r.mouseDown[MouseButtonLeft],
		pointInRect(r.mousePos.X, r.mousePos.Y, panel) || pointInRect(r.mousePos.X, r.mousePos.Y, bounds),
		scrollbar, r.mousePos.Y, maximum, float32(menuMetrics.DragThreshold))
	*offset = gesture.Offset
	r.dropdownGestures[id] = gesture
	if r.pointerCanReach(viewport) && r.mouseWheel != 0 {
		*offset = Dropdown_WheelOffset(*offset, r.mouseWheel, itemH, maximum)
		r.mouseWheel = 0
	}
	// The generic scroll host owns clipping and its scrollbar. Dropdown's
	// wheel, drag, reveal, and visible-row decisions come from shared policy.
	content := r.ScrollScope(viewport, contentHeight, offset)
	defer r.ScrollEndScope()
	rows := Dropdown_Rows(int32(len(labels)), *offset, viewport.Height, itemH)
	for i := int(rows.First); i < int(rows.End); i++ {

		label := labels[i]
		optionPaint := Dropdown_OptionPaintFor(panel, menuLayout.OptionWidth,
			int32(i), int32(itemH), *offset, menuMetrics.PaddingTop,
			menuMetrics.PaddingBottom, menuMetrics.HighlightInsetX,
			menuMetrics.HighlightInsetY)
		row := Rectangle{X: content.X, Y: float32(optionPaint.OptionY),
			Width: content.Width, Height: itemH}
		visibleRow := optionPaint.VisibleBounds
		selectedRow := selected != nil && int32(i) == *selected
		highlighted := !disabledRow(int32(i)) && (r.dropdownHighlight[id] == int32(i) || pointInRect(r.mousePos.X, r.mousePos.Y, visibleRow))
		state := ButtonStateNormal
		if highlighted {
			state = ButtonStateHover
		}
		if disabledRow(int32(i)) {
			state = ButtonStateDisabled
		}
		paint := r.dropdownSurface(optionPaint.HighlightBounds, Dropdown_DropdownOptionRole(), selectedRow, state, styleClass)
		paint.ID, paint.Row, paint.Selected, paint.Focused = id, int32(i), selectedRow, highlighted
		if selectedRow || highlighted {
			r.record(paint)
		}
		hasIcon := i < len(items) && items[i].IconType != IconNone
		rowContent := Dropdown_DropdownOptionContentFor(row, visibleRow,
			contentMetrics, menuMetrics, hasIcon)
		if selected != nil && Dropdown_CanCommit(!disabledRow(int32(i)), pressed, false, false,
			r.consumeTap(row), scrollbar, gesture.Dragging, *offset == gesture.OriginOffset) {
			next := int32(i)
			if *selected != next {
				*selected = next
				changed = true
			}
			openDecision := Dropdown_DropdownOpenAfterCommit(open, true)
			open = openDecision.Open
			if openDecision.Closed {
				r.closeDropdown(id)
			}
			selectedRow = true
		}
		paint.TextColor = unpackRGBA(Surface_Opacity(packRGBA(paint.TextColor), paint.Opacity))
		if i < len(items) {
			item := items[i]
			if item.SeparatorBefore {
				r.record(FrameOp{Kind: FrameOpLine, Bounds: rowContent.SeparatorBounds, Color: r.Fade(paint.TextColor, 0.18), ID: id})
			}
			if item.IconType != IconNone {
				r.record(FrameOp{Kind: FrameOpIcon, Bounds: rowContent.IconBounds, IconType: item.IconType, Color: paint.TextColor, ID: id})
			}
		}
		r.record(FrameOp{Kind: FrameOpText, Clip: rowContent.ClipBounds, HasClip: true, Bounds: rowContent.TextBounds, Text: label, Color: paint.TextColor, FontSize: int32(contentMetrics.Font), ID: id, Row: int32(i), Selected: selectedRow})
		if selectedRow {
			r.record(FrameOp{Kind: FrameOpIcon, ID: id, Color: paint.TextColor, IconType: IconCheck,
				Bounds: rowContent.CheckBounds, Row: int32(i), Selected: true})
		}
	}
	return changed
}
func (r *runtime) Column(props ColumnProps) {
	r.pushLayout(props, false, FrameOpColumn)
}
func (r *runtime) Row(props ColumnProps) {
	r.pushLayout(props, true, FrameOpRow)
}
func (r *runtime) Group(props ColumnProps) {
	r.pushGroup(props, FrameOpGroup)
}
func (r *runtime) Stack(props ColumnProps) {
	r.pushLayout(props, false, FrameOpStack)
}
func (r *runtime) Screen(props ColumnProps) {
	r.pushGroup(props, FrameOpScreen)
}
func (r *runtime) Grid(props GridProps) {
	r.pushGrid(props)
}
func (r *runtime) SetPageTitle(title string) {
	r.pageTitle = title
}
func (r *runtime) SetPageDescription(description string) {
	r.pageDescription = description
}
func (r *runtime) SetPageCanonicalURL(url string) {
	r.pageCanonicalURL = url
}
func (r *runtime) SetPageThemeColor(color Color) {
	r.pageThemeColor = color
}
func (r *runtime) GetRoutePath() string {
	if r.routePath == "" {
		return "/"
	}
	return r.routePath
}
func (r *runtime) GetRouteHash() string {
	return r.routeHash
}
func (r *runtime) GetRouteVersion() int32 {
	return r.routeVersion
}
func (r *runtime) PushRoute(path string) {
	r.setRoute(path)
}
func (r *runtime) ReplaceRoute(path string) {
	r.setRoute(path)
}
func styleLength(value float32) int32 {
	if value <= 0 {
		return 0
	}
	return int32(value + 0.5)
}
func styleForClassKind(className int32, kind int32) Style {
	facts := StyleSheet_StyleDefaultFacts(kind)
	facts.ClassName = className
	facts.State = int32(ButtonStateNormal)
	return unpackStyle(ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleGap | StylePaddingX | StylePaddingY)}),
		facts, int32(ButtonStateNormal)))
}
func (r *runtime) Page(props PageProps) {
	bounds := Layout_LayoutScopeBounds(props.Bounds, r.GetScreenWidth(), r.GetScreenHeight())
	key := props.Key
	if key == 0 {
		key = Key(props.Title)
	}
	style := styleForClassKind(props.ClassName, StyleSheet_StyleKindPage())
	if props.Title != "" {
		r.SetPageTitle(props.Title)
	}
	if props.Description != "" {
		r.SetPageDescription(props.Description)
	}
	if props.CanonicalURL != "" {
		r.SetPageCanonicalURL(props.CanonicalURL)
	}
	if style.Background.A != 0 {
		r.SetPageThemeColor(style.Background)
	}
	r.record(FrameOp{Kind: FrameOpPage, Bounds: bounds, Text: props.Title, Semantic: SemanticPage})
	r.Column(ColumnProps{Bounds: bounds, Gap: styleLength(style.Gap), Padding: styleLength(style.PaddingX), Key: key})
}
func (r *runtime) Section(props SectionProps) {
	bounds := Layout_LayoutScopeBounds(props.Bounds, r.GetScreenWidth(), r.GetScreenHeight())
	key := props.Key
	if key == 0 {
		key = Key(props.Label)
	}
	r.record(FrameOp{Kind: FrameOpSection, Bounds: bounds, Text: props.Label, Semantic: SemanticSection})
	style := styleForClassKind(props.ClassName, StyleSheet_StyleKindSection())
	r.Column(ColumnProps{Bounds: bounds, Gap: styleLength(style.Gap), Padding: styleLength(style.PaddingX), Key: key})
}
func (r *runtime) Heading(props HeadingProps) {
	level := props.Level
	if level < 1 {
		level = 1
	} else if level > 6 {
		level = 6
	}
	style := defaultTextStyleForClassKind(Text24, props.ClassName, StyleSheet_StyleKindHeading())
	font, fontID := styleTextFace(style, Text24)
	color := style.Foreground
	bounds := props.Bounds
	if bounds.Width <= 0 {
		bounds.Width = float32(runtimeTextWidthWithFont(props.Text, font, fontID))
	}
	if bounds.Height <= 0 {
		bounds.Height = float32(font)
	}
	bounds = r.layoutRect(bounds)
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: props.Text, Color: color, Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: int32(props.Key), Semantic: SemanticHeading, Level: level})
}
func (r *runtime) ParagraphText(props ParagraphTextProps) {
	style := defaultTextStyleForClassKind(Text16, props.ClassName, StyleSheet_StyleKindParagraphText())
	font, fontID := styleTextFace(style, Text16)
	color := style.Foreground
	lineGap := int32(4)
	if style.Gap > 0 {
		lineGap = int32(style.Gap + 0.5)
	}
	width := int32(props.Bounds.Width)
	if width <= 0 {
		width = r.GetScreenWidth() - int32(props.Bounds.X)
	}
	bounds := r.layoutRect(Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: float32(width), Height: float32(font + lineGap)})
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: props.Text, Color: color, Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: int32(props.Key), Semantic: SemanticParagraph})
}
func (r *runtime) Link(props LinkProps) bool {
	state := ButtonStateNormal
	if props.Disabled {
		state = ButtonStateDisabled
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindLink(), StyleSheet_StyleAny())
	linkStyle := unpackStyle(frame.Value)
	font, fontID := styleTextFace(linkStyle, Text16)
	bounds := r.layoutRect(props.Bounds)
	bounds = Link_LinkBoundsFor(bounds,
		int32(runtimeTextWidthWithFont(props.Text, font, fontID)),
		textHeight(font, fontID), font)
	pressed := false
	if !props.Disabled {
		pressed = r.consumeTap(bounds)
	}
	appearance := Link_ResolveLinkAppearance(frame, false, props.Disabled)
	color := unpackRGBA(appearance.Color)
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: props.Text, Color: color, Opacity: linkStyle.Opacity, FontSize: font, FontID: fontID, FocusID: props.FocusID, Disabled: props.Disabled, Pressed: pressed, Semantic: SemanticLink, Link: props.Link, Role: "link"})
	return pressed
}
func (r *runtime) Flow(props FlowProps) {
	r.Row(ColumnProps(props))
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
func (r *runtime) GetThemeBorder() Color      { return r.theme().border }
func (r *runtime) GetThemeButton() Color      { return r.theme().button }
func (r *runtime) GetThemeButtonHover() Color { return r.theme().buttonHover }
func (r *runtime) GetThemeLink() Color        { return r.theme().link }

// The primary trio mirrors the Default mapping in theme_runtime.go: the
// palette's circle color is Primary, its contrast color OnPrimary, and a
// background tone serves as SurfaceVariant.
func (r *runtime) GetThemePrimary() Color   { return r.theme().circle }
func (r *runtime) GetThemeOnPrimary() Color { return materialOnColor(r.theme().circle) }
func (r *runtime) GetThemeSurfaceVariant() Color {
	return materialTone(r.theme().background, 10, 18, r.effectiveDark())
}
func (r *runtime) GetThemeScheme() DefaultScheme {
	return materialScheme(r.theme(), r.effectiveDark())
}
func wrapRuntimeTextMeasured(text string, width float32, measure func(string) int) []string {
	if width <= 0 || text == "" {
		return []string{text}
	}
	var lines []string
	for _, paragraph := range strings.Split(text, "\n") {
		words := strings.Fields(paragraph)
		if len(words) == 0 {
			lines = append(lines, "")
			continue
		}
		line := words[0]
		for _, word := range words[1:] {
			candidate := line + " " + word
			if float32(measure(candidate)) <= width {
				line = candidate
			} else {
				lines = append(lines, line)
				line = word
			}
		}
		lines = append(lines, line)
	}
	return lines
}
func (r *runtime) Bevel(x, y, w, h int32, light, dark Color) {
	lines := Bevel_BevelLinesFor(x, y, w, h)
	r.record(FrameOp{Kind: FrameOpLine, Bounds: lines.Top, Color: light})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: lines.Left, Color: light})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: lines.Bottom, Color: dark})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: lines.Right, Color: dark})
}
func (r *runtime) Icon(id, x, y, size int32, iconType int32, tint Color) {
	layout := Icon_IconLayoutFor(x, y, size)
	if !layout.Drawable {
		return
	}
	r.record(FrameOp{
		Kind:     FrameOpIcon,
		Bounds:   layout.Bounds,
		Color:    tint,
		ID:       id,
		IconType: iconType,
		IconSize: float32(layout.Size),
	})
}
func (r *runtime) Image(props ImageProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, props.ClassName, StyleSheet_StyleKindImage(), StyleSheet_StyleAny())
	style := unpackStyle(frame.Value)
	if style.Fields&uint32(StyleBackground|StyleBorder|StyleRadius|StyleBorderWidth|StyleMaterial|StyleOpacity) != 0 {
		r.record(styleFrameRectOp(props.Bounds, Rectangle{}, StyleFrame{
			Value: packStyle(style),
			Fill:  styleFill(style),
		}))
	}
	tintStyle := unpackStyle(ResolveActiveStyle(StyleData{Fields: uint32(StyleOpacity), Opacity: 1},
		StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindImage()),
		int32(ButtonStateNormal)))
	if props.ClassName != 0 {
		facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindImage())
		facts.ClassName = props.ClassName
		tintStyle = unpackStyle(ResolveActiveStyle(StyleData{Fields: uint32(StyleOpacity), Opacity: 1},
			facts, int32(ButtonStateNormal)))
	}
	tint := White
	if tintStyle.Fields&uint32(StyleForeground) != 0 {
		tint = tintStyle.Foreground
	}
	if tintStyle.Opacity < 1 {
		tint = unpackRGBA(Surface_Opacity(packRGBA(tint), tintStyle.Opacity))
	}
	op := FrameOp{Kind: FrameOpImage, Bounds: props.Bounds, Text: props.AssetPath, Color: tint}
	if props.AltText != "" {
		op.Semantic = SemanticImage
		op.Role = "img"
		op.AltText = props.AltText
	}
	r.record(op)
}
func (r *runtime) Paragraph(spec ParagraphSpec, x int32, y *int32) {
	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindParagraphText())
	facts.ClassName = spec.ClassName
	style := unpackStyle(ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleOpacity), Opacity: 1}),
		facts,
		int32(ButtonStateNormal)))
	textStyle := defaultTextStyle(Text16)
	textFont, textFontID := styleTextFace(textStyle, Text16)
	font, fontID := styleTextFaceWithFallback(style, textFont, textFontID)
	if spec.Font > 0 {
		font = spec.Font
	}
	color := textStyle.Foreground
	if style.Fields&StyleForeground != 0 {
		color = style.Foreground
	}
	if style.Opacity < 1 {
		color = unpackRGBA(Surface_Opacity(packRGBA(color), style.Opacity))
	}
	textY := int32(0)
	if y != nil {
		textY = *y
	}
	fallbackWidth := int32(r.config.Width) - x
	metrics := Paragraph_ParagraphResolveMetrics(font, Text16,
		spec.LineGap, 4, spec.IconSize, spec.Width, fallbackWidth, 0, textY)
	if !Paragraph_ParagraphCanLayout(metrics.Width) {
		return
	}
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(textY),
		Width: float32(metrics.Width), Height: float32(metrics.Height)})
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: spec.Text, Color: color, FontSize: metrics.Font, FontID: fontID})
	if y != nil {
		*y = metrics.NextY
	}
}

type iconActionProps struct {
	Bounds      Rectangle
	Icon        Texture2D
	IconType    int32
	IconSize    int32
	IconPadding int32
	FocusID     int32
	Disabled    bool
	StyleKind   int32
	Role        int32
	ClassName   int32
}

func (r *runtime) iconAction(props iconActionProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	padding := props.IconPadding
	if padding <= 0 {
		padding = 4
	}
	size := props.IconSize
	if size <= 0 {
		availableW := int32(props.Bounds.Width) - padding*2
		availableH := int32(props.Bounds.Height) - padding*2
		size = availableW
		if availableH < size {
			size = availableH
		}
		if size < 1 {
			size = 1
		}
	}
	styleKind := props.StyleKind
	if styleKind == 0 {
		styleKind = StyleSheet_StyleKindButton()
	}
	role := props.Role
	if role == 0 {
		role = StyleSheet_StyleAny()
	}
	frame, pressed := r.surfaceButtonFrameForRoleKind(ButtonProps{
		Bounds:    props.Bounds,
		ID:        props.FocusID,
		Disabled:  props.Disabled,
		Tone:      ButtonToneNeutral,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
		ClassName: props.ClassName,
	}, Rectangle{}, false, styleKind, role)
	r.record(frame)
	iconX := int32(props.Bounds.X) + (int32(props.Bounds.Width)-size)/2
	iconY := int32(props.Bounds.Y) + (int32(props.Bounds.Height)-size)/2
	iconType := props.IconType
	if iconType == 0 && props.Icon.ID != 0 {
		iconType = int32(props.Icon.ID)
	}
	r.Icon(props.FocusID, iconX, iconY, size, iconType, unpackRGBA(frame.Button.Foreground))
	return pressed
}
func (r *runtime) Toggle(props ToggleProps) bool {
	if props.Value == nil {
		return false
	}
	bounds := props.Bounds
	hasLabels := props.OffLabel != "" || props.OnLabel != ""
	disabled := props.Disabled || r.contentDisabled()
	labelStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		disabled, false, props.ClassName,
		StyleSheet_StyleKindToggle(), Toggle_ToggleLabelRole()).Value)
	labelFont, labelFontID := styleTextFace(labelStyle, Text16)
	offWidth := int32(runtimeTextWidthWithFont(props.OffLabel, labelFont, labelFontID))
	onWidth := int32(runtimeTextWidthWithFont(props.OnLabel, labelFont, labelFontID))
	checkedForMetrics := *props.Value != 0
	trackToneForMetrics := ButtonToneNeutral
	trackRoleForMetrics := Toggle_ToggleTrackRoleFor(checkedForMetrics, hasLabels)
	if checkedForMetrics && !hasLabels {
		trackToneForMetrics = ButtonToneAccent
	}
	trackFrameForMetrics := simpleStyleFrameWithClassRole(trackToneForMetrics, ButtonStateNormal,
		disabled, checkedForMetrics, props.ClassName, StyleSheet_StyleKindToggle(),
		trackRoleForMetrics)
	activeFrameForMetrics := simpleStyleFrameWithClassRole(ButtonToneAccent, ButtonStateNormal,
		disabled, checkedForMetrics, props.ClassName, StyleSheet_StyleKindToggle(), Toggle_ToggleFillRole())
	labelFrameForMetrics := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		disabled, false, props.ClassName, StyleSheet_StyleKindToggle(), Toggle_ToggleLabelRole())
	thumbFrameForMetrics := simpleStyleFrameWithClassRole(trackToneForMetrics, ButtonStateNormal,
		disabled, checkedForMetrics, props.ClassName, StyleSheet_StyleKindToggleThumb(),
		StyleSheet_StyleAny())
	if minW := float32(Toggle_ToggleMinimumWidthForStyle(hasLabels, offWidth, onWidth,
		1, trackFrameForMetrics, activeFrameForMetrics, labelFrameForMetrics)); bounds.Width < minW {
		bounds.Width = minW
	}
	if minH := float32(Toggle_ToggleMinimumHeightForStyle(1, trackFrameForMetrics,
		thumbFrameForMetrics)); bounds.Height < minH {
		bounds.Height = minH
	}
	bounds = r.layoutRect(bounds)
	input := r.ReadActivation(bounds, props.ID, !disabled)
	toggle := Toggle_ToggleValueFor(*props.Value != 0, input.Activated,
		!disabled, props.Value != nil)
	if toggle.Changed {
		if toggle.Value {
			*props.Value = 1
		} else {
			*props.Value = 0
		}
	}
	checked := *props.Value != 0
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	} else if input.Pressed {
		state = ButtonStatePressed
	} else if input.Focused {
		state = ButtonStateFocus
	} else if input.Hovered {
		state = ButtonStateHover
	}
	trackTone := ButtonToneNeutral
	trackRole := Toggle_ToggleTrackRoleFor(checked, hasLabels)
	if checked && !hasLabels {
		trackTone = ButtonToneAccent
	}
	labelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled,
		false, props.ClassName, StyleSheet_StyleKindToggle(), Toggle_ToggleLabelRole())
	paint := Toggle_TogglePaintFor(ToggleSpec{
		Bounds:    bounds,
		Checked:   checked,
		Enabled:   !disabled,
		Hovered:   input.Hovered,
		Pressed:   input.Pressed,
		Focused:   input.Focused,
		HasLabels: hasLabels,
		OffWidth:  offWidth,
		OnWidth:   onWidth,
		Font:      labelFont,
		Scale:     1,
		Track: simpleStyleFrameWithClassRole(trackTone, state, disabled, checked,
			props.ClassName, StyleSheet_StyleKindToggle(), trackRole),
		Active: simpleStyleFrameWithClassRole(ButtonToneAccent, state, disabled,
			checked, props.ClassName, StyleSheet_StyleKindToggle(), Toggle_ToggleFillRole()),
		Label: labelFrame,
		Thumb: simpleStyleFrameWithClassRole(trackTone, state, disabled, checked,
			props.ClassName, StyleSheet_StyleKindToggleThumb(), StyleSheet_StyleAny()),
	})
	if paint.HasLabels {
		labelStyle = unpackStyle(labelFrame.Value)
		labelFont, labelFontID = styleTextFace(labelStyle, Text16)
		labelColor := packRGBA(labelStyle.Foreground)
		if checked {
			paint.OffLabelColor = labelColor
		} else {
			paint.OnLabelColor = labelColor
		}
	}
	trackOp := styleFrameRectOp(paint.TrackBounds, Rectangle{}, paint.Track)
	trackOp.ID = props.ID
	trackOp.Focused = input.Focused
	trackOp.Hovered = input.Hovered
	trackOp.Pressed = input.Pressed
	trackOp.Selected = checked
	trackOp.Disabled = disabled
	if input.Focused {
		trackOp.BorderColor = trackOp.FocusColor
	}
	r.record(trackOp)
	if paint.HasLabels {
		activeOp := styleFrameRectOp(paint.ActiveBounds, paint.TrackBounds, paint.Active)
		activeOp.ID = props.ID
		activeOp.Pressed = input.Pressed
		activeOp.Selected = true
		activeOp.Disabled = disabled
		r.record(activeOp)
		r.record(FrameOp{Kind: FrameOpText, Bounds: paint.OffLabelBounds, Text: props.OffLabel, Color: unpackRGBA(paint.OffLabelColor), Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, ID: props.ID, Disabled: disabled})
		r.record(FrameOp{Kind: FrameOpText, Bounds: paint.OnLabelBounds, Text: props.OnLabel, Color: unpackRGBA(paint.OnLabelColor), Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, ID: props.ID, Disabled: disabled})
	} else {
		if input.Hovered && !disabled {
			r.record(FrameOp{Kind: FrameOpCircle, Bounds: circleBounds(paint.ThumbX, paint.ThumbY, paint.ThumbRadius+5), Color: unpackRGBA(paint.ThumbGlowColor), ID: props.ID, Hovered: true})
		}
		r.record(FrameOp{Kind: FrameOpCircle, Bounds: circleBounds(paint.ThumbX, paint.ThumbY+2, paint.ThumbRadius+1), Color: unpackRGBA(paint.ThumbShadowColor), ID: props.ID, Disabled: disabled})
		r.record(FrameOp{Kind: FrameOpCircle, Bounds: circleBounds(paint.ThumbX, paint.ThumbY, paint.ThumbRadius), Color: unpackRGBA(paint.ThumbFillColor), ID: props.ID, Selected: checked, Disabled: disabled})
		r.record(FrameOp{Kind: FrameOpCircle, Bounds: circleBounds(paint.ThumbX-3, paint.ThumbY-4, paint.ThumbRadius*0.45), Color: unpackRGBA(paint.ThumbHighlightColor), ID: props.ID, Disabled: disabled})
		edgeRadius := paint.ThumbRadius - 1
		if edgeRadius < 0 {
			edgeRadius = 0
		}
		r.record(FrameOp{Kind: FrameOpRing, Bounds: circleBounds(paint.ThumbX, paint.ThumbY, paint.ThumbRadius), Radius: edgeRadius, Color: unpackRGBA(paint.ThumbEdgeColor), ID: props.ID, Selected: checked, Disabled: disabled})
	}
	return input.Activated
}

func circleBounds(x, y, radius float32) Rectangle {
	return Rectangle{X: x - radius, Y: y - radius, Width: radius * 2, Height: radius * 2}
}
func (r *runtime) Modal(props ModalProps) int32 {
	count := int(props.ActionCount)
	if count <= 0 || count > len(props.Actions) {
		count = len(props.Actions)
	}
	actions := make([]ModalAction, 0, count)
	for i := 0; i < count; i++ {
		actions = append(actions, props.Actions[i])
	}
	fieldHeight := float32(0)
	if props.Text != nil && props.CursorPosition != nil && props.Focused != nil {
		fieldHeight = 38
	}
	result, field := r.drawActionModal(props.Title, props.Message, actions, fieldHeight, props.ClassName)
	commit := false
	if fieldHeight > 0 {
		focusID := props.FocusID
		if focusID == 0 {
			focusID = 7301
		}
		maxCodepoints := int32(len(props.Text) - 1)
		if props.TextSize > 0 && props.TextSize <= int32(len(props.Text)) {
			maxCodepoints = props.TextSize - 1
		}
		r.editText(field, props.Text, props.CursorPosition, props.Focused, &commit, focusID, textEditOptions{maxCodepoints: maxCodepoints})
		focused := r.focusID == focusID || props.Focused != nil && *props.Focused
		font := r.textInputDefaultFont(FrameOpTextField, focused, r.contentDisabled(), props.ClassName, Text16)
		r.recordTextInput(FrameOpTextField, field, props.Text, props.CursorPosition, props.Focused, focusID, font, false, false,
			textInputRecordOptions{className: props.ClassName})
	}
	if result == 0 && commit {
		if count > 1 {
			result = 2
		} else {
			result = 1
		}
	}
	return result
}

func defaultStyleFrame(styleKind int32) StyleFrame {
	value := ResolveActiveStyle(minimalControlStyleData(), StyleSheet_StyleDefaultFacts(styleKind), int32(ButtonStateNormal))
	return StyleFrame{Value: value, Fill: Surface_FillState(value.Fields, value.Background, value.BackgroundEnd)}
}

func styleMetricFrame(className int32, styleKind int32, role int32) StyleFrame {
	facts := StyleSheet_StyleDefaultFacts(styleKind)
	facts.ClassName = className
	facts.Role = role
	facts.State = int32(ButtonStateNormal)
	value := ResolveActiveStyle(StyleData{}, facts, int32(ButtonStateNormal))
	return StyleFrame{Value: value, Fill: Surface_FillState(value.Fields, value.Background, value.BackgroundEnd)}
}

func defaultTextStyle(font int32) Style {
	value := ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleFontSize | StyleOpacity), FontSize: float32(font), Opacity: 1}),
		StyleSheet_StyleTextFacts(0, 0, StyleSheet_StyleKindText(), int32(ButtonStateNormal)),
		int32(ButtonStateNormal))
	return unpackStyle(value)
}

func defaultTextStyleForKind(font int32, kind int32) Style {
	value := ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleFontSize | StyleOpacity), FontSize: float32(font), Opacity: 1}),
		StyleSheet_StyleDefaultFacts(kind),
		int32(ButtonStateNormal))
	return unpackStyle(value)
}

func defaultTextStyleForClassKind(font int32, className int32, kind int32) Style {
	facts := StyleSheet_StyleDefaultFacts(kind)
	facts.ClassName = className
	facts.State = int32(ButtonStateNormal)
	value := ResolveActiveStyle(packStyle(Style{Fields: uint32(StyleFontSize | StyleOpacity), FontSize: float32(font), Opacity: 1}),
		facts,
		int32(ButtonStateNormal))
	return unpackStyle(value)
}

func styleFont(style Style, fallback int32) int32 {
	font := int32(style.FontSize)
	if font > 0 {
		return font
	}
	return fallback
}

func styleFontID(style Style) uint32 {
	if style.Fields&StyleTypeface == 0 {
		return 0
	}
	return registeredTypeface(style.Typeface)
}

func styleTextFace(style Style, fallback int32) (int32, uint32) {
	return styleFont(style, fallback), styleFontID(style)
}

func styleTextFaceWithFallback(style Style, fallbackFont int32, fallbackID uint32) (int32, uint32) {
	font := styleFont(style, fallbackFont)
	fontID := fallbackID
	if style.Fields&StyleTypeface != 0 {
		fontID = styleFontID(style)
	}
	return font, fontID
}

func modalActionLabel(action ModalAction, index, count int) string {
	if action.Label != "" {
		return action.Label
	}
	if count == 1 {
		return "OK"
	}
	if index == 0 {
		return "Cancel"
	}
	return "OK"
}

func (r *runtime) drawActionModal(title, message string, actions []ModalAction, fieldHeight float32, className int32) (int32, Rectangle) {
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalPanelRole())
	titleFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalTitleRole())
	messageFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalMessageRole())
	actionFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalActionRole())
	closeFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalCloseRole())
	metrics := Modal_ModalMetricsFor(1, panelFrame, titleFrame, messageFrame, actionFrame, closeFrame)
	messageHeight := int32(0)
	if message != "" {
		messageHeight = 24
	}
	buttonRows := int32(0)
	if len(actions) > 0 {
		buttonRows = 1
	}
	layout := Modal_ModalLayoutFor(r.GetScreenWidth(), r.GetScreenHeight(), 0, messageHeight, buttonRows, fieldHeight > 0, metrics)
	panel := layout.Panel
	panelStyle := unpackStyle(panelFrame.Value)
	titleStyle := unpackStyle(titleFrame.Value)
	messageStyle := unpackStyle(messageFrame.Value)
	scrimStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		className, StyleSheet_StyleKindModal(), Modal_ModalScrimRole()).Value)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{Width: float32(r.GetScreenWidth()), Height: float32(r.GetScreenHeight())}, Color: unpackRGBA(Surface_Opacity(packRGBA(scrimStyle.Background), scrimStyle.Opacity)), Opacity: scrimStyle.Opacity})
	r.record(styleFrameRectOp(panel, Rectangle{}, panelFrame))
	titleFont, titleFontID := styleTextFace(titleStyle, Text16)
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: panel.X + float32(metrics.PaddingX), Y: panel.Y + float32(metrics.FrameTitleY), Width: float32(layout.ContentWidth), Height: 30}, Text: title, Color: titleStyle.Foreground, Opacity: titleStyle.Opacity, FontSize: titleFont, FontID: titleFontID})
	if message != "" {
		messageFont, messageFontID := styleTextFace(messageStyle, Text16)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(layout.MessageX), Y: float32(layout.MessageY), Width: float32(layout.ContentWidth), Height: float32(messageHeight)}, Text: message, Color: messageStyle.Foreground, Opacity: messageStyle.Opacity, FontSize: messageFont, FontID: messageFontID})
	}

	result := int32(0)
	buttonW := float32(metrics.ActionMinWidth)
	gap := float32(metrics.ButtonGap)
	buttonY := float32(layout.ButtonY)
	buttonX := panel.X + panel.Width - float32(metrics.PaddingX) - float32(len(actions))*buttonW - float32(maxInt(0, len(actions)-1))*gap
	for i, action := range actions {
		label := modalActionLabel(action, i, len(actions))
		bounds := Rectangle{X: buttonX + float32(i)*(buttonW+gap), Y: buttonY, Width: buttonW, Height: float32(metrics.ButtonHeight)}
		tone := action.Tone
		emphasis := action.Emphasis
		if emphasis == 0 {
			emphasis = ButtonEmphasisSoft
		}
		if tone == 0 && i == len(actions)-1 {
			tone = ButtonToneAccent
		}
		button, pressed := r.surfaceButtonFrameForRoleKind(ButtonProps{Bounds: bounds, Label: label,
			ClassName: className, Tone: tone, Emphasis: emphasis, Disabled: action.Disabled},
			panel, false, StyleSheet_StyleKindModal(), Modal_ModalActionRole())
		button.AmbientColor = panelStyle.Background
		r.record(button)
		if pressed {
			result = int32(i + 1)
		}
	}
	if result == 0 {
		for i := range r.taps {
			if !r.taps[i].consumed && !pointInRect(r.taps[i].x, r.taps[i].y, panel) {
				r.taps[i].consumed = true
				result = -1
				break
			}
		}
	}
	field := Rectangle{X: float32(layout.MessageX), Y: float32(layout.PromptY), Width: float32(layout.ContentWidth), Height: float32(layout.PromptHeight)}
	return result, field
}
func (r *runtime) TitleBar(props TitleBarProps) int32 {
	height := props.Height
	if height <= 0 {
		height = 44
	}
	surfaceFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindTitleBar(), TitleBar_TitleBarBarRole())
	titleFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindTitleBar(), TitleBar_TitleBarTitleRole())
	actionFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindTitleBar(), TitleBar_TitleBarActionRole())
	metrics := TitleBar_TitleBarMetricsFor(1, surfaceFrame, titleFrame, actionFrame)
	layout := TitleBar_TitleBarLayoutFor(r.GetScreenWidth(), height,
		props.HasLeadingAction, props.HasDropdown, props.Dropdown.Height,
		props.Dropdown.MinWidth, metrics)
	r.record(styleFrameRectOp(layout.Bounds, Rectangle{}, surfaceFrame))
	clicked := int32(0)
	if props.HasLeadingAction {
		button, pressed := r.surfaceButtonFrameForRoleKind(ButtonProps{
			Bounds:    layout.LeadingBounds,
			Disabled:  r.contentDisabled(),
			ClassName: props.ClassName,
			Tone:      ButtonToneNeutral,
			Emphasis:  ButtonEmphasisSoft,
			Size:      ControlSizeMedium,
		}, layout.Bounds, false, StyleSheet_StyleKindTitleBar(), TitleBar_TitleBarActionRole())
		if pressed {
			clicked = 1
		}
		button.IconSize = float32(metrics.LeadingIconSize)
		r.record(button)
	}
	if props.HasDropdown {
		if !props.Dropdown.Disabled {
			selected := props.Dropdown.SelectedIndex
			count := props.Dropdown.OptionCount
			if count <= 0 || count > int32(len(props.Dropdown.Options)) {
				count = int32(len(props.Dropdown.Options))
			}
			changed := int32(0)
			if r.Dropdown(DropdownProps{
				ID:            props.Dropdown.ID,
				Bounds:        layout.DropdownBounds,
				Options:       props.Dropdown.Options[:count],
				OptionCount:   count,
				SelectedIndex: selected,
			}) {
				changed = 1
			}
			return clicked | changed
		}
		return clicked
	}
	titleStyle := unpackStyle(titleFrame.Value)
	titleFont, titleFontID := styleTextFace(titleStyle, Text20)
	titleW := runtimeTextWidthWithFont(props.Title, titleFont, titleFontID)
	titlePaint := TitleBar_TitleBarTitlePaintFor(layout, int32(titleW), titleFont)
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{
		X: float32(titlePaint.X), Y: float32(titlePaint.Y),
		Width: float32(titleW), Height: float32(titleFont + 4),
	}, Text: props.Title, Color: titleStyle.Foreground, Opacity: titleStyle.Opacity, FontSize: titleFont, FontID: titleFontID})
	return clicked
}
func (r *runtime) NavigationBar(props NavigationBarProps) {
	count := int(props.Count)
	if count <= 0 || count > len(props.Items) {
		count = len(props.Items)
	}
	if count == 0 {
		return
	}
	w := props.ViewWidth
	if w <= 0 {
		w = r.GetScreenWidth()
	}
	viewH := props.ViewHeight
	if viewH <= 0 {
		viewH = r.GetScreenHeight()
	}
	itemBaseFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		NavigationBar_NavigationBarItemFactsFor(props.ClassName,
			int32(ButtonToneNeutral), int32(ButtonEmphasisSoft),
			int32(ButtonStateNormal)), int32(ButtonStateNormal))}
	barFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		NavigationBar_NavigationBarFactsFor(props.ClassName,
			int32(ButtonStateNormal)), int32(ButtonStateNormal))}
	paint := NavigationBar_NavigationBarPaintFor(NavigationBarSpec{
		ViewWidth:    w,
		ViewHeight:   viewH,
		Count:        int32(count),
		Height:       props.Height,
		SideMargin:   -1,
		BottomMargin: -1,
		IconSize:     -1,
		Scale:        1,
		Bar:          barFrame,
		Item:         itemBaseFrame,
	})
	bar := unpackStyle(paint.Bar.Value)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.BarBounds, Color: bar.Background,
		BorderColor: bar.Border, BorderWidth: bar.BorderWidth, Radius: bar.Radius,
		Material: bar.Material})
	for i := 0; i < count; i++ {
		item := props.Items[i]
		itemState := checkboxButtonState(false, false, false, item.Disabled)
		if item.Active && !item.Disabled {
			itemState = ButtonStateSelected
		}
		baseFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
			NavigationBar_NavigationBarItemFactsFor(props.ClassName,
				int32(ButtonToneNeutral), int32(ButtonEmphasisSoft),
				int32(checkboxButtonState(false, false, false, item.Disabled))),
			int32(checkboxButtonState(false, false, false, item.Disabled)))}
		baseStyle := unpackStyle(baseFrame.Value)
		labelFont, labelFontID := styleTextFace(baseStyle, Text14)
		faceTone := ButtonToneNeutral
		faceEmphasis := ButtonEmphasisSoft
		if item.Active {
			faceTone = ButtonToneAccent
			faceEmphasis = ButtonEmphasisFilled
		}
		faceFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
			NavigationBar_NavigationBarItemFactsFor(props.ClassName,
				int32(faceTone), int32(faceEmphasis), int32(itemState)),
			int32(itemState))}
		itemPaint := NavigationBar_NavigationBarItemPaintFor(NavigationBarItemSpec{
			Bar:         paint,
			Index:       int32(i),
			Active:      item.Active,
			Disabled:    item.Disabled,
			Hovered:     false,
			LabelHeight: labelFont + 4,
			Base:        baseFrame,
			Face:        faceFrame,
		})
		pressed := !item.Disabled && r.consumeTap(itemPaint.Bounds)
		textStyle := baseStyle
		if itemPaint.DrawFace {
			face := unpackStyle(itemPaint.Face.Value)
			textStyle = face
			r.record(FrameOp{Kind: FrameOpRect, Bounds: itemPaint.StateBounds, Color: face.Background,
				BorderColor: face.Border, BorderWidth: face.BorderWidth, Radius: face.Radius,
				Material: face.Material, Selected: item.Active, Disabled: item.Disabled})
		}
		textFont, textFontID := styleTextFaceWithFallback(textStyle, labelFont, labelFontID)
		if item.Icon.ID != 0 {
			tint := unpackRGBA(itemPaint.IconColor)
			if item.Disabled {
				tint.A = uint8(uint32(tint.A) * uint32(itemPaint.IconAlpha) / 255)
			}
			r.Icon(item.Route, int32(itemPaint.IconBounds.X), int32(itemPaint.IconBounds.Y),
				int32(itemPaint.IconBounds.Width), int32(item.Icon.ID), tint)
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: itemPaint.LabelBounds, Text: item.Label,
			Color: unpackRGBA(itemPaint.TextColor), Opacity: textStyle.Opacity,
			FontSize: textFont, FontID: textFontID, ID: item.Route,
			Pressed: pressed, Selected: item.Active, Disabled: item.Disabled})
	}
}
func (r *runtime) Toolbar(props ToolbarProps) ToolbarResult {
	result := ToolbarResult{SelectedMenuItem: -1, ClickedAction: -1}
	if props.Width <= 0 {
		props.Width = r.GetScreenWidth() - props.X
	}
	if props.Height <= 0 {
		props.Height = 44
	}
	actionCount := int32(props.ActionCount)
	if actionCount <= 0 || int(actionCount) > len(props.Actions) {
		actionCount = int32(len(props.Actions))
	}
	barFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindToolbar(), Toolbar_ToolbarBarRole())
	actionFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindToolbar(), Toolbar_ToolbarActionRole())
	layout := Toolbar_ToolbarLayoutFor(ToolbarSpec{
		X:                 props.X,
		Y:                 props.Y,
		Width:             props.Width,
		Height:            props.Height,
		ActionCount:       actionCount,
		ActionIconSize:    -1,
		ActionIconPadding: -1,
		ActionGap:         -1,
		SidePadding:       -1,
		DropdownMinWidth:  props.DropdownMinWidth,
		DropdownMaxWidth:  props.DropdownMaxWidth,
		DropdownHeight:    props.DropdownHeight,
		Scale:             1,
		Bar:               barFrame,
		Action:            actionFrame,
	})
	bounds := layout.Bounds
	r.record(styleFrameRectOp(bounds, Rectangle{}, barFrame))
	dividerStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
		props.ClassName, StyleSheet_StyleKindToolbar(), Toolbar_ToolbarDividerRole()).Value)
	r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height - 1, Width: bounds.Width, Height: 0}, Color: dividerStyle.Border})
	for i := int32(0); i < actionCount; i++ {
		action := props.Actions[i]
		if r.iconAction(iconActionProps{
			Bounds:      Toolbar_ToolbarActionBoundsFor(layout, i, actionCount),
			Icon:        action.Icon,
			IconType:    action.IconType,
			IconSize:    layout.ActionIconSize,
			IconPadding: layout.ActionIconPadding,
			FocusID:     Toolbar_ToolbarActionIdFor(props.ID, int32(i)),
			Disabled:    action.Disabled,
			StyleKind:   StyleSheet_StyleKindToolbar(),
			Role:        Toolbar_ToolbarActionRole(),
			ClassName:   props.ClassName,
		}) {
			result.ClickedAction = int32(i)
		}
	}
	return result
}
func menuItemAt(items []MenuItem, start, direction int) int {
	if len(items) == 0 {
		return -1
	}
	index := start
	for range items {
		index = (index + direction + len(items)) % len(items)
		if items[index].Kind != MenuSeparator && !items[index].Disabled {
			return index
		}
	}
	return -1
}

func menuFirstItem(items []MenuItem) int { return menuItemAt(items, -1, 1) }
func menuLastItem(items []MenuItem) int  { return menuItemAt(items, 0, -1) }

func (r *runtime) menuNav(id int32) *menuNavigation {
	state := r.menuNavigation[id]
	if state == nil {
		state = &menuNavigation{}
		r.menuNavigation[id] = state
	}
	return state
}

func resetMenuPath(state *menuNavigation, items []MenuItem) {
	state.Path = state.Path[:0]
	if first := menuFirstItem(items); first >= 0 {
		state.Path = append(state.Path, first)
	}
}

func (r *runtime) menuBar(id int32, className int32, bounds Rectangle, menus []MenuGroup, openIndex *int32) MenuResult {
	result := MenuResult{OpenIndex: -1}
	state := r.menuNav(id)
	barFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenu(), Menu_MenuBarRole())
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenu(), Menu_MenuPopupRole())
	menuItemBaseFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenuItem(), StyleSheet_StyleAny())
	metrics := Menu_MenuMetricsFor(1, panelFrame, menuItemBaseFrame, barFrame)
	if bounds.Width <= 0 {
		bounds.Width = float32(r.GetScreenWidth()) - bounds.X
	}
	if bounds.Height <= 0 {
		bounds.Height = 30
	}
	if openIndex != nil && *openIndex >= 0 && int(*openIndex) < len(menus) {
		r.openMenus[id] = *openIndex
	}
	open := int32(-1)
	openedByKeyboard := false
	if v, ok := r.openMenus[id]; ok {
		open = v
	}
	if len(menus) > 0 {
		state.Top = clamp32(state.Top, 0, int32(len(menus)-1))
	}
	if !r.contentDisabled() {
		r.registerField(id)
	}
	focused := !r.contentDisabled() && id != 0 && r.focusID == id && !r.popupFocusCaptures(id)
	if focused && len(menus) > 0 {
		keyboardInput := Menu_MenuKeyboardInputFor(false, r.keyDown[KeyDown],
			r.keyDown[KeyHome], r.keyDown[KeyEnd], r.keyDown[KeyLeft],
			r.keyDown[KeyRight], r.keyDown[KeyEnter] || r.keyDown[335],
			r.keyDown[KeySpace], r.keyDown[KeyEscape])
		keyboardDecision := Menu_MenuBarKeyboardDecisionFor(keyboardInput,
			open >= 0, int32(len(state.Path)-1))
		if open < 0 {
			if keyboardDecision.MoveTopDelta != 0 {
				state.Top = Menu_MenuBarMoveTopIndex(state.Top,
					int32(len(menus)), keyboardDecision.MoveTopDelta)
			} else if keyboardDecision.FirstTop {
				state.Top = 0
			} else if keyboardDecision.LastTop {
				state.Top = int32(len(menus) - 1)
			} else if keyboardDecision.OpenTop {
				open = state.Top
				r.openMenus[id] = open
				resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
				openedByKeyboard = true
			}
		} else if keyboardDecision.CloseOpen {
			open = -1
			delete(r.openMenus, id)
			delete(r.openSubmenus, id)
			state.Path = state.Path[:0]
		} else if keyboardDecision.MoveOpenDelta != 0 {
			open = Menu_MenuBarMoveTopIndex(open, int32(len(menus)),
				keyboardDecision.MoveOpenDelta)
			state.Top = open
			r.openMenus[id] = open
			delete(r.openSubmenus, id)
			resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
			openedByKeyboard = true
		} else if keyboardDecision.MoveOpenIfNoSubmenuDelta != 0 {
			selected := -1
			if len(state.Path) == 1 {
				selected = state.Path[0]
			}
			opensSubmenu := selected >= 0 && selected < len(limitedMenuItems(menus[open].Items, menus[open].ItemCount)) &&
				limitedMenuItems(menus[open].Items, menus[open].ItemCount)[selected].Kind == MenuSubmenu &&
				!limitedMenuItems(menus[open].Items, menus[open].ItemCount)[selected].Disabled
			if !opensSubmenu {
				open = Menu_MenuBarMoveTopIndex(open, int32(len(menus)),
					keyboardDecision.MoveOpenIfNoSubmenuDelta)
				state.Top = open
				r.openMenus[id] = open
				delete(r.openSubmenus, id)
				resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
				openedByKeyboard = true
			}
		}
	}
	barStyle := unpackStyle(barFrame.Value)
	r.record(styleFrameRectOp(bounds, Rectangle{}, barFrame))
	r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height - 1, Width: bounds.Width, Height: 0}, Color: barStyle.Border})
	x := Menu_MenuBarFirstItemX(bounds, metrics)
	menuItemBaseStyle := unpackStyle(menuItemBaseFrame.Value)
	font, fontID := styleTextFace(menuItemBaseStyle, Text14)
	for i, menu := range menus {
		w := Menu_MenuGroupItemWidth(int32(runtimeTextWidthWithFont(menu.Label, font, fontID)), metrics)
		item := Menu_MenuGroupItemBounds(x, bounds, w, metrics)
		tapped := !r.contentDisabled() && r.consumeTap(item)
		openID := Menu_MenuBarOpenIdFor(id, open, int32(len(menus)))
		itemID := Menu_MenuBarOpenIdFor(id, int32(i), int32(len(menus)))
		pointerDecision := Menu_MenuGroupPointerDecisionFor(itemID, int32(i),
			openID, tapped, tapped)
		if pointerDecision.SetFocus {
			r.setFocus(id)
		}
		if pointerDecision.ChangedOpen {
			open = Menu_MenuBarOpenIndexFor(id, pointerDecision.NextOpenId,
				int32(len(menus)))
			if open < 0 {
				delete(r.openMenus, id)
			} else {
				r.openMenus[id] = open
			}
		}
		if pointerDecision.ClearSubmenu {
			delete(r.openSubmenus, id)
		}
		if pointerDecision.ResetNavigation {
			state.Top = pointerDecision.NavigationTop
			resetMenuPath(state, limitedMenuItems(menu.Items, menu.ItemCount))
		}
		itemSelected := open == int32(i)
		itemFocused := focused && open < 0 && state.Top == int32(i)
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if itemSelected {
				return ButtonStateSelected
			}
			if itemFocused {
				return ButtonStateFocus
			}
			return ButtonStateNormal
		}(), false, itemSelected, className, StyleSheet_StyleKindMenuItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		itemFont, itemFontID := styleTextFace(itemStyle, font)
		if itemSelected || itemFocused {
			op := styleFrameRectOp(item, bounds, itemFrame)
			op.Selected = itemSelected
			op.Focused = itemFocused
			r.record(op)
		}
		labelX := Menu_MenuBarLabelX(item, metrics)
		labelY := Menu_MenuBarLabelY(item, itemFont)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(labelX), Y: float32(labelY), Width: item.X + item.Width - float32(labelX), Height: item.Height}, Text: menu.Label, Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: itemFont, FontID: itemFontID})
		x = Menu_MenuBarNextItemX(x, w, metrics)
	}
	if open >= 0 && int(open) < len(menus) {
		result.OpenIndex = open
		if openIndex != nil {
			*openIndex = open
		}
		menu := menus[open]
		menuItemX := Menu_MenuBarFirstItemX(bounds, metrics)
		menuItemWidth := int32(0)
		for i := 0; i < int(open); i++ {
			w := Menu_MenuGroupItemWidth(int32(runtimeTextWidthWithFont(menus[i].Label, font, fontID)), metrics)
			menuItemX = Menu_MenuBarNextItemX(menuItemX, w, metrics)
		}
		menuItemWidth = Menu_MenuGroupItemWidth(int32(runtimeTextWidthWithFont(menu.Label, font, fontID)), metrics)
		menuItem := Menu_MenuGroupItemBounds(menuItemX, bounds, menuItemWidth, metrics)
		origin := PopupPolicy_PopupMenuBarOrigin(menuItem, bounds)
		items := limitedMenuItems(menu.Items, menu.ItemCount)
		handled := openedByKeyboard
		result.ActivatedID, _ = r.drawPopupMenu(id, className, int32(origin.X), int32(origin.Y), items, id, 0, &handled)
		if result.ActivatedID != 0 {
			delete(r.openMenus, id)
			delete(r.openSubmenus, id)
			open = -1
			state.Path = state.Path[:0]
		}
	}
	if open < 0 && openIndex != nil {
		*openIndex = -1
	}
	return result
}

func limitedMenus(menus []MenuGroup, count int32) []MenuGroup {
	if count <= 0 || int(count) > len(menus) {
		return menus
	}
	return menus[:count]
}

func limitedMenuItems(items []MenuItem, count int32) []MenuItem {
	if count <= 0 || int(count) > len(items) {
		return items
	}
	return items[:count]
}

func (r *runtime) drawPopupMenu(id, className, x, y int32, items []MenuItem, focusID int32, depth int, handled *bool) (int32, Rectangle) {
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenu(), Menu_MenuPopupRole())
	barFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenu(), Menu_MenuBarRole())
	baseFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenuItem(), StyleSheet_StyleAny())
	baseStyle := unpackStyle(baseFrame.Value)
	font, fontID := styleTextFace(baseStyle, Text14)
	metrics := Menu_MenuMetricsFor(1, panelFrame, baseFrame, barFrame)
	width := metrics.PanelMinWidth
	for _, item := range items {
		accelWidth := int32(0)
		if item.Accelerator != "" {
			accelWidth = int32(runtimeTextWidthWithFont(item.Accelerator, font, fontID))
		}
		width = Menu_MenuPanelWidthStep(width,
			int32(runtimeTextWidthWithFont(item.Label, font, fontID)), accelWidth,
			item.Accelerator != "", metrics)
	}
	panel := Menu_MenuPanelBounds(x, y, width, int32(len(items)), metrics)
	if len(items) == 0 {
		return 0, panel
	}
	state := r.menuNav(focusID)
	keyboard := !r.contentDisabled() && focusID != 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
	if keyboard {
		for len(state.Path) <= depth {
			state.Path = append(state.Path, menuFirstItem(items))
		}
		selected := state.Path[depth]
		if selected < 0 || selected >= len(items) || items[selected].Kind == MenuSeparator || items[selected].Disabled {
			selected = menuFirstItem(items)
			state.Path[depth] = selected
		}
		if !*handled && depth == len(state.Path)-1 && selected >= 0 {
			keyboardInput := Menu_MenuKeyboardInputFor(r.keyDown[KeyUp],
				r.keyDown[KeyDown], r.keyDown[KeyHome], r.keyDown[KeyEnd],
				r.keyDown[KeyLeft], r.keyDown[KeyRight],
				r.keyDown[KeyEnter] || r.keyDown[335], r.keyDown[KeySpace],
				false)
			keyboardDecision := Menu_MenuKeyboardDecisionFor(keyboardInput,
				int32(depth), int32(selected))
			if keyboardDecision.MoveDelta != 0 {
				state.Path[depth] = menuItemAt(items, selected,
					int(keyboardDecision.MoveDelta))
				*handled = true
			} else if keyboardDecision.First {
				state.Path[depth] = menuFirstItem(items)
				*handled = true
			} else if keyboardDecision.Last {
				state.Path[depth] = menuLastItem(items)
				*handled = true
			} else if keyboardDecision.CloseParent {
				state.Path = state.Path[:depth]
				*handled = true
			} else if keyboardDecision.OpenOrActivate {
				item := items[selected]
				opensSubmenu := Menu_MenuItemCanOpenSubmenu(int32(item.Kind),
					item.Disabled, item.Submenu != nil,
					int32(len(limitedMenuItems(item.Submenu, item.SubmenuCount))),
					int32(depth), menuMaxDepth)
				if opensSubmenu {
					r.openSubmenus[id] = item.ID
					state.Path = append(state.Path, menuFirstItem(limitedMenuItems(item.Submenu, item.SubmenuCount)))
					*handled = true
				} else if Menu_MenuItemKeyboardActivates(int32(item.Kind),
					item.Disabled, opensSubmenu) {
					*handled = true
					return item.ID, panel
				}
			}
		}
	}
	separatorStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, className, StyleSheet_StyleKindMenuSeparator(), StyleSheet_StyleAny()).Value)
	r.record(styleFrameRectOp(panel, Rectangle{}, panelFrame))
	for i, item := range items {
		row := Menu_MenuRowBounds(panel, int32(i), metrics)
		if item.Kind == MenuSeparator {
			line := Menu_MenuSeparatorLineFor(row, metrics)
			r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: float32(line.X1), Y: float32(line.Y1), Width: float32(line.X2 - line.X1)}, Color: separatorStyle.Border})
			continue
		}
		hovered := !r.contentDisabled() && pointInRect(r.mousePos.X, r.mousePos.Y, row)
		selected := keyboard && len(state.Path) > depth && state.Path[depth] == i
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if item.Disabled {
				return ButtonStateDisabled
			}
			if hovered {
				return ButtonStateHover
			}
			if selected {
				return ButtonStateSelected
			}
			return ButtonStateNormal
		}(), item.Disabled, selected, className, StyleSheet_StyleKindMenuItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		itemFont, itemFontID := styleTextFace(itemStyle, font)
		if hovered && !item.Disabled {
			if len(state.Path) > depth {
				state.Path[depth] = i
				state.Path = state.Path[:depth+1]
			}
			op := styleFrameRectOp(row, panel, itemFrame)
			op.Hovered = true
			r.record(op)
			if item.Kind == MenuSubmenu {
				r.openSubmenus[id] = item.ID
			}
		}
		if selected && !hovered {
			op := styleFrameRectOp(row, panel, itemFrame)
			op.Selected = true
			r.record(op)
		}
		tapped := !item.Disabled && r.consumeTap(row)
		pointerDecision := Menu_MenuItemPointerDecisionFor(hovered, tapped,
			r.focusID == focusID, int32(item.Kind), item.Disabled, item.ID,
			int32(i), int32(depth), menuMaxDepth)
		if pointerDecision.ResetNavigation {
			resetMenuPath(state, items)
		}
		if pointerDecision.SetNavigationPath {
			for len(state.Path) <= int(pointerDecision.NavigationDepth) {
				state.Path = append(state.Path, menuFirstItem(items))
			}
			state.Path[pointerDecision.NavigationDepth] =
				int(pointerDecision.NavigationIndex)
			if pointerDecision.ClearChildNavigation {
				state.Path = state.Path[:int(pointerDecision.NavigationDepth)+1]
			}
		}
		if pointerDecision.SetFocus {
			r.setFocus(focusID)
		}
		if pointerDecision.SetSubmenu {
			r.openSubmenus[id] = pointerDecision.SubmenuId
		}
		if pointerDecision.Activate {
			return pointerDecision.ActivatedID, panel
		}
		textColor := itemStyle.Foreground
		label := item.Label
		if (item.Kind == MenuCheck || item.Kind == MenuRadio) && item.Checked {
			label = "✓ " + label
		}
		labelX := Menu_MenuLabelX(row, metrics)
		textY := Menu_MenuTextY(row, itemFont)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(labelX), Y: float32(textY), Width: row.X + row.Width - float32(labelX), Height: row.Height}, Text: label, Color: textColor, Opacity: itemStyle.Opacity, FontSize: itemFont, FontID: itemFontID, Disabled: item.Disabled})
		if item.Accelerator != "" {
			accelWidth := runtimeTextWidthWithFont(item.Accelerator, itemFont, itemFontID)
			accelX := Menu_MenuAcceleratorX(row, int32(accelWidth), metrics)
			r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(accelX), Y: float32(textY), Width: float32(accelWidth), Height: row.Height}, Text: item.Accelerator, Color: textColor, Opacity: itemStyle.Opacity, FontSize: itemFont, FontID: itemFontID, Disabled: item.Disabled})
		}
		if item.Kind == MenuSubmenu {
			indicatorX := Menu_MenuSubmenuIndicatorX(row, metrics)
			r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: float32(indicatorX), Y: float32(textY), Width: 12, Height: row.Height}, Text: ">", Color: textColor, Opacity: itemStyle.Opacity, FontSize: itemFont, FontID: itemFontID})
			submenuOpen := r.openSubmenus[id] == item.ID
			if keyboard {
				submenuOpen = selected && len(state.Path) > depth+1
			}
			if submenuOpen {
				subitems := limitedMenuItems(item.Submenu, item.SubmenuCount)
				origin := Menu_MenuSubmenuOrigin(row)
				activated, _ := r.drawPopupMenu(item.ID, className, int32(origin.X), int32(origin.Y), subitems, focusID, depth+1, handled)
				if activated != 0 {
					return activated, panel
				}
			}
		}
	}
	return 0, panel
}

func (r *runtime) popupMenu(id, className, x, y int32, items []MenuItem, itemCount int32) int32 {
	items = limitedMenuItems(items, itemCount)
	if !r.contentDisabled() {
		r.registerField(id)
	}
	state := r.menuNav(id)
	if r.focusID == id && !r.popupFocusCaptures(id) && r.keyDown[KeyEscape] {
		state.Path = state.Path[:0]
		r.setFocus(0)
		return 0
	}
	handled := false
	selected, _ := r.drawPopupMenu(id, className, x, y, items, id, 0, &handled)
	return selected
}

func (r *runtime) contextMenu(props MenuProps) int32 {
	if props.ID == 0 {
		return 0
	}
	if props.Open != nil && *props.Open != 0 {
		pos := r.mousePos
		if props.X != nil {
			pos.X = float32(*props.X)
		}
		if props.Y != nil {
			pos.Y = float32(*props.Y)
		}
		r.contextMenus[props.ID] = pos
	}
	if !r.contentDisabled() && r.mouseReleased[MouseButtonRight] && pointInRect(r.mousePos.X, r.mousePos.Y, props.Trigger) {
		r.contextMenus[props.ID] = r.mousePos
		r.setFocus(props.ID)
		resetMenuPath(r.menuNav(props.ID), limitedMenuItems(props.Items, props.ItemCount))
		if props.Open != nil {
			openResult := Menu_MenuContextOpenFor(*props.Open != 0, true, false, props.Open != nil)
			*props.Open = boolInt(openResult.Open)
		}
		if props.X != nil {
			*props.X = int32(r.mousePos.X)
		}
		if props.Y != nil {
			*props.Y = int32(r.mousePos.Y)
		}
	}
	pos, open := r.contextMenus[props.ID]
	if !open {
		return 0
	}
	if !r.contentDisabled() {
		r.registerField(props.ID)
	}
	if !r.contentDisabled() && r.focusID == props.ID && !r.popupFocusCaptures(props.ID) && r.keyDown[KeyEscape] {
		delete(r.contextMenus, props.ID)
		delete(r.openSubmenus, props.ID)
		r.menuNav(props.ID).Path = r.menuNav(props.ID).Path[:0]
		if props.Open != nil {
			openResult := Menu_MenuContextOpenFor(*props.Open != 0, false, true, props.Open != nil)
			*props.Open = boolInt(openResult.Open)
		}
		return 0
	}
	handled := false
	selected, panel := r.drawPopupMenu(props.ID, props.ClassName, int32(pos.X), int32(pos.Y), limitedMenuItems(props.Items, props.ItemCount), props.ID, 0, &handled)
	closeMenu := selected != 0
	if !closeMenu && !r.contentDisabled() {
		for i := range r.taps {
			if !r.taps[i].consumed && !pointInRect(r.taps[i].x, r.taps[i].y, panel) {
				r.taps[i].consumed = true
				closeMenu = true
				break
			}
		}
	}
	if closeMenu {
		delete(r.contextMenus, props.ID)
		delete(r.openSubmenus, props.ID)
		r.menuNav(props.ID).Path = r.menuNav(props.ID).Path[:0]
		if props.Open != nil {
			openResult := Menu_MenuContextOpenFor(*props.Open != 0, false, true, props.Open != nil)
			*props.Open = boolInt(openResult.Open)
		}
	}
	return selected
}

func (r *runtime) Menu(props MenuProps) MenuResult {
	result := MenuResult{OpenIndex: -1}
	switch props.Mode {
	case MenuModeBar:
		return r.menuBar(props.ID, props.ClassName, props.Bounds,
			limitedMenus(props.Menus, props.MenuCount), props.OpenIndex)
	case MenuModePopup:
		result.ActivatedID = r.popupMenu(props.ID, props.ClassName, int32(props.Bounds.X),
			int32(props.Bounds.Y), props.Items, props.ItemCount)
		return result
	default:
		result.ActivatedID = r.contextMenu(props)
		return result
	}
}
func (r *runtime) CanvasGrid(bounds Rectangle, step int32, color Color) {
	bounds = r.layoutRect(bounds)
	spacing := CanvasGrid_CanvasGridSpacing(r.Scale(step), 4)
	packed := packRGBA(color)
	for i, count := int32(0), CanvasGrid_CanvasGridLineCount(bounds.Width, spacing); i < count; i++ {
		line := CanvasGrid_CanvasGridVerticalLine(bounds, i, spacing, packed)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: line.Bounds, Color: unpackRGBA(line.Color)})
	}
	for i, count := int32(0), CanvasGrid_CanvasGridLineCount(bounds.Height, spacing); i < count; i++ {
		line := CanvasGrid_CanvasGridHorizontalLine(bounds, i, spacing, packed)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: line.Bounds, Color: unpackRGBA(line.Color)})
	}
}
func (r *runtime) Toast(props ToastProps) {
	metrics := Toast_ToastMetricsFor(1, StyleFrame{})
	if props.Message == "" {
		r.toastMessage = ""
		r.toastClassName = 0
		r.toastUntil = time.Time{}
		return
	}
	r.toastMessage = props.Message
	r.toastClassName = props.ClassName
	duration := Toast_ToastDuration(float32(props.Seconds), metrics)
	r.toastUntil = time.Now().Add(time.Duration(float64(duration) * float64(time.Second)))
}
func (r *runtime) recordToast() {
	if r.toastMessage == "" || time.Now().After(r.toastUntil) {
		r.toastMessage = ""
		return
	}
	surfaceFrame := StyleFrame{
		Value: ResolveActiveStyle(StyleData{}, Toast_ToastSurfaceFactsFor(r.toastClassName),
			int32(ButtonStateNormal)),
	}
	labelFrame := StyleFrame{
		Value: ResolveActiveStyle(StyleData{}, Toast_ToastLabelFactsFor(r.toastClassName),
			int32(ButtonStateNormal)),
	}
	surface := unpackStyle(surfaceFrame.Value)
	label := unpackStyle(labelFrame.Value)
	metrics := Toast_ToastMetricsFor(1, surfaceFrame)
	labelFont, labelFontID := styleTextFace(label, Text14)
	textWidth := int32(runtimeTextWidthWithFont(r.toastMessage, labelFont, labelFontID))
	layout := Toast_ToastLayoutFor(r.GetScreenWidth(), r.GetScreenHeight(), textWidth, labelFont, metrics)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: layout.Bounds, Color: surface.Background, BorderColor: surface.Border, Radius: surface.Radius, BorderWidth: surface.BorderWidth, Material: MaterialKind(surface.Material), Opacity: surface.Opacity})
	r.record(FrameOp{Kind: FrameOpText, Bounds: layout.TextBounds, Text: r.toastMessage, Color: label.Foreground, Opacity: label.Opacity, FontSize: labelFont, FontID: labelFontID})
}
func (r *runtime) TextArea(props TextAreaProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	changed := r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, nil, props.FocusID, textEditOptions{
		maxCodepoints: props.MaxCodepoints,
		pageRows:      r.textAreaPageRows(props),
		readOnly:      props.ReadOnly,
		multiline:     true,
	})
	r.recordTextArea(props)
	return changed
}

func (r *runtime) textAreaPageRows(props TextAreaProps) int {
	style := r.textInputStyle(FrameOpTextArea, false, r.contentDisabled(), props.ClassName)
	defaultFont := r.textInputDefaultFont(FrameOpTextArea, false, r.contentDisabled(), props.ClassName, Text16)
	metrics := TextInput_TextInputMetricsFor(style.Fields, 0, int32(style.PaddingX), int32(style.PaddingY), styleGapLength(style), defaultFont, 10, 8, 0)
	return int(TextInput_TextAreaPageRows(props.Bounds.Height, metrics.Font, metrics.LineGap, metrics.PaddingY))
}

func (r *runtime) Radio(props RadioProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	input := r.ReadActivation(props.Bounds, props.ID, !props.Disabled)
	state := checkboxButtonState(input.Hovered, input.Pressed, input.Focused, props.Disabled)
	selectedFrame := radioStyleFrame(ButtonToneAccent, state, props.Disabled, props.Checked, props.ClassName, Radio_RadioMarkRole())
	paint := Radio_RadioPaintFor(RadioSpec{
		Bounds:   props.Bounds,
		Checked:  props.Checked,
		Disabled: props.Disabled,
		SelectedAmount: func() float32 {
			if props.Checked {
				return 1
			}
			return 0
		}(),
		Scale:    1,
		Frame:    radioStyleFrame(ButtonToneNeutral, state, props.Disabled, props.Checked, props.ClassName, Radio_RadioRingRole()),
		Selected: selectedFrame,
	})
	label := radioStyleFrame(ButtonToneNeutral, state, props.Disabled, props.Checked, props.ClassName, Radio_RadioLabelRole())
	paint.LabelColor = label.Value.Foreground
	markStyle := unpackStyle(selectedFrame.Value)
	labelStyle := unpackStyle(label.Value)
	labelFont, labelFontID := styleTextFace(labelStyle, Text16)
	markFont, markFontID := styleTextFace(markStyle, Text16)
	mark := Radio_RadioMarkText(props.Checked)
	markColor := unpackRGBA(paint.RingColor)
	if props.Checked {
		markColor = unpackRGBA(paint.FillColor)
	}
	labelColor := unpackRGBA(paint.LabelColor)
	r.record(FrameOp{Kind: FrameOpText, Bounds: paint.MarkBounds, Text: mark, Color: markColor, Opacity: markStyle.Opacity, FontSize: markFont, FontID: markFontID, ID: props.ID, Pressed: input.Pressed, Disabled: props.Disabled, Selected: props.Checked, Focused: input.Focused})
	r.record(FrameOp{Kind: FrameOpText, Bounds: paint.LabelBounds, Text: props.Label, Color: labelColor, Opacity: labelStyle.Opacity, FontSize: labelFont, FontID: labelFontID, ID: props.ID, Pressed: input.Pressed, Disabled: props.Disabled, Selected: props.Checked, Focused: input.Focused})
	return Radio_RadioActivationFor(props.ID, input.Activated, props.Disabled)
}

func radioStyleFrame(tone ButtonTone, state ButtonState, disabled, selected bool, className int32, role int32) StyleFrame {
	props := ButtonProps{
		ClassName: className,
		Tone:      tone,
		Emphasis:  ButtonEmphasisOutline,
		Size:      ControlSizeMedium,
		Disabled:  disabled,
		Selected:  selected,
	}
	if tone == ButtonToneAccent {
		props.Emphasis = ButtonEmphasisFilled
	}
	return resolveMinimalControlRoleFrame(props, state, false, 0, 0, 0,
		StyleSheet_StyleKindRadio(), role)
}

func simpleStyleFrameWithRole(tone ButtonTone, state ButtonState, disabled, selected bool, styleKind int32, role int32) StyleFrame {
	return simpleStyleFrameWithClassRole(tone, state, disabled, selected, 0,
		styleKind, role)
}

func simpleStyleFrameWithClassRole(tone ButtonTone, state ButtonState, disabled, selected bool, className int32, styleKind int32, role int32) StyleFrame {
	props := ButtonProps{
		ClassName: className,
		Tone:      tone,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
		Pill:      true,
		Disabled:  disabled,
		Selected:  selected,
	}
	if tone == ButtonToneAccent {
		props.Emphasis = ButtonEmphasisFilled
	}
	return resolveMinimalControlRoleFrame(props, state, false, 0, 0, 0, styleKind, role)
}

func styleFrameRectOp(bounds, surface Rectangle, frame StyleFrame) FrameOp {
	style := unpackStyle(frame.Value)
	return FrameOp{
		Kind:             FrameOpRect,
		Bounds:           bounds,
		SurfaceBounds:    surface,
		Color:            style.Background,
		BackgroundEnd:    style.BackgroundEnd,
		HasBackgroundEnd: frame.Value.Fields&uint32(StyleBackgroundEnd) != 0,
		FillStates:       frame.Fill,
		FillStatesValid:  true,
		BorderColor:      style.Border,
		FocusColor:       style.Focus,
		TextColor:        style.Foreground,
		Radius:           style.Radius,
		BorderWidth:      style.BorderWidth,
		Opacity:          style.Opacity,
		Material:         style.Material,
	}
}

func (r *runtime) Spinbox(p SpinboxProps) bool {
	p.Bounds = r.layoutRect(p.Bounds)
	layout := Spinbox_SpinboxLayoutFor(p.Bounds, Spinbox_SpinboxDefaultButtonWidth(1.0))
	l := layout.Left
	rr := layout.Right
	disabled := p.Disabled || r.contentDisabled()
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, p.ClassName, StyleSheet_StyleKindSpinbox(), StyleSheet_StyleAny())
	op := styleFrameRectOp(p.Bounds, Rectangle{}, frame)
	op.ID = p.ID
	op.Disabled = disabled
	r.record(op)
	minus := r.buttonAt(ButtonProps{Bounds: l, Label: "-",
		ID: Spinbox_SpinboxDecrementIdFor(p.ID), ClassName: p.ClassName, Disabled: disabled})
	plus := r.buttonAt(ButtonProps{Bounds: rr, Label: "+",
		ID: Spinbox_SpinboxIncrementIdFor(p.ID), ClassName: p.ClassName, Disabled: disabled})
	changed := false
	if p.Value != nil {
		result := Spinbox_SpinboxStepButtonsValue(*p.Value, p.Min, p.Max,
			p.Step, minus, plus, p.Wrap)
		changed = result.Changed
		*p.Value = result.Value
	}
	center := layout.Text
	txt := p.ValueText
	if txt == "" {
		v := int32(0)
		if p.Value != nil {
			v = *p.Value
		}
		txt = fmt.Sprint(v)
	}
	valueFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), disabled, false, p.ClassName, StyleSheet_StyleKindSpinboxValue(), StyleSheet_StyleAny())
	valueStyle := unpackStyle(valueFrame.Value)
	valueOp := styleFrameRectOp(center, p.Bounds, valueFrame)
	valueOp.ID = p.ID
	valueOp.Disabled = disabled
	r.record(valueOp)
	valueFont, valueFontID := styleTextFace(valueStyle, Text16)
	r.record(FrameOp{Kind: FrameOpText, Bounds: center, Text: txt, Color: valueStyle.Foreground, Opacity: valueStyle.Opacity, FontSize: valueFont, FontID: valueFontID, ID: p.ID, Disabled: disabled})
	return changed
}
func (r *runtime) Fieldset(p FieldsetProps) {
	p.Bounds = r.layoutRect(p.Bounds)
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, p.ClassName, StyleSheet_StyleKindFieldset(), StyleSheet_StyleAny())
	style := unpackStyle(frame.Value)
	font, fontID := styleTextFace(style, Text14)
	w := float32(0)
	if p.Title != "" {
		w = float32(runtimeTextWidthWithFont(p.Title, font, fontID))
	}
	paint := Fieldset_FieldsetPaintFor(p.Bounds, w, p.Title != "", 1, frame)
	r.record(styleFrameRectOp(paint.Frame, Rectangle{}, paint.Face))
	if paint.ShowTitle {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: paint.TitleBackground, Color: unpackRGBA(paint.BackgroundColor)})
		r.record(FrameOp{Kind: FrameOpText, Bounds: paint.TitleText, Text: p.Title, Color: unpackRGBA(paint.TextColor), Opacity: style.Opacity, FontSize: font, FontID: fontID})
	}
}
func (r *runtime) PanedView(p PanedViewProps) int32 {
	if p.Split == nil {
		return 0
	}
	p.Bounds = r.layoutRect(p.Bounds)
	split := *p.Split
	normalFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		PanedView_PanedViewHandleFactsFor(p.ClassName, int32(ButtonStateNormal)),
		int32(ButtonStateNormal))}
	metrics := PanedView_PanedViewMetricsFor(1, normalFrame)
	limit := PanedView_PanedViewLimit(PanedView_PanedViewSize(p.Bounds, p.Vertical), p.MinFirst, p.MinSecond)
	split = PanedView_PanedViewClampSplit(split, p.MinFirst, limit)
	h := PanedView_PanedViewHandleFor(p.Bounds, p.Vertical, split, metrics)
	changed := int32(0)
	if r.drag.active && r.popupInputOwnerCaptures(r.drag.owner) {
		r.drag = scalarDrag{}
	}
	if r.contentDisabled() && r.drag.active && r.drag.token == p.ID {
		r.drag = scalarDrag{}
	}
	if !r.contentDisabled() && r.mousePressed[MouseButtonLeft] && r.consumeTap(h) {
		r.drag = scalarDrag{active: true, token: p.ID, owner: r.currentPopupInputOwner()}
	}
	if r.drag.active && r.drag.token == p.ID && r.mouseDown[MouseButtonLeft] {
		n := PanedView_PanedViewPointerSplit(p.Bounds, p.Vertical, r.mousePos.X, r.mousePos.Y)
		n = PanedView_PanedViewClampSplit(n, p.MinFirst, limit)
		if n != *p.Split {
			*p.Split = n
			split = n
			changed = 1
		}
	}
	h = PanedView_PanedViewHandleFor(p.Bounds, p.Vertical, split, metrics)
	if !r.mouseDown[MouseButtonLeft] && r.drag.token == p.ID {
		r.drag = scalarDrag{}
	}
	if *p.Split != split {
		*p.Split = split
		changed = 1
	}
	state := ButtonStateNormal
	if changed != 0 {
		state = ButtonStatePressed
	}
	frame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		PanedView_PanedViewHandleFactsFor(p.ClassName, int32(state)),
		int32(state))}
	op := styleFrameRectOp(h, p.Bounds, frame)
	op.ID = p.ID
	op.Pressed = changed != 0
	r.record(op)
	return changed
}

type treeHeaderNav struct{ id, depth int32 }

func (r *runtime) treeHeaderTarget(p CollapsibleProps, key int32) int32 {
	for i, node := range r.prevTreeHeaders {
		if node.id != p.ID {
			continue
		}
		switch key {
		case KeyDown:
			if i+1 < len(r.prevTreeHeaders) {
				return r.prevTreeHeaders[i+1].id
			}
		case KeyUp:
			if i > 0 {
				return r.prevTreeHeaders[i-1].id
			}
		case KeyRight:
			if i+1 < len(r.prevTreeHeaders) && r.prevTreeHeaders[i+1].depth > node.depth {
				return r.prevTreeHeaders[i+1].id
			}
		case KeyLeft:
			for j := i - 1; j >= 0; j-- {
				if r.prevTreeHeaders[j].depth < node.depth {
					return r.prevTreeHeaders[j].id
				}
			}
		}
		break
	}
	return p.ID
}

func (r *runtime) Collapsible(p CollapsibleProps) int32 {
	if p.Visible != nil && !*p.Visible {
		return 0
	}
	defaultState := ButtonStateNormal
	if p.Disabled || r.contentDisabled() {
		defaultState = ButtonStateDisabled
	} else if p.Selected {
		defaultState = ButtonStateSelected
	}
	headerRole := Collapsible_CollapsibleHeaderRoleFor(p.Tree)
	defaultHeaderFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		defaultState, p.Disabled || r.contentDisabled(), p.Selected,
		p.ClassName, StyleSheet_StyleKindCollapsible(), headerRole)
	treeHeaderFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		defaultState, p.Disabled || r.contentDisabled(), p.Selected,
		p.ClassName, StyleSheet_StyleKindCollapsible(), Collapsible_CollapsibleTreeHeaderRole())
	closeDefaultFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral,
		ButtonStateNormal, p.Disabled || r.contentDisabled(), false,
		p.ClassName, StyleSheet_StyleKindCollapsible(), Collapsible_CollapsibleCloseRole())
	metrics := Collapsible_CollapsibleMetricsFor(1, defaultHeaderFrame,
		treeHeaderFrame, closeDefaultFrame)
	layoutBounds := p.Bounds
	layoutBounds.Height = float32(metrics.HeaderHeight)
	p.Bounds = r.layoutRect(layoutBounds)
	layout := Collapsible_CollapsibleLayoutFor(p.Bounds, p.Tree, p.Depth, p.Visible != nil, metrics)
	enabled := !p.Disabled && !r.contentDisabled()
	if enabled {
		r.registerField(p.ID)
		if p.Tree && p.ID > 0 {
			r.treeHeaders = append(r.treeHeaders, treeHeaderNav{p.ID, max(p.Depth, 0)})
		}
	}
	header := layout.Header
	closeBounds := layout.CloseBounds
	body := layout.Body
	closed := false
	if p.Visible != nil {
		closed = enabled && r.consumeTap(closeBounds)
		if closed {
			*p.Visible = false
		}
	}
	tapped := enabled && !closed && r.consumeTap(body)
	if tapped && p.ID != 0 {
		r.setFocus(p.ID)
	}
	pressed := tapped && !p.Leaf && p.Open != nil
	if pressed {
		openResult := Collapsible_CollapsibleOpenApply(
			*p.Open, true, false, false, true)
		*p.Open = openResult.Open
		pressed = openResult.Changed
	}
	if enabled && p.ID != 0 && r.focusID == p.ID && !r.popupFocusCaptures(p.ID) {
		remaining := r.inputEvents[:0]
		for _, event := range r.inputEvents {
			handled := false
			if !event.shortcut && r.focusID == p.ID {
				if p.Tree && (event.key == KeyUp || event.key == KeyDown ||
					event.key == KeyLeft && (p.Leaf || p.Open == nil || !*p.Open) ||
					event.key == KeyRight && !p.Leaf && p.Open != nil && *p.Open) {
					r.setFocus(r.treeHeaderTarget(p, event.key))
					handled = true
				} else if event.key == KeyTab {
					r.setFocus(r.nextFocus(p.ID, event.shift))
					handled = true
				} else if !p.Leaf && p.Open != nil {
					switch event.key {
					case KeyRight:
						openResult := Collapsible_CollapsibleOpenApply(
							*p.Open, false, true, true, true)
						*p.Open = openResult.Open
						pressed = pressed || openResult.Changed
						handled = true
					case KeyLeft:
						openResult := Collapsible_CollapsibleOpenApply(
							*p.Open, false, true, false, true)
						*p.Open = openResult.Open
						pressed = pressed || openResult.Changed
						handled = true
					case KeyEnter, KeySpace:
						openResult := Collapsible_CollapsibleOpenApply(
							*p.Open, true, false, false, true)
						*p.Open = openResult.Open
						pressed = pressed || openResult.Changed
						handled = true
					}
				}
			}
			if !handled {
				remaining = append(remaining, event)
			}
		}
		r.inputEvents = remaining
	}
	marker := Collapsible_CollapsibleMarkerFor(p.Open != nil && *p.Open, p.Leaf)
	mark := Collapsible_CollapsibleMarkerText(marker)
	state := ButtonStateNormal
	if !enabled {
		state = ButtonStateDisabled
	} else if pressed {
		state = ButtonStatePressed
	} else if enabled && p.ID != 0 && r.focusID == p.ID {
		state = ButtonStateFocus
	} else if p.Selected {
		state = ButtonStateSelected
	}
	buttonProps := ButtonProps{
		Bounds:    header,
		ID:        p.ID,
		ClassName: p.ClassName,
		Tone:      ButtonToneNeutral,
		Emphasis:  ButtonEmphasisSoft,
		Size:      ControlSizeMedium,
		Selected:  p.Selected,
		Disabled:  !enabled,
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, !enabled, p.Selected,
		p.ClassName, StyleSheet_StyleKindCollapsible(), headerRole)
	headerStyle := unpackStyle(frame.Value)
	headerFont, headerFontID := styleTextFace(headerStyle, Text16)
	label := elideTextWithFont(mark+"  "+p.Label, body.Width-12, headerFont, headerFontID)
	buttonProps.Label = label
	button := Button_BuildFrame(buttonProps, ButtonInput{}, frame, InteractionMotion{},
		Rectangle{}, packRGBA(r.appAmbientColor()), 1,
		headerFont, Text16)
	fg := unpackRGBA(button.Foreground)
	r.recordButton(FrameOp{Kind: FrameOpButton, Button: button, Opacity: button.Appearance.Value.Opacity,
		Bounds: header, Text: label, Color: unpackRGBA(button.Appearance.Value.Background),
		BorderColor: unpackRGBA(button.Appearance.Value.Border), TextColor: fg,
		BorderWidth: button.Appearance.Value.BorderWidth, Radius: button.Appearance.Value.Radius,
		Material: MaterialKind(button.Appearance.Value.Material), FontSize: headerFont, FontID: headerFontID,
		Pressed: pressed, Selected: p.Selected, ID: p.ID,
		Focused: enabled && p.ID != 0 && r.focusID == p.ID, Disabled: !enabled})
	if p.Visible != nil {
		closeState := ButtonStateNormal
		if !enabled {
			closeState = ButtonStateDisabled
		} else if closed {
			closeState = ButtonStatePressed
		}
		closeStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, closeState, !enabled,
			false, p.ClassName, StyleSheet_StyleKindCollapsible(), Collapsible_CollapsibleCloseRole()).Value)
		closeFont, closeFontID := styleTextFace(closeStyle, Text16)
		r.record(FrameOp{Kind: FrameOpText, Bounds: closeBounds, Text: "×", Color: closeStyle.Foreground, Opacity: closeStyle.Opacity, FontSize: closeFont, FontID: closeFontID, Pressed: closed, Disabled: !enabled})
	}
	if pressed || closed {
		return 1
	}
	return 0
}
func (r *runtime) TreeView(props TreeViewProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	count := props.ItemCount
	if count <= 0 || count > int32(len(props.Items)) {
		count = int32(len(props.Items))
	}
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if props.Disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), props.Disabled, false, props.ClassName, StyleSheet_StyleKindTreeView(), StyleSheet_StyleAny())
	defaultItemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if props.Disabled {
			return ButtonStateDisabled
		}
		return ButtonStateNormal
	}(), props.Disabled, false, props.ClassName, StyleSheet_StyleKindTreeViewItem(), StyleSheet_StyleAny())
	metrics := TreeView_TreeViewMetricsFor(1, panelFrame, defaultItemFrame)
	rowH := TreeView_TreeViewRowHeight(props.RowHeight, 1, metrics)
	contentHeight := TreeView_TreeViewContentHeight(count, rowH)
	maxScroll := TreeView_TreeViewMaxScroll(int32(props.Bounds.Height), contentHeight)
	if props.ScrollOffset != nil {
		*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
		if !props.Disabled && r.pointerCanReach(props.Bounds) && r.mouseWheel != 0 {
			*props.ScrollOffset = clamp32(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, 0, maxScroll)
		}
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	scrollLayout := TreeView_TreeViewScrollFor(scroll, rowH)
	visible := TreeView_TreeViewVisibleRows(int32(props.Bounds.Height), rowH)
	panelOp := styleFrameRectOp(props.Bounds, Rectangle{}, panelFrame)
	panelOp.ID = props.ID
	panelOp.Disabled = props.Disabled
	r.record(panelOp)
	changed := int32(0)
	first := scrollLayout.First
	yOffset := scrollLayout.YOffset
	for visibleIndex := int32(0); visibleIndex < visible && first+visibleIndex < count; visibleIndex++ {
		index := first + visibleIndex
		item := props.Items[index]
		row := TreeView_TreeViewRowBounds(props.Bounds, visibleIndex, rowH, yOffset)
		markerBounds := TreeView_TreeViewMarkerBounds(row, item.Depth, metrics)
		textBounds := TreeView_TreeViewTextBounds(row, item.Depth, metrics)
		selected := props.SelectedID != nil && *props.SelectedID == item.ID
		pressed := !props.Disabled && item.Selectable != 0 && r.consumeTap(row)
		if pressed && props.SelectedID != nil {
			*props.SelectedID = item.ID
			selected = true
			changed = 1
		}
		hovered := !props.Disabled && pointInRect(r.mousePos.X, r.mousePos.Y, row)
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if props.Disabled {
				return ButtonStateDisabled
			}
			if hovered {
				return ButtonStateHover
			}
			if selected {
				return ButtonStateSelected
			}
			return ButtonStateNormal
		}(), props.Disabled, selected, props.ClassName, StyleSheet_StyleKindTreeViewItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		if selected || hovered || props.Disabled {
			op := styleFrameRectOp(row, props.Bounds, itemFrame)
			op.ID = props.ID
			op.Row = index
			op.Selected = selected
			op.Hovered = hovered
			op.Disabled = props.Disabled
			r.record(op)
		}
		mark := ">"
		if item.Expanded != 0 {
			mark = "v"
		}
		font, fontID := styleTextFace(itemStyle, Text16)
		textPaint := TreeView_TreeViewTextPaintFor(markerBounds, textBounds, font)
		markerBounds.X = float32(textPaint.MarkerX)
		markerBounds.Y = float32(textPaint.MarkerY)
		markerBounds.Height = float32(font)
		textBounds.X = float32(textPaint.TextX)
		textBounds.Y = float32(textPaint.TextY)
		textBounds.Height = float32(font)
		r.record(FrameOp{Kind: FrameOpText, Bounds: markerBounds, Text: mark, Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: font, FontID: fontID, ID: item.ID, Row: index, Disabled: props.Disabled})
		r.record(FrameOp{Kind: FrameOpText, Bounds: textBounds, Text: item.Label, Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: font, FontID: fontID, ID: item.ID, Row: index, Pressed: pressed, Selected: selected, Disabled: props.Disabled})
	}
	return changed
}
func (r *runtime) ListBox(props ListBoxProps) int32 {
	if props.Selected != nil {
		return r.listBoxMultiSelect(props)
	}
	props = normalizeListBoxProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	props.Disabled = props.Disabled || r.contentDisabled()
	defaultItemFrame := listBoxItemMetricFrame(props.ClassName, props.Disabled)
	rowH := ListBox_ListBoxRowHeight(props.RowHeight, 1, defaultItemFrame)
	maxScroll := ListBox_ListBoxMaxScroll(props.Bounds.Height, int32(len(props.Items)), rowH, 0, 1, defaultItemFrame)
	if props.ScrollOffset != nil {
		*props.ScrollOffset = ListBox_ListBoxClampScroll(*props.ScrollOffset, maxScroll)
	}
	changed := int32(0)
	if !props.Disabled && r.pointerCanReach(props.Bounds) && props.ScrollOffset != nil && r.mouseWheel != 0 {
		*props.ScrollOffset = ListBox_ListBoxClampScroll(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, maxScroll)
		changed = 1
	}
	if !props.Disabled && props.ID != 0 {
		r.registerField(props.ID)
	}
	if !props.Disabled && props.ID != 0 && r.focusID == props.ID &&
		!r.popupFocusCaptures(props.ID) && props.SelectedIndex != nil && len(props.Items) > 0 {
		key := int32(0)
		switch {
		case r.keyDown[KeyHome]:
			key = 1
		case r.keyDown[KeyEnd]:
			key = 2
		case r.keyDown[KeyUp]:
			key = 3
		case r.keyDown[KeyDown]:
			key = 4
		}
		if key != 0 {
			scroll := int32(0)
			if props.ScrollOffset != nil {
				scroll = *props.ScrollOffset
			}
			nav := ListBox_ListBoxNavigate(*props.SelectedIndex, int32(len(props.Items)), key, scroll, rowH, props.Bounds.Height, maxScroll, 1, defaultItemFrame)
			if nav.Changed {
				*props.SelectedIndex = nav.Selected
				changed = 1
			}
			if props.ScrollOffset != nil {
				*props.ScrollOffset = nav.Scroll
			}
		}
	}
	changed |= r.recordListBoxOps(props, rowH)
	return changed
}

func tableViewMetrics(props TableViewProps) TableViewMetrics {
	state := ButtonStateNormal
	if props.Disabled {
		state = ButtonStateDisabled
	}
	tableFrame := StyleFrame{Value: ResolveActiveStyle(StyleData{},
		TableView_TableViewFactsFor(props.ClassName, int32(state)), int32(state))}
	headerFrame := tableViewRoleFrame(props.ClassName, state, TableView_TableViewHeaderRole())
	cellFrame := tableViewRoleFrame(props.ClassName, state, TableView_TableViewCellRole())
	dividerFrame := tableViewRoleFrame(props.ClassName, state, TableView_TableViewDividerRole())
	return TableView_TableViewMetricsFor(1, tableFrame, headerFrame, cellFrame, dividerFrame)
}

func tableViewRoleFrame(className int32, state ButtonState, role int32) StyleFrame {
	facts := TableView_TableViewRoleFactsFor(className, role, int32(state))
	value := ResolveActiveStyle(StyleData{}, facts, int32(state))
	return StyleFrame{Value: value}
}

func (r *runtime) TableView(props TableViewProps) int32 {
	props = normalizeTableViewProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	props.Disabled = props.Disabled || r.contentDisabled()
	if len(props.Columns) == 0 {
		return 0
	}

	metrics := tableViewMetrics(props)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), props.RowHeight, props.HeaderHeight, props.FreezeRows, 1, metrics)
	rowH := layout.RowHeight
	headerH := layout.HeaderHeight
	body := layout.Body
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
	if props.PastedText != nil {
		*props.PastedText = ""
	}
	if props.PastedRow != nil {
		*props.PastedRow = -1
	}
	if props.PastedColumn != nil {
		*props.PastedColumn = -1
	}

	changed := int32(0)
	if r.tableResize.active && r.popupInputOwnerCaptures(r.tableResize.owner) {
		r.tableResize = tableResize{}
	}
	if r.tableResize.active && r.mouseReleased[MouseButtonLeft] &&
		(r.tableResize.id != props.ID || props.Disabled || !props.Resizable || len(props.ColumnWidths) == 0) {
		r.tableResize = tableResize{}
	}
	maxScroll := layout.MaxScroll
	if props.ScrollOffset != nil {
		*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
	}
	if props.Disabled {
		r.record(FrameOp{Kind: FrameOpTable, Bounds: props.Bounds, ID: props.ID, Disabled: true})
		r.drawTableOps(props, rowH, headerH)
		return 0
	}
	if props.Resizable && len(props.ColumnWidths) > 0 {
		for i := range r.clicks {
			click := &r.clicks[i]
			if click.consumed || click.button != MouseButtonLeft ||
				click.y < props.Bounds.Y || click.y >= props.Bounds.Y+float32(headerH) {
				continue
			}
			shift := tableHeaderShift(props, click.y)
			column, separatorX := tableSeparatorAtX(props, click.x-shift, float32(metrics.ResizeTolerance))
			separatorX += shift
			if column < 0 || int(column) >= len(props.ColumnWidths) {
				continue
			}
			click.consumed = true
			tolerance := float32(metrics.ResizeTolerance)
			r.consumeTap(Rectangle{X: separatorX - tolerance, Y: props.Bounds.Y, Width: tolerance * 2, Height: float32(headerH)})
			r.tableResize = tableResize{active: true, id: props.ID, column: column, startX: click.x, startWidth: tableColumnWidth(props, column), owner: r.currentPopupInputOwner()}
			break
		}
		if r.tableResize.active && r.tableResize.id == props.ID {
			if r.mouseDown[MouseButtonLeft] {
				minimum := TableView_TableViewMinimumColumnWidth(props.MinColumnWidth, 1, metrics)
				width := max32(minimum, r.tableResize.startWidth+int32(r.mousePos.X-r.tableResize.startX))
				if props.ColumnWidths[r.tableResize.column] != width {
					props.ColumnWidths[r.tableResize.column] = width
					changed = 1
				}
			}
			if r.mouseReleased[MouseButtonLeft] {
				r.tableResize = tableResize{}
			}
		}
	}
	if !props.Disabled && r.pointerCanReach(body) && props.ScrollOffset != nil && r.mouseWheel != 0 {
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
		col := tableColumnAtX(props, headerClickX-tableHeaderShift(props, r.mousePos.Y))
		previousSortColumn := int32(-1)
		if props.SortColumn != nil {
			previousSortColumn = *props.SortColumn
		}
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
		if col >= 0 && props.SortColumn != nil && props.SortDirection != nil {
			if previousSortColumn != col || *props.SortDirection == 0 {
				*props.SortDirection = 1
			} else if *props.SortDirection > 0 {
				*props.SortDirection = -1
			} else {
				*props.SortDirection = 0
			}
			changed = 1
		}
		if props.ID != 0 {
			r.setFocus(props.ID)
		}
	}

	if !props.CustomCells {
		if r.tableDrag.active && r.popupInputOwnerCaptures(r.tableDrag.owner) {
			r.tableDrag = tableDrag{}
		}
		for _, click := range r.consumeMouseButtonEvents(MouseButtonLeft, body) {
			row, col := tableCellAt(props, body, rowH, click.x, click.y)
			if row >= 0 && col >= 0 {
				changed |= setTableSelection(props, row, col, row, col)
				r.tableDrag = tableDrag{active: true, id: props.ID, startRow: row, startCol: col, owner: r.currentPopupInputOwner()}
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
		if r.tableDrag.active && r.tableDrag.id == props.ID && r.mouseDown[MouseButtonLeft] {
			row, col := tableCellAt(props, body, rowH, r.mousePos.X, r.mousePos.Y)
			if row >= 0 && col >= 0 {
				changed |= setTableSelection(props, r.tableDrag.startRow, r.tableDrag.startCol, row, col)
			}
		}
		if r.tableDrag.active && r.tableDrag.id == props.ID && r.mouseReleased[MouseButtonLeft] {
			r.tableDrag = tableDrag{}
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

		if !r.contentDisabled() && !props.Disabled && props.ID != 0 && r.focusID == props.ID &&
			!r.popupFocusCaptures(props.ID) {
			changed |= r.handleTableKeys(props)
		}
	}

	r.record(FrameOp{Kind: FrameOpTable, Bounds: props.Bounds, ID: props.ID})
	r.drawTableOps(props, rowH, headerH)
	return changed
}
func (r *runtime) CanvasScope(canvas Canvas) CanvasResult {
	var scrollX, scrollY int32
	zoom := float32(1)
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, canvas.ClassName, StyleSheet_StyleKindCanvas(), StyleSheet_StyleAny())
	r.record(styleFrameRectOp(canvas.Bounds, canvas.Bounds, frame))
	if canvas.ScrollX != nil {
		scrollX = *canvas.ScrollX
	}
	if canvas.ScrollY != nil {
		scrollY = *canvas.ScrollY
	}
	if canvas.Zoom != nil {
		zoom = *canvas.Zoom
	}
	policy := Canvas_CanvasBeginResultFor(canvas.Bounds, r.mousePos, scrollX, scrollY, zoom, r.mouseDown[MouseButtonLeft])
	return CanvasResult{Active: policy.Active, Dragging: policy.Dragging, World: policy.World}
}
func (r *runtime) CanvasEndScope(Canvas) {}
func (r *runtime) SetCurrentTheme(themeID int32, darkMode int32) {
	r.defaultTheme = false
	r.activeTheme = nil
	r.activeThemeFamily = nil
	r.currentThemeID = normalizeTheme(themeID)
	if darkMode != 0 {
		r.themeMode = ThemeModeDark
	} else {
		r.themeMode = ThemeModeLight
	}
}

func (r *runtime) SetTheme(theme Theme) {
	r.defaultTheme = false
	copy := theme
	r.activeThemeFamily = nil
	r.activeTheme = &copy
	r.themeMode = theme.Mode
}

func (r *runtime) SetThemeFamily(family ThemeFamily) {
	r.defaultTheme = false
	copy := family
	copy.Light.Mode = ThemeModeLight
	copy.Dark.Mode = ThemeModeDark
	r.activeThemeFamily = &copy
	r.applyThemeFamily()
}

func (r *runtime) GetThemeFamily() ThemeFamily {
	if r.activeThemeFamily == nil {
		return ThemeFamily{}
	}
	return *r.activeThemeFamily
}

func (r *runtime) applyThemeFamily() {
	if r.activeThemeFamily == nil {
		return
	}
	selected := r.activeThemeFamily.Light
	if r.effectiveDark() {
		selected = r.activeThemeFamily.Dark
	}
	r.activeTheme = &selected
}

func (r *runtime) GetTheme() Theme {
	if r.activeTheme != nil {
		return *r.activeTheme
	}
	if r.effectiveDark() {
		return ThemeDefaultDark()
	}
	return ThemeDefaultLight()
}
func (r *runtime) SetThemeSource(source ThemeSource) {
	if r.defaultTheme {
		r.defaultTheme = false
		r.activeTheme = nil
		r.activeThemeFamily = nil
	}
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
	r.applyThemeFamily()
}

func (r *runtime) GetThemeMode() ThemeMode {
	return r.themeMode
}

func themeLabel(id int32) string {
	switch normalizeTheme(id) {
	case ThemeSky:
		return "Sky"
	case ThemeOcean:
		return "Ocean"
	case ThemeForest:
		return "Forest"
	case ThemeSunset:
		return "Sunset"
	case ThemeLavender:
		return "Lavender"
	case ThemeCherry:
		return "Cherry"
	case ThemeDawn:
		return "Dawn"
	case ThemeSage:
		return "Sage"
	case ThemeInk:
		return "Ink"
	case ThemeMint:
		return "Mint"
	case ThemeCobalt:
		return "Cobalt"
	case ThemePlan9:
		return "Plan9"
	case ThemeXfce:
		return "Xfce"
	case ThemeSweet:
		return "Sweet"
	default:
		return "Mono"
	}
}

func (r *runtime) TextField(props TextFieldProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	focused := r.focusID == props.FocusID || props.Focused != nil && *props.Focused
	defaultFont := r.textInputDefaultFont(FrameOpTextField, focused, r.contentDisabled(), props.ClassName, Text16)
	style := r.textInputStyle(FrameOpTextField, focused, r.contentDisabled(), props.ClassName)
	metrics := TextInput_TextInputMetricsFor(style.Fields, 0, int32(style.PaddingX), int32(style.PaddingY), 0, defaultFont, 10, 8, 0)
	r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, props.CommitPressed, props.FocusID, textEditOptions{
		maxCodepoints: props.MaxCodepoints,
		secure:        props.Secure,
		readOnly:      props.ReadOnly,
	})
	r.recordTextInput(FrameOpTextField, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, metrics.Font, props.Secure, props.ReadOnly, textInputRecordOptions{
		className: props.ClassName,
		paddingX:  metrics.PaddingX,
		paddingY:  metrics.PaddingY,
	})
}

func (r *runtime) theme() themePalette {
	if r.activeTheme != nil {
		colors := r.activeTheme.Colors
		return themeSelectionDefaults(themePalette{
			background:   colors.Background,
			surface:      colors.Surface,
			text:         colors.Text,
			circle:       colors.Accent,
			button:       colors.Accent,
			buttonHover:  colors.AccentHover,
			icon:         colors.Icon,
			link:         colors.Link,
			linkHover:    colors.LinkHover,
			textDisabled: colors.DisabledText,
			border:       colors.Border,
			focus:        colors.Focus,
			selected:     colors.Selection,
		})
	}
	dark := r.effectiveDark()
	if r.themeSource == ThemeSourceSystem {
		if palette, ok := currentSystemTheme(dark); ok {
			return palette
		}
	}
	return themeCatalogPalette(r.currentThemeID, dark)
}

func (r *runtime) effectiveDark() bool {
	return Theme_ResolveDark(ThemePolicy(r.themeMode), systemPrefersDark())
}

func systemPrefersDark() bool {
	return systemThemePrefersDark()
}

func (r *runtime) record(op FrameOp) {
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

func (r *runtime) recordTextInput(kind FrameOpKind, bounds Rectangle, buf []byte, cursor *int32, focused *bool, focusID, font int32, secure, readOnly bool, options ...textInputRecordOptions) {
	text := string(buf[:zeroIndex(buf)])
	pos := len(text)
	if cursor != nil {
		pos = clampCursor(text, int(*cursor))
	}
	selectionStart, selectionEnd := pos, pos
	if sel, ok := r.selection[focusID]; ok {
		selectionStart, selectionEnd = selectionRange(sel)
	}
	compositionStart, compositionEnd := 0, 0
	if preedit, ok := r.preedit[focusID]; ok && r.focusID == focusID && !secure {
		if view, visible := makeTextCompositionView(text, selectionStart, selectionEnd, preedit); visible {
			text = view.text
			pos = view.cursor
			selectionStart = view.selectionStart
			selectionEnd = view.selectionEnd
			compositionStart = view.compositionStart
			compositionEnd = view.compositionEnd
		}
	}
	if secure {
		text = strings.Repeat("*", utf8.RuneCountInString(text))
	}
	fieldFocused := r.focusID == focusID || focused != nil && *focused
	disabled := r.contentDisabled()
	var opt textInputRecordOptions
	if len(options) > 0 {
		opt = options[0]
	}
	paint := r.textInputStyle(kind, fieldFocused, disabled, opt.className)
	op := FrameOp{
		Kind:              kind,
		Bounds:            bounds,
		Text:              text,
		Color:             paint.Background,
		BorderColor:       paint.Border,
		FocusColor:        paint.Focus,
		AmbientColor:      r.appAmbientColor(),
		TextColor:         paint.Foreground,
		SelectionColor:    paint.Focus,
		SelectedTextColor: paint.Foreground,
		CursorColor:       paint.Focus,
		Radius:            paint.Radius,
		BorderWidth:       paint.BorderWidth,
		Opacity:           paint.Opacity,
		Material:          paint.Material,
		FillStates:        styleFill(paint),
		FillStatesValid:   true,
		FontSize:          font,
		FontID:            styleFontID(paint),
		Gap:               float32(opt.lineGap),
		ContentOffset:     Vector2{X: float32(opt.paddingX), Y: float32(opt.paddingY)},
		ScrollY:           opt.scrollY,
		Wrap:              opt.wrap,
		FocusID:           focusID,
		Cursor:            int32(pos),
		SelectionStart:    int32(selectionStart),
		SelectionEnd:      int32(selectionEnd),
		CompositionStart:  int32(compositionStart),
		CompositionEnd:    int32(compositionEnd),
		Focused:           r.focusID == focusID,
		Disabled:          disabled,
		Secure:            secure,
		ReadOnly:          readOnly,
	}
	if focused != nil {
		op.Focused = *focused
	}
	r.record(op)
}

func (r *runtime) recordTextArea(props TextAreaProps) {
	focused := r.focusID == props.FocusID || props.Focused != nil && *props.Focused
	defaultFont := r.textInputDefaultFont(FrameOpTextArea, focused, r.contentDisabled(), props.ClassName, Text16)
	style := r.textInputStyle(FrameOpTextArea, focused, r.contentDisabled(), props.ClassName)
	metrics := TextInput_TextInputMetricsFor(style.Fields, 0, int32(style.PaddingX), int32(style.PaddingY), styleGapLength(style), defaultFont, 10, 8, 0)
	scrollY := int32(0)
	if props.ScrollY != nil {
		scrollY = *props.ScrollY
	}
	r.recordTextInput(FrameOpTextArea, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, metrics.Font, false, props.ReadOnly, textInputRecordOptions{
		className: props.ClassName,
		lineGap:   metrics.LineGap,
		paddingX:  metrics.PaddingX,
		paddingY:  metrics.PaddingY,
		scrollY:   scrollY,
		wrap:      props.Wrap,
	})
}

type textInputRecordOptions struct {
	className int32
	lineGap   int32
	paddingX  int32
	paddingY  int32
	scrollY   int32
	wrap      bool
}

func (r *runtime) textInputStyle(kind FrameOpKind, focused, disabled bool, className int32) Style {
	state := ButtonStateNormal
	if focused {
		state = ButtonStateFocus
	}
	if disabled {
		state = ButtonStateDisabled
	}
	styleKind := StyleSheet_StyleKindTextField()
	if kind == FrameOpTextArea {
		styleKind = StyleSheet_StyleKindTextArea()
	}
	return resolveButtonStyleForKind(r.theme(), r.effectiveDark(), r.activeTheme,
		ButtonProps{ClassName: className, Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft, Disabled: disabled}, state, styleKind)
}

func (r *runtime) textInputDefaultFont(kind FrameOpKind, focused, disabled bool, className int32, fallback int32) int32 {
	style := r.textInputStyle(kind, focused, disabled, className)
	return styleFont(style, fallback)
}

func styleGapLength(style Style) int32 {
	if style.Fields&StyleGap == 0 {
		return -1
	}
	return styleLength(style.Gap)
}

func (r *runtime) pushLayout(props ColumnProps, horizontal bool, kind FrameOpKind) {
	bounds := r.layoutRect(props.Bounds)
	metrics := Layout_LayoutMetricsFor(bounds, props.Gap, props.Padding)
	r.layout = append(r.layout, layoutFrame{
		bounds:     bounds,
		cursorX:    metrics.Content.X,
		cursorY:    metrics.Content.Y,
		gap:        float32(metrics.Gap),
		padding:    float32(metrics.Padding),
		horizontal: horizontal,
	})
	r.record(FrameOp{Kind: kind, Bounds: bounds, ID: int32(props.Key)})
}

func (r *runtime) pushGrid(props GridProps) {
	bounds := r.layoutRect(props.Bounds)
	props.Bounds = bounds
	cursor := Grid_BeginGridCursor(props)
	r.layout = append(r.layout, layoutFrame{
		bounds:     bounds,
		gridCursor: cursor,
	})
	r.record(FrameOp{Kind: FrameOpGrid, Bounds: bounds, ID: int32(props.Key), Columns: cursor.Metrics.Columns})
}

func (r *runtime) pushGroup(props ColumnProps, kind FrameOpKind) {
	policy := Group_GroupPolicyFor(props.Bounds, props.Gap, props.Padding)
	bounds := policy.Bounds
	gap := policy.Gap
	padding := policy.Padding
	if kind == FrameOpScreen {
		policy = Group_ScreenGroupPolicyFor(props.Bounds, r.GetScreenWidth(), r.GetScreenHeight(), props.Gap, props.Padding)
		bounds = policy.Bounds
		gap = policy.Gap
		padding = policy.Padding
	}
	r.layout = append(r.layout, layoutFrame{
		bounds:   bounds,
		gap:      float32(gap),
		padding:  float32(padding),
		noLayout: true,
	})
	r.record(FrameOp{Kind: kind, Bounds: bounds, ID: int32(props.Key)})
}

func (r *runtime) layoutRect(bounds Rectangle) Rectangle {
	if len(r.layout) == 0 {
		return bounds
	}
	frame := &r.layout[len(r.layout)-1]
	if frame.noLayout {
		return bounds
	}
	if frame.center {
		return Style_CenterChild(bounds, bounds, frame.bounds)
	}
	if bounds.X != 0 || bounds.Y != 0 {
		return bounds
	}
	if frame.gridCursor.Metrics.Columns > 0 {
		height := int32(bounds.Height)
		if height <= 0 {
			height = int32(frame.gridCursor.Metrics.Content.Height)
		}
		frame.gridCursor = Grid_GridStep(frame.gridCursor, height, 1)
		return frame.gridCursor.Item
	}
	out := bounds
	metrics := Layout_LayoutMetricsFor(frame.bounds, int32(frame.gap), int32(frame.padding))
	cursor := frame.cursorY
	if frame.horizontal {
		cursor = frame.cursorX
	}
	out = Layout_LayoutChildBounds(bounds, out, metrics, frame.horizontal, cursor)
	if frame.horizontal {
		frame.cursorX += out.Width + frame.gap
	} else {
		frame.cursorY += out.Height + frame.gap
	}
	return out
}

func (r *runtime) setRoute(path string) {
	oldPath := r.GetRoutePath()
	oldHash := r.GetRouteHash()
	if path == "" {
		r.routePath = "/"
		r.routeHash = ""
	} else if idx := strings.Index(path, "#"); idx >= 0 {
		r.routePath = path[:idx]
		r.routeHash = path[idx:]
	} else {
		r.routePath = path
		r.routeHash = ""
	}
	if r.routePath == "" {
		r.routePath = "/"
	}
	if r.routePath != oldPath || r.routeHash != oldHash {
		r.routeVersion++
	}
}

type textEditOptions struct {
	maxCodepoints int32
	pageRows      int
	secure        bool
	readOnly      bool
	multiline     bool
}

func (r *runtime) editText(bounds Rectangle, buf []byte, cursor *int32, focused *bool, commit *bool, focusID int32, options textEditOptions) bool {
	if options.readOnly {
		delete(r.preedit, focusID)
	}
	if len(buf) == 0 {
		return false
	}
	r.registerField(focusID)
	if focused != nil {
		r.focusRefs[focusID] = focused
	}
	if commit != nil {
		*commit = false
	}
	if r.contentDisabled() || r.popupKeyboardCaptures() {
		delete(r.preedit, focusID)
		return false
	}
	tapX, tapped := r.consumeTapPoint(bounds)
	if focusID != 0 && tapped {
		r.setFocus(focusID)
	}
	if focused != nil && *focused {
		r.setFocus(focusID)
	}
	if r.focusID != focusID {
		delete(r.preedit, focusID)
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
		sel = collapsedSelection(pos)
	}
	changed := false
	for _, event := range r.inputEvents {
		if event.text != "" {
			if options.readOnly {
				continue
			}
			var inserted bool
			text, pos, inserted = insertText(text, pos, sel, event.text, textLimit(buf, options.maxCodepoints))
			if inserted {
				changed = true
				sel = collapsedSelection(pos)
			}
			continue
		}
		textSelection := event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift]
		if event.shortcut {
			switch event.key {
			case KeyA:
				sel = selectAllSelection(len(text))
			case KeyC:
				if !options.secure && sel.Anchor != sel.Cursor {
					start, end := selectionRange(sel)
					r.clipboard = text[start:end]
				}
			case KeyX:
				if options.readOnly {
					continue
				}
				if !options.secure && sel.Anchor != sel.Cursor {
					start, end := selectionRange(sel)
					r.clipboard = text[start:end]
					text = text[:start] + text[end:]
					pos = start
					sel = collapsedSelection(pos)
					changed = true
				}
			case KeyV:
				if options.readOnly {
					continue
				}
				var inserted bool
				text, pos, inserted = insertText(text, pos, sel, r.clipboard, textLimit(buf, options.maxCodepoints))
				if inserted {
					changed = true
					sel = collapsedSelection(pos)
				}
			case KeyHome:
				pos, sel = textMoveSelection(sel, pos, 0, textSelection)
			case KeyEnd:
				pos, sel = textMoveSelection(sel, pos, len(text), textSelection)
			case KeyLeft:
				target := 0
				if !options.secure {
					target = textWordLeft(text, pos)
				}
				if !textSelection && sel.Anchor != sel.Cursor {
					target, _ = selectionRange(sel)
				}
				pos, sel = textMoveSelection(sel, pos, target, textSelection)
			case KeyRight:
				target := len(text)
				if !options.secure {
					target = textWordRight(text, pos)
				}
				if !textSelection && sel.Anchor != sel.Cursor {
					_, target = selectionRange(sel)
				}
				pos, sel = textMoveSelection(sel, pos, target, textSelection)
			case KeyBackspace, KeyDelete:
				if options.readOnly {
					continue
				}
				var deleted bool
				text, pos, sel, deleted = textDeleteKey(
					text, pos, sel, event.key, true, options.secure,
				)
				changed = changed || deleted
			}
			continue
		}
		switch event.key {
		case KeyTab:
			r.setFocus(r.nextFocus(focusID, event.shift))
			sel = collapsedSelection(pos)
		case KeyLeft:
			target := prevRune(text, pos)
			if !textSelection && sel.Anchor != sel.Cursor {
				target, _ = selectionRange(sel)
			}
			pos, sel = textMoveSelection(sel, pos, target, textSelection)
		case KeyRight:
			target := nextRune(text, pos)
			if !textSelection && sel.Anchor != sel.Cursor {
				_, target = selectionRange(sel)
			}
			pos, sel = textMoveSelection(sel, pos, target, textSelection)
		case KeyHome:
			target := 0
			if options.multiline {
				target = textLineStart(text, pos)
			}
			pos, sel = textMoveSelection(sel, pos, target, textSelection)
		case KeyEnd:
			target := len(text)
			if options.multiline {
				target = textLineEnd(text, pos)
			}
			pos, sel = textMoveSelection(sel, pos, target, textSelection)
		case KeyUp, KeyDown, KeyPageUp, KeyPageDown:
			if options.multiline {
				direction, rows := -1, 1
				if event.key == KeyDown || event.key == KeyPageDown {
					direction = 1
				}
				if event.key == KeyPageUp || event.key == KeyPageDown {
					rows = max(1, options.pageRows)
				}
				target := textMoveVertical(text, pos, direction, rows)
				pos, sel = textMoveSelection(sel, pos, target, textSelection)
			}
		case KeyBackspace:
			if options.readOnly {
				continue
			}
			var deleted bool
			text, pos, sel, deleted = textDeleteKey(
				text, pos, sel, KeyBackspace, false, options.secure,
			)
			changed = changed || deleted
		case KeyDelete:
			if options.readOnly {
				continue
			}
			var deleted bool
			text, pos, sel, deleted = textDeleteKey(
				text, pos, sel, KeyDelete, false, options.secure,
			)
			changed = changed || deleted
		case KeyEnter:
			if options.multiline && !options.readOnly {
				var inserted bool
				text, pos, inserted = insertText(text, pos, sel, "\n", textLimit(buf, options.maxCodepoints))
				changed = changed || inserted
				sel = collapsedSelection(pos)
			} else if commit != nil {
				*commit = true
			}
		}
	}
	if len(r.inputEvents) > 0 {
		r.inputEvents = nil
	}
	var composed bool
	if options.readOnly {
		r.ClearTextComposition()
	} else {
		text, pos, sel, composed = r.editComposition(focusID, text, pos, sel, textLimit(buf, options.maxCodepoints))
	}
	changed = changed || composed
	if !options.readOnly {
		clear(buf)
		copy(buf, text)
	}
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
	bounds = r.scrollClip(bounds)
	if r.contentDisabled() {
		return 0, false
	}
	for i := range r.taps {
		if r.taps[i].consumed || r.popupCaptures(r.taps[i].x, r.taps[i].y) {
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

func anyInt32(v any) (int32, bool) {
	switch n := v.(type) {
	case int:
		return int32(n), true
	case int32:
		return n, true
	case int64:
		return int32(n), true
	case uint:
		return int32(n), true
	case uint32:
		return int32(n), true
	}
	return 0, false
}

func dropdownSelected(rest ...any) *int32 {
	for _, arg := range rest {
		if p, ok := arg.(*int32); ok {
			return p
		}
	}
	return nil
}

func selectedLabel(labels []string, selected *int32) string {
	if len(labels) == 0 {
		return ""
	}
	index := int32(0)
	if selected != nil {
		index = clamp32(*selected, 0, int32(len(labels)-1))
	}
	return labels[index]
}

func sliderSuffix(rest ...any) string {
	for _, arg := range rest {
		if s, ok := arg.(string); ok {
			return s
		}
	}
	return ""
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
	if len(order) == 0 {
		return 0
	}
	index := -1
	for i, id := range order {
		if id == current {
			index = i
			break
		}
	}
	if index < 0 {
		if reverse {
			return order[len(order)-1]
		}
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
		return collapsedSelection(pos)
	}
	s.Anchor = clampCursor(text, s.Anchor)
	s.Cursor = clampCursor(text, s.Cursor)
	return s
}

func selectionRange(sel selection) (int, int) {
	result := TextInput_TextSelectionRangeFor(int32(sel.Anchor), int32(sel.Cursor))
	return int(result.Start), int(result.End)
}

func collapsedSelection(pos int) selection {
	result := TextInput_TextSelectionCollapsed(int32(pos))
	return selection{Anchor: int(result.Anchor), Cursor: int(result.Cursor)}
}

func selectAllSelection(length int) selection {
	result := TextInput_TextSelectionAll(int32(length))
	return selection{Anchor: int(result.Anchor), Cursor: int(result.Cursor)}
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
	return int(TextInput_TextInputBufferLimit(int32(len(buf)), maxCodepoints))
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

// CString returns the text before the first NUL byte in a generated fixed
// char buffer.
func CString(buf []byte) string {
	return string(buf[:zeroIndex(buf)])
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

func textCodepointAt(text string, pos int) rune {
	pos = clampCursor(text, pos)
	if pos >= len(text) {
		return 0
	}
	codepoint, _ := utf8.DecodeRuneInString(text[pos:])
	return codepoint
}

func textIsBlank(codepoint rune) bool {
	return codepoint == ' ' || codepoint == '\t' || codepoint == '\u3000'
}

func textIsSeparator(codepoint rune) bool {
	switch codepoint {
	case ',', '\u3001', '.', '\u3002', ';', '\uff1b',
		'(', '\uff08', ')', '\uff09', '{', '\uff5b', '}', '\uff5d',
		'[', '\u300c', ']', '\u300d', '|', '\uff5c', '!', '\uff01',
		'\\', '\uffe5', '/', '\u30fb', '\uff0f', '\n', '\r':
		return true
	default:
		return false
	}
}

func textIsWordBoundary(text string, pos int) bool {
	if pos <= 0 {
		return false
	}
	previous := textCodepointAt(text, prevRune(text, pos))
	current := textCodepointAt(text, pos)
	previousBlank := textIsBlank(previous)
	previousSeparator := textIsSeparator(previous)
	currentBlank := textIsBlank(current)
	currentSeparator := textIsSeparator(current)
	return ((previousBlank || previousSeparator) &&
		!(currentSeparator || currentBlank)) ||
		(currentSeparator && !previousSeparator)
}

func textWordLeft(text string, pos int) int {
	pos = prevRune(text, pos)
	for pos > 0 && !textIsWordBoundary(text, pos) {
		pos = prevRune(text, pos)
	}
	return pos
}

func textWordRight(text string, pos int) int {
	pos = nextRune(text, pos)
	for pos < len(text) && !textIsWordBoundary(text, pos) {
		pos = nextRune(text, pos)
	}
	return pos
}

func textDeleteKey(
	text string,
	pos int,
	current selection,
	key int32,
	word bool,
	secure bool,
) (string, int, selection, bool) {
	start, end := selectionRange(current)
	if start == end {
		switch key {
		case KeyBackspace:
			if word && secure {
				start = 0
			} else if word {
				start = textWordLeft(text, pos)
			} else {
				start = prevRune(text, pos)
			}
			end = pos
		case KeyDelete:
			start = pos
			if word && secure {
				end = len(text)
			} else if word {
				end = textWordRight(text, pos)
			} else {
				end = nextRune(text, pos)
			}
		default:
			return text, pos, current, false
		}
	}
	if end <= start {
		return text, pos, current, false
	}
	text = text[:start] + text[end:]
	current = collapsedSelection(start)
	return text, start, current, true
}

func textLineStart(text string, pos int) int {
	pos = clampCursor(text, pos)
	if start := strings.LastIndexByte(text[:pos], '\n'); start >= 0 {
		return start + 1
	}
	return 0
}

func textLineEnd(text string, pos int) int {
	pos = clampCursor(text, pos)
	if end := strings.IndexByte(text[pos:], '\n'); end >= 0 {
		return pos + end
	}
	return len(text)
}

func textMoveVertical(text string, pos, direction, rows int) int {
	start := textLineStart(text, pos)
	column := utf8.RuneCountInString(text[start:clampCursor(text, pos)])
	for step := 0; step < max(1, rows); step++ {
		if direction < 0 {
			if start == 0 {
				break
			}
			pos = start - 1
			start = textLineStart(text, pos)
		} else {
			end := textLineEnd(text, start)
			if end == len(text) {
				break
			}
			start = end + 1
		}
	}
	pos = start
	end := textLineEnd(text, start)
	for column > 0 && pos < end {
		pos = nextRune(text, pos)
		column--
	}
	return pos
}

func textMoveSelection(current selection, cursor, target int, extend bool) (int, selection) {
	next := TextInput_TextSelectionAfterMove(
		int32(current.Anchor), int32(cursor), int32(target), extend,
	)
	return int(next.Cursor), selection{Anchor: int(next.Anchor), Cursor: int(next.Cursor)}
}

func (r *runtime) recordListBoxOps(props ListBoxProps, rowH int32) int32 {
	defaultItemFrame := listBoxItemMetricFrame(props.ClassName, props.Disabled)
	rowH = ListBox_ListBoxRowHeight(rowH, 1, defaultItemFrame)
	changed := int32(0)
	focused := !props.Disabled && props.ID != 0 && r.focusID == props.ID && !r.popupFocusCaptures(props.ID)
	listFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
		if props.Disabled {
			return ButtonStateDisabled
		}
		if focused {
			return ButtonStateFocus
		}
		return ButtonStateNormal
	}(), props.Disabled, false, props.ClassName, StyleSheet_StyleKindListBox(), StyleSheet_StyleAny())
	listOp := styleFrameRectOp(props.Bounds, Rectangle{}, listFrame)
	listOp.ID = props.ID
	listOp.Disabled = props.Disabled
	listOp.Focused = focused
	r.record(listOp)
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	layout := ListBox_ListBoxLayoutFor(props.Bounds, int32(len(props.Items)), rowH, 0, scroll, 1, defaultItemFrame)
	first := layout.FirstRow
	visible := layout.VisibleRows
	for i := int32(0); i <= visible && first+i < int32(len(props.Items)); i++ {
		index := first + i
		row := ListBox_ListBoxRowBounds(props.Bounds, i, layout)
		if !props.Disabled && props.SelectedIndex != nil && r.consumeTap(row) {
			if *props.SelectedIndex != index {
				*props.SelectedIndex = index
				changed = 1
			}
			if props.ID != 0 {
				r.setFocus(props.ID)
			}
		}
		selected := props.SelectedIndex != nil && *props.SelectedIndex == index
		hovered := !props.Disabled && pointInRect(r.mousePos.X, r.mousePos.Y, row)
		itemFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, func() ButtonState {
			if props.Disabled {
				return ButtonStateDisabled
			}
			if hovered {
				return ButtonStateHover
			}
			if selected {
				return ButtonStateSelected
			}
			return ButtonStateNormal
		}(), props.Disabled, selected, props.ClassName, StyleSheet_StyleKindListBoxItem(), StyleSheet_StyleAny())
		itemStyle := unpackStyle(itemFrame.Value)
		if selected || hovered || props.Disabled {
			op := styleFrameRectOp(row, props.Bounds, itemFrame)
			op.ID = props.ID
			op.Row = index
			op.Selected = selected
			op.Hovered = hovered
			op.Disabled = props.Disabled
			r.record(op)
		}
		labelX := itemStyle.PaddingX
		if labelX <= 0 {
			labelX = 8
		}
		labelY := itemStyle.PaddingY
		if labelY <= 0 {
			labelY = 4
		}
		font, fontID := styleTextFace(itemStyle, Text16)
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + labelX, Y: row.Y + labelY, Width: row.Width - labelX*2, Height: row.Height - labelY*2}, Text: elideTextWithFont(props.Items[index], row.Width-labelX*2, font, fontID), Color: itemStyle.Foreground, Opacity: itemStyle.Opacity, FontSize: font, FontID: fontID, ID: props.ID, Row: index, Selected: selected, Disabled: props.Disabled})
	}
	return changed
}

func listBoxItemMetricFrame(className int32, disabled bool) StyleFrame {
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	}
	return simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false,
		className, StyleSheet_StyleKindListBoxItem(), StyleSheet_StyleAny())
}

func listBoxMultiItemMetricFrame(className int32, disabled bool) StyleFrame {
	state := ButtonStateNormal
	if disabled {
		state = ButtonStateDisabled
	}
	return simpleStyleFrameWithClassRole(ButtonToneNeutral, state, disabled, false,
		className, StyleSheet_StyleKindListBoxMultiItem(), StyleSheet_StyleAny())
}

func normalizeTableViewProps(props TableViewProps) TableViewProps {
	if props.ColumnCount > 0 && int(props.ColumnCount) < len(props.Columns) {
		props.Columns = props.Columns[:props.ColumnCount]
	}
	if props.RowCount > 0 && int(props.RowCount) < len(props.Rows) {
		props.Rows = props.Rows[:props.RowCount]
	}
	if len(props.ColumnWidths) > len(props.Columns) {
		props.ColumnWidths = props.ColumnWidths[:len(props.Columns)]
	}
	if len(props.ColumnEnabled) > len(props.Columns) {
		props.ColumnEnabled = props.ColumnEnabled[:len(props.Columns)]
	}
	if len(props.ColumnOrder) > len(props.Columns) {
		props.ColumnOrder = props.ColumnOrder[:len(props.Columns)]
	}
	for i := range props.Rows {
		if props.Rows[i].CellCount > 0 && int(props.Rows[i].CellCount) < len(props.Rows[i].Cells) {
			props.Rows[i].Cells = props.Rows[i].Cells[:props.Rows[i].CellCount]
		}
	}
	return props
}

func normalizeListBoxProps(props ListBoxProps) ListBoxProps {
	if props.ItemCount > 0 && int(props.ItemCount) < len(props.Items) {
		props.Items = props.Items[:props.ItemCount]
	}
	return props
}

func (r *runtime) TableCellScope(props TableViewProps, row, col int32) Rectangle {
	props = normalizeTableViewProps(props)
	cell := TableCellRect(props, row, col)
	metrics := tableViewMetrics(props)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), props.RowHeight, props.HeaderHeight, props.FreezeRows, 1, metrics)
	viewport := TableView_TableViewViewport(props.Bounds, layout, row >= layout.FrozenRows)
	left, right := max(cell.X, props.Bounds.X), min(cell.X+cell.Width, props.Bounds.X+props.Bounds.Width)
	top, bottom := max(viewport.Y, cell.Y), min(viewport.Y+viewport.Height, cell.Y+cell.Height)
	clip := Rectangle{X: left, Y: top, Width: max(float32(0), right-left), Height: max(float32(0), bottom-top)}
	r.DisabledScope(props.Disabled)
	r.ScrollScope(clip, int32(clip.Height), nil)
	return cell
}

func (r *runtime) TableCellEndScope() { r.ScrollEndScope(); r.DisabledEndScope() }

func TableCellRect(props TableViewProps, row, col int32) Rectangle {
	props = normalizeTableViewProps(props)
	if len(props.Columns) == 0 || row < 0 || col < 0 || int(row) >= len(props.Rows) || int(col) >= len(props.Columns) {
		return Rectangle{}
	}
	metrics := tableViewMetrics(props)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), props.RowHeight, props.HeaderHeight, props.FreezeRows, 1, metrics)
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	x := props.Bounds.X
	found := false
	for _, logical := range tableDisplayColumns(props) {
		if logical == col {
			found = true
			break
		}
		x += float32(tableColumnWidth(props, logical))
	}
	if !found {
		return Rectangle{}
	}
	scrollLayout := TableView_TableViewScrollFor(scroll, layout.FrozenRows, layout.RowHeight, layout.ScrollBodyHeight)
	drawIndex := row
	if row >= layout.FrozenRows {
		drawIndex = layout.FrozenRows + row - scrollLayout.First
	}
	rowBounds := TableView_TableViewRowBounds(props.Bounds, layout, row, drawIndex, scrollLayout, row >= layout.FrozenRows)
	return TableView_TableViewCellBounds(rowBounds, int32(x), tableColumnWidth(props, col))
}

func (r *runtime) handleTableKeys(props TableViewProps) int32 {
	if props.SelectedRow == nil {
		return 0
	}
	changed := int32(0)
	handled := false
	selectionChanged := false
	selectedRow := *props.SelectedRow
	selectedCol := int32(-1)
	if props.SelectedColumn != nil {
		selectedCol = *props.SelectedColumn
	}
	row := int32(0)
	if selectedRow >= 0 {
		row = clamp32(selectedRow, 0, int32(len(props.Rows)-1))
	}
	displayColumns := tableDisplayColumns(props)
	if len(displayColumns) == 0 {
		return 0
	}
	colIndex := 0
	for i, candidate := range displayColumns {
		if candidate == selectedCol {
			colIndex = i
			break
		}
	}
	col := displayColumns[colIndex]
	for _, event := range r.inputEvents {
		if event.text != "" {
			continue
		}
		if event.shortcut {
			switch event.key {
			case KeyC, KeyX:
				if text, ok := tableClipboardText(props, selectedRow, selectedCol); ok {
					r.clipboard = text
					handled = true
					changed = 1
				}
			case KeyV:
				if props.PastedText != nil {
					*props.PastedText = r.clipboard
					if props.PastedRow != nil {
						*props.PastedRow = selectedRow
					}
					if props.PastedColumn != nil {
						*props.PastedColumn = selectedCol
					}
					handled = true
					changed = 1
				}
			}
			continue
		}
		switch event.key {
		case KeyUp:
			row = clamp32(row-1, 0, int32(len(props.Rows)-1))
			changed = 1
			selectionChanged = true
		case KeyDown:
			row = clamp32(row+1, 0, int32(len(props.Rows)-1))
			changed = 1
			selectionChanged = true
		case KeyLeft:
			if colIndex > 0 {
				colIndex--
			}
			col = displayColumns[colIndex]
			changed = 1
			selectionChanged = true
		case KeyRight:
			if colIndex < len(displayColumns)-1 {
				colIndex++
			}
			col = displayColumns[colIndex]
			changed = 1
			selectionChanged = true
		case KeyTab:
			if event.shift {
				if colIndex > 0 {
					colIndex--
				} else {
					colIndex = len(displayColumns) - 1
					row = clamp32(row-1, 0, int32(len(props.Rows)-1))
				}
			} else if colIndex < len(displayColumns)-1 {
				colIndex++
			} else {
				colIndex = 0
				row = clamp32(row+1, 0, int32(len(props.Rows)-1))
			}
			col = displayColumns[colIndex]
			changed = 1
			selectionChanged = true
		case KeyEnter, KeyF2:
			if props.ActivatedRow != nil {
				*props.ActivatedRow = row
			}
			if props.ActivatedColumn != nil {
				*props.ActivatedColumn = col
			}
			changed = 1
			selectionChanged = true
		case KeyEscape:
			if *props.SelectedRow >= 0 || props.SelectedColumn != nil && *props.SelectedColumn >= 0 {
				row = -1
				col = -1
				changed = 1
				selectionChanged = true
			}
		}
	}
	if selectionChanged {
		*props.SelectedRow = row
		if props.SelectedColumn != nil {
			*props.SelectedColumn = col
		}
		if row >= 0 {
			r.scrollTableSelectionIntoView(props)
		}
	}
	if handled || changed != 0 {
		r.inputEvents = nil
	}
	return changed
}

func tableClipboardText(props TableViewProps, row, col int32) (string, bool) {
	if props.CopyText != nil {
		return *props.CopyText, true
	}
	switch {
	case row >= 0 && int(row) < len(props.Rows) && col >= 0:
		return tableCellText(props, row, col), true
	case row >= 0 && int(row) < len(props.Rows):
		cells := make([]string, len(props.Columns))
		for c := range cells {
			cells[c] = tableCellText(props, row, int32(c))
		}
		return strings.Join(cells, "\t"), true
	case col >= 0 && int(col) < len(props.Columns):
		cells := make([]string, len(props.Rows))
		for r := range cells {
			cells[r] = tableCellText(props, int32(r), col)
		}
		return strings.Join(cells, "\n"), true
	}
	return "", false
}

func setTableSelection(props TableViewProps, startRow, startCol, endRow, endCol int32) int32 {
	changed := int32(0)
	if props.SelectedRow != nil && *props.SelectedRow != endRow {
		*props.SelectedRow = endRow
		changed = 1
	}
	if props.SelectedColumn != nil && *props.SelectedColumn != endCol {
		*props.SelectedColumn = endCol
		changed = 1
	}
	if props.SelectionStartRow != nil && *props.SelectionStartRow != startRow {
		*props.SelectionStartRow = startRow
		changed = 1
	}
	if props.SelectionStartColumn != nil && *props.SelectionStartColumn != startCol {
		*props.SelectionStartColumn = startCol
		changed = 1
	}
	if props.SelectionEndRow != nil && *props.SelectionEndRow != endRow {
		*props.SelectionEndRow = endRow
		changed = 1
	}
	if props.SelectionEndColumn != nil && *props.SelectionEndColumn != endCol {
		*props.SelectionEndColumn = endCol
		changed = 1
	}
	return changed
}

func tableCellText(props TableViewProps, row, col int32) string {
	if row < 0 || int(row) >= len(props.Rows) || col < 0 {
		return ""
	}
	cells := props.Rows[row].Cells
	if int(col) >= len(cells) {
		return ""
	}
	return cells[col]
}

func tableSelectionRange(props TableViewProps) (int32, int32, int32, int32, bool) {
	if props.SelectionStartRow == nil || props.SelectionStartColumn == nil ||
		props.SelectionEndRow == nil || props.SelectionEndColumn == nil {
		return 0, 0, 0, 0, false
	}
	startRow, startCol := *props.SelectionStartRow, *props.SelectionStartColumn
	endRow, endCol := *props.SelectionEndRow, *props.SelectionEndColumn
	if startRow < 0 || startCol < 0 || endRow < 0 || endCol < 0 {
		return 0, 0, 0, 0, false
	}
	if startRow > endRow {
		startRow, endRow = endRow, startRow
	}
	if startCol > endCol {
		startCol, endCol = endCol, startCol
	}
	return startRow, startCol, endRow, endCol, true
}

func tableCellSelected(props TableViewProps, row, col, selectedRow, selectedCol int32) bool {
	if startRow, startCol, endRow, endCol, ok := tableSelectionRange(props); ok {
		return row >= startRow && row <= endRow && col >= startCol && col <= endCol
	}
	return row == selectedRow && col == selectedCol || selectedRow < 0 && col == selectedCol
}

func (r *runtime) scrollTableSelectionIntoView(props TableViewProps) {
	if props.ScrollOffset == nil || props.SelectedRow == nil {
		return
	}
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	bodyH := int32(props.Bounds.Height) - max32(30, props.HeaderHeight)
	if bodyH <= 0 {
		return
	}
	frozenRows := tableFrozenRows(props, rowH, bodyH)
	if *props.SelectedRow < frozenRows {
		return
	}
	viewH := bodyH - frozenRows*rowH
	if viewH <= 0 {
		return
	}
	top := (*props.SelectedRow - frozenRows) * rowH
	bottom := top + rowH
	if top < *props.ScrollOffset {
		*props.ScrollOffset = top
	} else if bottom > *props.ScrollOffset+viewH {
		*props.ScrollOffset = bottom - viewH
	}
	maxScroll := max32(0, (int32(len(props.Rows))-frozenRows)*rowH-viewH)
	*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
}

func tableHeaderShift(props TableViewProps, y float32) float32 {
	a := float64(props.HeaderAngle)
	if a == 0 || math.IsNaN(a) || math.IsInf(a, 0) {
		return 0
	}
	a = math.Max(-89, math.Min(89, a))
	h := float32(max32(30, props.HeaderHeight))
	return -(h - (y - props.Bounds.Y)) / float32(math.Tan(a*math.Pi/180))
}

func (r *runtime) drawTableOps(props TableViewProps, rowH, headerH int32) {
	disabledColor := func(color Color) Color {
		if props.Disabled {
			return r.Fade(color, 0.45)
		}
		return color
	}
	tableState := ButtonStateNormal
	if props.Disabled {
		tableState = ButtonStateDisabled
	}
	surfaceFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, tableState, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewPanelRole())
	cellStyle := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, tableState, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewCellRole()).Value)
	rowFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, tableState, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewRowRole())
	rowStyle := unpackStyle(rowFrame.Value)
	selectedFrame := simpleStyleFrameWithClassRole(ButtonToneAccent, ButtonStateSelected, props.Disabled, true,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewSelectionRole())
	selectedStyle := unpackStyle(selectedFrame.Value)
	dividerFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, tableState, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewDividerRole())
	dividerStyle := unpackStyle(dividerFrame.Value)
	metrics := tableViewMetrics(props)
	tableOp := styleFrameRectOp(props.Bounds, Rectangle{}, surfaceFrame)
	tableOp.Color = disabledColor(tableOp.Color)
	tableOp.Disabled = props.Disabled
	r.record(tableOp)
	displayColumns := tableDisplayColumns(props)
	if len(displayColumns) == 0 {
		return
	}
	selectedRow := int32(-1)
	selectedCol := int32(-1)
	if props.SelectedRow != nil {
		selectedRow = *props.SelectedRow
	}
	if props.SelectedColumn != nil {
		selectedCol = *props.SelectedColumn
	}
	fallbackFont := Text12
	if rowH >= 28 {
		fallbackFont = Text14
	}
	cellFont, cellFontID := styleTextFace(cellStyle, fallbackFont)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), rowH, headerH, props.FreezeRows, 1, metrics)
	for _, col := range displayColumns {
		c := int(col)
		x := props.Bounds.X
		for _, logical := range displayColumns {
			if logical == col {
				break
			}
			x += float32(tableColumnWidth(props, logical))
		}
		rect := TableView_TableViewHeaderBounds(props.Bounds, int32(x), tableColumnWidth(props, col), headerH)
		selected := false
		if selectedRow < 0 && selectedCol == col {
			selected = true
		}
		headerFrame := simpleStyleFrameWithClassRole(func() ButtonTone {
			if selected {
				return ButtonToneAccent
			}
			return ButtonToneNeutral
		}(), func() ButtonState {
			if props.Disabled {
				return ButtonStateDisabled
			}
			if selected {
				return ButtonStateSelected
			}
			return ButtonStateNormal
		}(), props.Disabled, selected, props.ClassName, StyleSheet_StyleKindTableView(), TableView_TableViewHeaderRole())
		headerStyle := unpackStyle(headerFrame.Value)
		headerFont, headerFontID := styleTextFace(headerStyle, fallbackFont)
		shift := tableHeaderShift(props, rect.Y)
		var polygon [4]Vector2
		if shift != 0 {
			polygon = [4]Vector2{{rect.X + shift, rect.Y}, {rect.X + rect.Width + shift, rect.Y}, {rect.X + rect.Width, rect.Y + rect.Height}, {rect.X, rect.Y + rect.Height}}
		}
		headerClip := r.scrollClip(Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: props.Bounds.Width, Height: float32(headerH)})
		headerOp := styleFrameRectOp(rect, props.Bounds, headerFrame)
		headerOp.Polygon = polygon
		headerOp.HasPolygon = shift != 0
		headerOp.Color = disabledColor(headerOp.Color)
		headerOp.Row = -1
		headerOp.Column = col
		headerOp.Selected = selected
		headerOp.Disabled = props.Disabled
		r.record(headerOp)
		if shift != 0 {
			r.ops[len(r.ops)-1].Clip = headerClip
			r.ops[len(r.ops)-1].HasClip = true
		}
		textOp := FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect, metrics.HeaderTextPadX), Text: elideTextWithFont(props.Columns[c], rect.Width-float32(metrics.HeaderTextPadX*2), headerFont, headerFontID), Color: disabledColor(headerStyle.Foreground), Opacity: headerStyle.Opacity, FontSize: headerFont, FontID: headerFontID, Row: -1, Column: col, Disabled: props.Disabled}
		angle := props.HeaderAngle
		if math.IsNaN(float64(angle)) || math.IsInf(float64(angle), 0) {
			angle = 0
		}
		angle = max(float32(-89), min(float32(89), angle))
		if angle != 0 {
			textOp.Polygon = polygon
			textOp.HasPolygon = true
			textOp.Text, textOp.Rotation = props.Columns[c], angle
			textOp.Bounds.X, textOp.Bounds.Y = rect.X+float32(metrics.HeaderTextPadX), rect.Y+float32(metrics.HeaderTextPadX)
			if angle > 0 {
				textOp.Bounds.X += shift
			}
			if angle < 0 {
				textOp.Bounds.Y = rect.Y + rect.Height - float32(metrics.HeaderTextPadX)
			}
		}
		r.record(textOp)
		if angle != 0 {
			r.ops[len(r.ops)-1].Clip = headerClip
			r.ops[len(r.ops)-1].HasClip = true
		}
		if props.Resizable && c < len(props.ColumnWidths) {
			r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: rect.X + rect.Width + shift - 1, Y: rect.Y, Width: -shift, Height: rect.Height}, Color: disabledColor(dividerStyle.Border), Column: col, Disabled: props.Disabled})
			r.ops[len(r.ops)-1].Clip, r.ops[len(r.ops)-1].HasClip = headerClip, true
		}
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	frozenRows := layout.FrozenRows
	scrollLayout := TableView_TableViewScrollFor(scroll, frozenRows, rowH, layout.ScrollBodyHeight)
	first := scrollLayout.First
	visible := scrollLayout.VisibleRows
	drawRow := func(row int32) {
		clip := TableView_TableViewViewport(props.Bounds, layout, row >= frozenRows)
		start := len(r.ops)
		defer func() {
			for i := start; i < len(r.ops); i++ {
				r.ops[i].Clip = r.scrollClip(clip)
				r.ops[i].HasClip = true
			}
		}()
		rowY := TableCellRect(props, row, displayColumns[0]).Y
		rowRect := Rectangle{X: props.Bounds.X, Y: rowY, Width: props.Bounds.Width, Height: float32(rowH)}
		if row%2 == 1 {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: rowRect, Color: disabledColor(rowStyle.Background), BorderColor: disabledColor(rowStyle.Border), Radius: rowStyle.Radius, BorderWidth: rowStyle.BorderWidth, Material: rowStyle.Material, Opacity: rowStyle.Opacity, Row: row, Disabled: props.Disabled})
		}
		if row == selectedRow && selectedCol < 0 {
			op := styleFrameRectOp(rowRect, props.Bounds, selectedFrame)
			op.Color = disabledColor(op.Color)
			op.Row = row
			op.Column = -1
			op.Selected = true
			op.Disabled = props.Disabled
			r.record(op)
		}
		for _, col := range displayColumns {
			c := int(col)
			rect := TableCellRect(props, row, col)
			if rect.Y+rect.Height < props.Bounds.Y+float32(headerH) || rect.Y > props.Bounds.Y+props.Bounds.Height {
				continue
			}
			cellTextColor := cellStyle.Foreground
			selectedCell := tableCellSelected(props, row, col, selectedRow, selectedCol)
			if selectedCell {
				op := styleFrameRectOp(rect, props.Bounds, selectedFrame)
				op.Color = disabledColor(op.Color)
				op.Row = row
				op.Column = col
				op.Selected = true
				op.Disabled = props.Disabled
				op.SelectionStartRow = valueOr32(props.SelectionStartRow, -1)
				op.SelectionStartCol = valueOr32(props.SelectionStartColumn, -1)
				op.SelectionEndRow = valueOr32(props.SelectionEndRow, -1)
				op.SelectionEndCol = valueOr32(props.SelectionEndColumn, -1)
				r.record(op)
				cellTextColor = selectedStyle.Foreground
			}
			text := ""
			if int(row) < len(props.Rows) && c < len(props.Rows[row].Cells) {
				text = props.Rows[row].Cells[c]
			}
			textOpacity := cellStyle.Opacity
			textFont := cellFont
			textFontID := cellFontID
			if selectedCell {
				textOpacity = selectedStyle.Opacity
				textFont, textFontID = styleTextFaceWithFallback(selectedStyle, cellFont, cellFontID)
			}
			r.record(FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect, metrics.HeaderTextPadX), Text: elideTextWithFont(text, rect.Width-float32(metrics.HeaderTextPadX*2), textFont, textFontID), Color: disabledColor(cellTextColor), Opacity: textOpacity, FontSize: textFont, FontID: textFontID, Row: row, Column: col, Disabled: props.Disabled})
		}
	}
	for row := int32(0); row < frozenRows; row++ {
		drawRow(row)
	}
	for i := int32(0); i < visible && first+i < int32(len(props.Rows)); i++ {
		drawRow(first + i)
	}
}

func tableTextBounds(rect Rectangle, inset int32) Rectangle {
	pad := float32(inset)
	return Rectangle{X: rect.X + pad, Y: rect.Y + pad, Width: rect.Width - pad*2, Height: rect.Height - pad - 2}
}

func tableCellAt(props TableViewProps, body Rectangle, rowH int32, x, y float32) (int32, int32) {
	props = normalizeTableViewProps(props)
	if rowH <= 0 || len(props.Columns) == 0 {
		return -1, -1
	}
	metrics := tableViewMetrics(props)
	layout := TableView_TableViewLayoutFor(props.Bounds, int32(len(props.Rows)), rowH, props.HeaderHeight, props.FreezeRows, 1, metrics)
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	frozenRows := layout.FrozenRows
	frozenHeight := float32(frozenRows * layout.RowHeight)
	localY := y - body.Y
	row := int32(0)
	if localY < frozenHeight {
		row = int32(localY / float32(layout.RowHeight))
	} else {
		row = frozenRows + int32((localY-frozenHeight+float32(scroll))/float32(layout.RowHeight))
	}
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
	props = normalizeTableViewProps(props)
	if len(props.Columns) == 0 {
		return -1
	}
	cursor := props.Bounds.X
	for _, col := range tableDisplayColumns(props) {
		w := float32(tableColumnWidth(props, col))
		if x >= cursor && x < cursor+w {
			return col
		}
		cursor += w
	}
	return -1
}

func tableColumnWidth(props TableViewProps, col int32) int32 {
	props = normalizeTableViewProps(props)
	if col < 0 || int(col) >= len(props.Columns) {
		return 0
	}
	if int(col) < len(props.ColumnWidths) && props.ColumnWidths[col] > 0 {
		return props.ColumnWidths[col]
	}
	visible := len(tableDisplayColumns(props))
	if visible == 0 {
		return 0
	}
	return TableView_TableViewDefaultColumnWidth(int32(props.Bounds.Width), int32(visible))
}

func tableFrozenRows(props TableViewProps, rowH, bodyHeight int32) int32 {
	return TableView_TableViewFrozenRows(props.FreezeRows, int32(len(props.Rows)), bodyHeight, rowH)
}

func tableSeparatorAtX(props TableViewProps, x, tolerance float32) (int32, float32) {
	cursor := props.Bounds.X
	for _, column := range tableDisplayColumns(props) {
		cursor += float32(tableColumnWidth(props, column))
		if x >= cursor-tolerance && x <= cursor+tolerance {
			return column, cursor
		}
	}
	return -1, 0
}

func tableDisplayColumns(props TableViewProps) []int32 {
	count := len(props.Columns)
	if count == 0 {
		return nil
	}
	enabled := func(col int) bool {
		return col >= len(props.ColumnEnabled) || props.ColumnEnabled[col] != 0
	}
	columns := make([]int32, 0, count)
	seen := make([]bool, count)
	for _, requested := range props.ColumnOrder {
		col := int(requested)
		if col >= 0 && col < count && !seen[col] {
			seen[col] = true
			if enabled(col) {
				columns = append(columns, requested)
			}
		}
	}
	for col := 0; col < count; col++ {
		if !seen[col] && enabled(col) {
			columns = append(columns, int32(col))
		}
	}
	return columns
}

func elideText(text string, maxWidth float32, font int32) string {
	return elideTextWithFont(text, maxWidth, font, 0)
}

func elideTextWithFont(text string, maxWidth float32, font int32, fontID uint32) string {
	if maxWidth <= 0 {
		return ""
	}
	runes := []rune(text)
	if runtimeTextWidthWithFont(text, font, fontID) <= int(maxWidth) {
		return text
	}
	ellipsis := "…"
	if runtimeTextWidthWithFont(ellipsis, font, fontID) > int(maxWidth) {
		return ""
	}
	for i := len(runes) - 1; i > 0; i-- {
		short := string(runes[:i]) + ellipsis
		if runtimeTextWidthWithFont(short, font, fontID) <= int(maxWidth) {
			return short
		}
	}
	return ellipsis
}

func runtimeTextWidth(text string, font int32) int {
	return runtimeTextWidthWithFont(text, font, 0)
}

func runtimeTextWidthWithFont(text string, font int32, fontID uint32) int {
	if text == "" {
		return 0
	}
	return int(MeasureTextEx(Font{ID: fontID}, text, float32(font), 1).X)
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
