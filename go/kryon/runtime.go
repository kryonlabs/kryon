package kryon

import (
	"fmt"
	"hash/fnv"
	"log"
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
type MenuItemKind int32
type UISemanticKind int32

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
	ThemeStyleSystem ThemeStyle = iota
	ThemeStyleRetro
	ThemeStyleMaterial
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

	THEME_STYLE_SYSTEM   = 0
	THEME_STYLE_RETRO    = 1
	THEME_STYLE_MATERIAL = 2
	THEME_SOURCE_APP     = 0
	THEME_SOURCE_SYSTEM  = 1
	THEME_MODE_SYSTEM    = 0
	THEME_MODE_LIGHT     = 1
	THEME_MODE_DARK      = 2

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

	PICTURE_FIT_STRETCH = PictureFitStretch
	PICTURE_FIT_CONTAIN = PictureFitContain
	PICTURE_FIT_COVER   = PictureFitCover
)

const (
	UISemanticNone UISemanticKind = iota
	UISemanticPage
	UISemanticSection
	UISemanticHeading
	UISemanticParagraph
	UISemanticLink
	UISemanticPicture
	UISemanticButton

	UI_SEMANTIC_NONE      = UISemanticNone
	UI_SEMANTIC_PAGE      = UISemanticPage
	UI_SEMANTIC_SECTION   = UISemanticSection
	UI_SEMANTIC_HEADING   = UISemanticHeading
	UI_SEMANTIC_PARAGRAPH = UISemanticParagraph
	UI_SEMANTIC_LINK      = UISemanticLink
	UI_SEMANTIC_PICTURE   = UISemanticPicture
	UI_SEMANTIC_BUTTON    = UISemanticButton
)

const (
	MenuCommand MenuItemKind = iota
	MenuCheck
	MenuRadio
	MenuSeparator
	MenuSubmenu
)

const (
	UIIconTypeNone = iota
	UIIconTypeActivity
	UIIconTypeAmen
	UIIconTypeBackward
	UIIconTypeC
	UIIconTypeCalendar
	UIIconTypeCheck
	UIIconTypeEdit
	UIIconTypeEye
	UIIconTypeEyeOff
	UIIconTypeFingerprint
	UIIconTypeForward
	UIIconTypeGear
	UIIconTypeGlobe
	UIIconTypeHome
	UIIconTypeJupiter
	UIIconTypeKryon
	UIIconTypeLeft
	UIIconTypeLightoff
	UIIconTypeLighton
	UIIconTypeLink
	UIIconTypeManual
	UIIconTypeMars
	UIIconTypeMercury
	UIIconTypeMoon
	UIIconTypeMusic
	UIIconTypeMute
	UIIconTypePause
	UIIconTypePencil
	UIIconTypePet
	UIIconTypePlay
	UIIconTypePlus
	UIIconTypeProfile
	UIIconTypeReturn
	UIIconTypeRight
	UIIconTypeRocket
	UIIconTypeRoutine
	UIIconTypeSaturn
	UIIconTypeSave
	UIIconTypeSound
	UIIconTypeSound0
	UIIconTypeSound1
	UIIconTypeSound2
	UIIconTypeSound3
	UIIconTypeStack
	UIIconTypeStat
	UIIconTypeSun
	UIIconTypeText
	UIIconTypeTimeline
	UIIconTypeTodos
	UIIconTypeTrash
	UIIconTypeVenus
	UIIconTypeWeekly
	UIIconTypeWrench
	UIIconTypeX
	UIIconTypeLanguageRay
	UIIconTypeLanguageTcl
	UIIconTypeLanguageUxn
	UIIconTypeLanguageWasm
	UIIconTypeLanguageWasm4
	UIIconTypePaymentsBtc
	UIIconTypePaymentsMonero
	UIIconTypePaymentsStripe
	UIIconTypePfpBambus
	UIIconTypePfpBird
	UIIconTypePfpBowl
	UIIconTypePfpBush
	UIIconTypePfpButterfly
	UIIconTypePfpCactus
	UIIconTypePfpCoffee
	UIIconTypePfpDragonfly
	UIIconTypePfpFireplace
	UIIconTypePfpFlower1
	UIIconTypePfpFlower2
	UIIconTypePfpFox
	UIIconTypePfpHeart
	UIIconTypePfpIncense
	UIIconTypePfpLotus
	UIIconTypePfpMountain
	UIIconTypePfpMushroom
	UIIconTypePfpPalm
	UIIconTypePfpPerson1
	UIIconTypePfpRainbow
	UIIconTypePfpTent
	UIIconTypePfpTree1
	UIIconTypePfpTree2
	UIIconTypePfpTree3
	UIIconTypePfpTree4
	UIIconTypePlatformsAppimage
	UIIconTypePlatformsBrowser
	UIIconTypePlatformsChromewebstore
	UIIconTypePlatformsDebian
	UIIconTypePlatformsDiscord
	UIIconTypePlatformsDroid
	UIIconTypePlatformsEsp32
	UIIconTypePlatformsFdroid
	UIIconTypePlatformsFedora
	UIIconTypePlatformsFlatpak
	UIIconTypePlatformsFreebsd
	UIIconTypePlatformsGithub
	UIIconTypePlatformsGlenda
	UIIconTypePlatformsIos
	UIIconTypePlatformsItch
	UIIconTypePlatformsMacos
	UIIconTypePlatformsMicrocontroller
	UIIconTypePlatformsPlaystore
	UIIconTypePlatformsSnap
	UIIconTypePlatformsSrht
	UIIconTypePlatformsTelegram
	UIIconTypePlatformsTux
	UIIconTypePlatformsWin
	UIIconTypeProjInbe
	UIIconTypeProjKryon
	UIIconTypeProjWao
	UIIconTypeTilesTile
	UIIconTypeTilesTile2
	UIIconTypeTilesTile3
	UIIconTypeTilesTile4
	UIIconTypeWorkbookClearFormatting
	UIIconTypeWorkbookFillColor
	UIIconTypeWorkbookTextColor
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

type UIThemeSettingsState struct {
	DrawSourceMenu  int32
	DrawModeMenu    int32
	DrawPaletteMenu int32
	DrawStyleMenu   int32
	PaletteIndex    int32
}

type ThemeSettingsProps struct {
	IdBase                int32
	X, Y, W               int32
	ThemeSource           *int32
	ThemeMode             *int32
	ThemeId               *int32
	ThemeStyle            *int32
	AllowSystemSource     int32
	AllowSystemMode       int32
	ThemeLabel            string
	SourceAppLabel        string
	SourceSystemLabel     string
	ModeLabel             string
	ModeSystemLabel       string
	ModeLightLabel        string
	ModeDarkLabel         string
	PaletteLabel          string
	StyleLabel            string
	StyleSystemLabel      string
	StyleRetroLabel       string
	StyleMaterialLabel    string
	StyleFluentLabel      string
	StyleAdwaitaLabel     string
	StyleLiquidGlassLabel string
	SystemThemeLabel      string
}

type UIThemeSettingsResult struct {
	Changed        int32
	SourceChanged  int32
	ModeChanged    int32
	PaletteChanged int32
	StyleChanged   int32
}

type ButtonProps struct {
	Bounds   Rectangle
	Label    string
	Style    ButtonStyle
	Font     int32
	ID       int32
	Disabled bool
}

type SelectableProps struct {
	Bounds   Rectangle
	ID       int32
	Label    string
	Selected *int32
	Disabled bool
}

type CheckboxFlagsProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Flags      *int32
	FlagsValue int32
	Disabled   bool
}

type ImageWithBgProps struct {
	Picture    PictureProps
	Background Color
}

type ImageButtonProps struct {
	Picture    PictureProps
	Background Color
	ID         int32
	Disabled   bool
}

type InvisibleButtonProps struct {
	Bounds   Rectangle
	ID       int32
	Disabled bool
}

type SeparatorTextProps struct {
	Bounds   Rectangle
	Label    string
	Font     int32
	Disabled bool
}

type ArrowDirection int32

const (
	ArrowLeft ArrowDirection = iota
	ArrowRight
	ArrowUp
	ArrowDown
)

type ArrowButtonProps struct {
	Bounds    Rectangle
	ID        int32
	Direction ArrowDirection
	Disabled  bool
}

type ColorEditProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float32
	ValueCount int32
	Disabled   bool
}

type ColorButtonProps struct {
	Bounds   Rectangle
	ID       int32
	Label    string
	Color    Color
	Disabled bool
}

type TooltipProps struct {
	Trigger  Rectangle
	Text     string
	Font     int32
	MaxWidth int32
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

type PageProps struct {
	Bounds       Rectangle
	Title        string
	Description  string
	CanonicalURL string
	ThemeColor   Color
	Background   Color
	Gap          int32
	Padding      int32
	Key          KeyID
}

type SectionProps struct {
	Bounds  Rectangle
	Label   string
	Gap     int32
	Padding int32
	Key     KeyID
}

type HeadingProps struct {
	Bounds Rectangle
	Text   string
	Level  int32
	Font   int32
	Color  Color
	Key    KeyID
}

type ParagraphTextProps struct {
	Bounds  Rectangle
	Text    string
	Font    int32
	Color   Color
	LineGap int32
	Key     KeyID
}

type LinkProps struct {
	Bounds     Rectangle
	Text       string
	Href       string
	Font       int32
	FocusID    int32
	Disabled   bool
	Color      Color
	HoverColor Color
}

type FlowProps = ColumnProps

type GridProps struct {
	Bounds  Rectangle
	Columns int32
	Gap     int32
	Padding int32
	Key     KeyID
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
	IconColor      Color
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
	Actions           []ToolbarAction
	ActionCount       int32
}

type ToolbarAction struct {
	Icon     Texture2D
	IconType int32
	Disabled bool
}

type ToolbarResult struct {
	SelectedMenuItem int32
	ClickedAction    int32
}

type MenuItem struct {
	Kind         MenuItemKind
	Label        string
	Accelerator  string
	ID           int32
	Disabled     bool
	Checked      bool
	Submenu      []MenuItem
	SubmenuCount int32
}

type Menu struct {
	Bounds    Rectangle
	Label     string
	Items     []MenuItem
	ItemCount int32
}

type MenuBarResult struct {
	ActivatedID int32
	OpenIndex   int32
}

type ContextMenuProps struct {
	ID        int32
	Trigger   Rectangle
	Items     []MenuItem
	ItemCount int32
	Open      *int32
	X         *int32
	Y         *int32
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

type PlotProps struct {
	Bounds     Rectangle
	Label      string
	Values     []float32
	ValueCount int32
	Offset     int32
	Overlay    string
	ScaleMin   float32
	ScaleMax   float32
}

type DragFloatProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float32
	ValueCount int32
	Speed      float32
	Min        float32
	Max        float32
	Format     string
	Disabled   bool
}

type DragIntProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []int32
	ValueCount int32
	Speed      float32
	Min        int32
	Max        int32
	Format     string
	Disabled   bool
}

type DragFloatRange2Props struct {
	Bounds     Rectangle
	ID         int32
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

type DragIntRange2Props struct {
	Bounds     Rectangle
	ID         int32
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

type SliderFloatProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []float32
	ValueCount int32
	Min        float32
	Max        float32
	Format     string
	Disabled   bool
}

type SliderIntProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Values     []int32
	ValueCount int32
	Min        int32
	Max        int32
	Format     string
	Disabled   bool
}

type SliderAngleProps struct {
	Bounds     Rectangle
	ID         int32
	Label      string
	Value      *float32
	MinDegrees float32
	MaxDegrees float32
	Format     string
	Disabled   bool
}

type InputFloatProps struct {
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

type InputIntProps struct {
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

type InputDoubleProps struct {
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
	OptionCount   int32
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
	ItemCount     int32
	SelectedIndex *int32
	ScrollOffset  *int32
	RowHeight     int32
}

type UITreeItem struct {
	Label      string
	Depth      int32
	ID         int32
	Expanded   int32
	Selectable int32
}

type TreeViewProps struct {
	Bounds       Rectangle
	ID           int32
	Items        []UITreeItem
	ItemCount    int32
	SelectedID   *int32
	ScrollOffset *int32
	RowHeight    int32
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

type TableRow struct {
	Cells            []string
	CellCount        int32
	TextColors       []Color
	BackgroundColors []Color
}

type TableViewProps struct {
	Bounds               Rectangle
	ID                   int32
	Columns              []string
	ColumnCount          int32
	Rows                 []TableRow
	RowCount             int32
	ColumnWidths         []int32
	SelectedRow          *int32
	SelectedColumn       *int32
	SelectionStartRow    *int32
	SelectionStartColumn *int32
	SelectionEndRow      *int32
	SelectionEndColumn   *int32
	ActivatedRow         *int32
	ActivatedColumn      *int32
	RightClickedRow      *int32
	RightClickedColumn   *int32
	CopyText             *string
	PastedText           *string
	PastedRow            *int32
	PastedColumn         *int32
	SortColumn           *int32
	ScrollOffset         *int32
	RowHeight            int32
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
	Selectable(SelectableProps) bool
	CheckboxFlags(CheckboxFlagsProps) bool
	ImageWithBg(ImageWithBgProps)
	ImageButton(ImageButtonProps) bool
	SmallButton(ButtonProps) bool
	InvisibleButton(InvisibleButtonProps) bool
	ArrowButton(ArrowButtonProps) bool
	Bullet(Rectangle)
	Separator(Rectangle, int32)
	SeparatorText(SeparatorTextProps)
	ColorEdit3(ColorEditProps) bool
	ColorEdit4(ColorEditProps) bool
	ColorPicker3(ColorEditProps) bool
	ColorPicker4(ColorEditProps) bool
	ColorButton(ColorButtonProps) bool
	Tooltip(TooltipProps) bool
	TabBar(Rectangle, []string, *int32, *int32) int32
	Progress(ProgressBarProps)
	PlotLines(PlotProps)
	PlotHistogram(PlotProps)
	DragFloat(DragFloatProps) bool
	DragInt(DragIntProps) bool
	DragFloatRange2(DragFloatRange2Props) bool
	DragIntRange2(DragIntRange2Props) bool
	SliderFloat(SliderFloatProps) bool
	SliderInt(SliderIntProps) bool
	VSliderFloat(SliderFloatProps) bool
	VSliderInt(SliderIntProps) bool
	SliderAngle(SliderAngleProps) bool
	InputFloat(InputFloatProps) bool
	InputInt(InputIntProps) bool
	InputDouble(InputDoubleProps) bool
	Checkbox(int32, int32, int32, string, *int32) bool
	Dropdown(id, x, y, w, h int32, options any, rest ...any) bool
	Column(ColumnProps)
	Row(ColumnProps)
	Stack(ColumnProps)
	Screen(ColumnProps)
	GridLayout(GridProps)
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
	PagePicture(PictureProps, string)
	Flow(FlowProps)
	PageGrid(GridProps)
	TextField(TextFieldProps)
	Key(text string) KeyID
	Fade(Color, float32) Color
	GetThemeSurface() Color
	GetThemeButton() Color
	GetThemeButtonHover() Color
	GetThemeLink() Color
	GetThemePrimary() Color
	GetThemeOnPrimary() Color
	GetThemeSurfaceVariant() Color
	GetUIMaterialScheme() MaterialScheme
	TextInRect(text string, rect Rectangle, fontSize int32, color Color)
	TextColored(text string, x, y, fontSize int32, color Color)
	TextDisabled(text string, x, y, fontSize int32)
	TextWrapped(text string, bounds Rectangle, fontSize int32, color Color)
	LabelText(label, value string, bounds Rectangle, fontSize int32, color Color)
	BulletText(text string, bounds Rectangle, fontSize int32, color Color)
	TextLines(lines any, count int32, x int32, y *int32, font, lineH int32, color Color)
	Bevel(x, y, w, h int32, light, dark Color)
	Icon(id, x, y, size int32, iconType int32, tint Color)
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
	Toolbar(props ToolbarProps) ToolbarResult
	MenuBar(id int32, bounds Rectangle, menus []Menu, openIndex *int32) MenuBarResult
	PopupMenu(id, x, y int32, items []MenuItem, itemCount int32) int32
	ContextMenu(props ContextMenuProps) int32
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
	ColorPicker(bounds Rectangle, color *Color) bool
	TreeView(props TreeViewProps) int32
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
	config           AppConfig
	closed           bool
	frames           int
	focusID          int32
	clipboard        string
	inputEvents      []inputEvent
	taps             []tapEvent
	clicks           []mouseClickEvent
	mousePos         Vector2
	mouseWheel       float32
	mouseDown        map[int32]bool
	mousePressed     map[int32]bool
	mouseReleased    map[int32]bool
	keyDown          map[int32]bool
	chars            []rune
	fieldOrder       []int32
	prevOrder        []int32
	focusRefs        map[int32]*bool
	selection        map[int32]selection
	layout           []layoutFrame
	ops              []FrameOp
	pageTitle        string
	pageDescription  string
	pageCanonicalURL string
	pageThemeColor   Color
	routePath        string
	routeHash        string
	routeVersion     int32
	lastTableClick   tableClick
	tableDrag        tableDrag
	openMenus        map[int32]int32
	openSubmenus     map[int32]int32
	contextMenus     map[int32]Vector2
	openDropdowns    map[int32]bool
	selectableText   KeyID
	drag             scalarDrag
	slider           scalarDrag
	numericInputs    map[int32]*numericInputState
	toastMessage     string
	toastUntil       time.Time
	currentThemeID   ThemeId
	themeSource      ThemeSource
	themeMode        ThemeMode
	themeStyle       ThemeStyle
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
	columns    int32
	cellIndex  int32
	rowHeight  float32
	noLayout   bool
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
}

type scalarDrag struct {
	active bool
	token  int32
	lastX  float32
}

type numericInputState struct {
	text    []byte
	cursor  int32
	focused bool
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
		mousePressed:   map[int32]bool{},
		mouseReleased:  map[int32]bool{},
		keyDown:        map[int32]bool{},
		openMenus:      map[int32]int32{},
		openSubmenus:   map[int32]int32{},
		contextMenus:   map[int32]Vector2{},
		openDropdowns:  map[int32]bool{},
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
	r.recordToast()
	r.prevOrder = append(r.prevOrder[:0], r.fieldOrder...)
	r.taps = nil
	r.clicks = nil
	r.mouseWheel = 0
	r.mousePressed = map[int32]bool{}
	r.mouseReleased = map[int32]bool{}
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

func (r *runtime) Selectable(props SelectableProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	selected := props.Selected != nil && *props.Selected != 0
	pressed := !props.Disabled && r.consumeTap(props.Bounds)
	if pressed && props.Selected != nil {
		if selected {
			*props.Selected = 0
		} else {
			*props.Selected = 1
		}
		selected = !selected
	}
	theme := r.theme()
	if selected || pressed {
		fill := theme.button
		if pressed {
			fill = theme.buttonHover
		}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: fill, Disabled: props.Disabled})
	}
	textColor := theme.text
	if props.Disabled {
		textColor = r.Fade(textColor, 0.45)
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X + 8, Y: props.Bounds.Y + 6, Width: props.Bounds.Width - 16, Height: props.Bounds.Height}, Text: props.Label, Color: textColor, FontSize: Text14, ID: props.ID, Disabled: props.Disabled, Pressed: pressed, Selected: selected})
	return pressed
}

func (r *runtime) CheckboxFlags(props CheckboxFlagsProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	checked := props.Flags != nil && (*props.Flags&props.FlagsValue) == props.FlagsValue
	pressed := !props.Disabled && props.Flags != nil && r.consumeTap(props.Bounds)
	if pressed {
		if checked {
			*props.Flags &^= props.FlagsValue
		} else {
			*props.Flags |= props.FlagsValue
		}
		checked = !checked
	}
	theme := r.theme()
	box := Rectangle{X: props.Bounds.X, Y: props.Bounds.Y + (props.Bounds.Height-20)/2, Width: 20, Height: 20}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: box, Color: theme.surface, BorderColor: theme.border, ID: props.ID, Disabled: props.Disabled, Pressed: pressed, Selected: checked})
	if checked {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: box.X + 4, Y: box.Y + 4, Width: 12, Height: 12}, Color: theme.circle})
	}
	textColor := theme.text
	if props.Disabled {
		textColor = r.Fade(textColor, 0.45)
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: box.X + 28, Y: props.Bounds.Y + 5, Width: props.Bounds.Width - 28, Height: props.Bounds.Height}, Text: props.Label, Color: textColor, FontSize: Text14, Disabled: props.Disabled})
	return pressed
}

func (r *runtime) ImageWithBg(props ImageWithBgProps) {
	bounds := r.layoutRect(props.Picture.Bounds)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: props.Background})
	r.record(FrameOp{Kind: FrameOpPicture, Bounds: bounds, Text: props.Picture.AssetPath, Color: props.Picture.Tint})
}

func (r *runtime) ImageButton(props ImageButtonProps) bool {
	bounds := r.layoutRect(props.Picture.Bounds)
	pressed := !props.Disabled && r.consumeTap(bounds)
	r.record(FrameOp{Kind: FrameOpButton, Bounds: bounds, Color: props.Background, BorderColor: r.theme().border, ID: props.ID, Disabled: props.Disabled, Pressed: pressed})
	r.record(FrameOp{Kind: FrameOpPicture, Bounds: bounds, Text: props.Picture.AssetPath, Color: props.Picture.Tint, Disabled: props.Disabled})
	return pressed
}

func (r *runtime) SmallButton(props ButtonProps) bool {
	if props.Font <= 0 {
		props.Font = Text14
	}
	return r.Button(props)
}

func (r *runtime) InvisibleButton(props InvisibleButtonProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	return !props.Disabled && r.consumeTap(props.Bounds)
}

func (r *runtime) ArrowButton(props ArrowButtonProps) bool {
	label := "<"
	switch props.Direction {
	case ArrowRight:
		label = ">"
	case ArrowUp:
		label = "^"
	case ArrowDown:
		label = "v"
	}
	return r.Button(ButtonProps{Bounds: props.Bounds, Label: label, Font: Text14, ID: props.ID, Disabled: props.Disabled})
}

func (r *runtime) Bullet(bounds Rectangle) {
	bounds = r.layoutRect(bounds)
	size := bounds.Width
	if bounds.Height < size {
		size = bounds.Height
	}
	size *= 0.5
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: bounds.X + (bounds.Width-size)/2, Y: bounds.Y + (bounds.Height-size)/2, Width: size, Height: size}, Color: r.theme().text})
}

func (r *runtime) Separator(bounds Rectangle, vertical int32) {
	bounds = r.layoutRect(bounds)
	if vertical != 0 {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X + bounds.Width/2, Y: bounds.Y, Height: bounds.Height}, Color: r.theme().border})
	} else {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height/2, Width: bounds.Width}, Color: r.theme().border})
	}
}

func (r *runtime) SeparatorText(props SeparatorTextProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	font := props.Font
	if font <= 0 {
		font = Text14
	}
	color := r.theme().text
	lineColor := r.theme().border
	if props.Disabled {
		color = r.Fade(color, 0.45)
		lineColor = r.Fade(lineColor, 0.45)
	}
	lineX := props.Bounds.X
	if props.Label != "" {
		textWidth := float32(runtimeTextWidth(props.Label, font))
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: textWidth, Height: props.Bounds.Height}, Text: props.Label, Color: color, FontSize: font, Disabled: props.Disabled})
		lineX += textWidth + 12
	}
	endX := props.Bounds.X + props.Bounds.Width
	if lineX < endX {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: lineX, Y: props.Bounds.Y + props.Bounds.Height/2, Width: endX - lineX}, Color: lineColor, Disabled: props.Disabled})
	}
}

func (r *runtime) colorEdit(props ColorEditProps, channels int) bool {
	if len(props.Values) < channels || int(props.ValueCount) > 0 && int(props.ValueCount) < channels {
		return false
	}
	return r.sliderFloat(SliderFloatProps{Bounds: props.Bounds, ID: props.ID, Label: props.Label, Values: props.Values[:channels], ValueCount: int32(channels), Min: 0, Max: 1, Format: "%.3f", Disabled: props.Disabled}, false)
}

func colorFromFloats(values []float32, channels int) Color {
	component := [4]float32{0, 0, 0, 1}
	for i := 0; i < channels && i < len(values); i++ {
		component[i] = values[i]
		if component[i] < 0 {
			component[i] = 0
		} else if component[i] > 1 {
			component[i] = 1
		}
	}
	return Color{R: uint8(component[0]*255 + 0.5), G: uint8(component[1]*255 + 0.5), B: uint8(component[2]*255 + 0.5), A: uint8(component[3]*255 + 0.5)}
}

func (r *runtime) colorPickerFloat(props ColorEditProps, channels int) bool {
	if len(props.Values) < channels || int(props.ValueCount) > 0 && int(props.ValueCount) < channels {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	swatchHeight := float32(36)
	gap := float32(4)
	rowHeight := (props.Bounds.Height - swatchHeight - gap) / float32(channels)
	if rowHeight < 20 {
		rowHeight = 28
	}
	changed := false
	for i := 0; i < channels; i++ {
		row := Rectangle{X: props.Bounds.X, Y: props.Bounds.Y + float32(i)*rowHeight, Width: props.Bounds.Width, Height: rowHeight - 2}
		changed = r.sliderFloat(SliderFloatProps{Bounds: row, ID: props.ID*8 + int32(i) + 1, Values: props.Values[i : i+1], ValueCount: 1, Min: 0, Max: 1, Format: "%.3f", Disabled: props.Disabled}, false) || changed
	}
	swatch := Rectangle{X: props.Bounds.X, Y: props.Bounds.Y + rowHeight*float32(channels) + gap, Width: props.Bounds.Width, Height: swatchHeight}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: swatch, Color: colorFromFloats(props.Values, channels), BorderColor: r.theme().border, Disabled: props.Disabled})
	r.drawSliderLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) ColorEdit3(props ColorEditProps) bool   { return r.colorEdit(props, 3) }
func (r *runtime) ColorEdit4(props ColorEditProps) bool   { return r.colorEdit(props, 4) }
func (r *runtime) ColorPicker3(props ColorEditProps) bool { return r.colorPickerFloat(props, 3) }
func (r *runtime) ColorPicker4(props ColorEditProps) bool { return r.colorPickerFloat(props, 4) }

func (r *runtime) ColorButton(props ColorButtonProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	pressed := !props.Disabled && r.consumeTap(props.Bounds)
	t := r.theme()
	halfW, halfH := props.Bounds.Width/2, props.Bounds.Height/2
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: Color{180, 180, 180, 255}})
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: halfW, Height: halfH}, Color: Color{220, 220, 220, 255}})
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: props.Bounds.X + halfW, Y: props.Bounds.Y + halfH, Width: halfW, Height: halfH}, Color: Color{220, 220, 220, 255}})
	r.record(FrameOp{Kind: FrameOpButton, Bounds: props.Bounds, Text: props.Label, Color: props.Color, BorderColor: t.border, TextColor: t.text, FontSize: Text14, ID: props.ID, Disabled: props.Disabled, Pressed: pressed})
	return pressed
}

func (r *runtime) Tooltip(props TooltipProps) bool {
	if props.Disabled || props.Text == "" || !pointInRect(r.mousePos.X, r.mousePos.Y, props.Trigger) {
		return false
	}
	font := props.Font
	if font <= 0 {
		font = Text14
	}
	maxWidth := props.MaxWidth
	if maxWidth <= 0 {
		maxWidth = 240
	}
	lines := wrapRuntimeText(props.Text, float32(maxWidth), font)
	contentWidth := float32(24)
	for _, line := range lines {
		if width := float32(runtimeTextWidth(line, font)); width > contentWidth {
			contentWidth = width
		}
	}
	if contentWidth > float32(maxWidth) {
		contentWidth = float32(maxWidth)
	}
	panel := Rectangle{X: r.mousePos.X + 12, Y: r.mousePos.Y + 16, Width: contentWidth + 16, Height: float32(len(lines))*float32(font+2) + 14}
	viewWidth, viewHeight := float32(r.GetScreenWidth()), float32(r.GetScreenHeight())
	if panel.X+panel.Width > viewWidth {
		panel.X = viewWidth - panel.Width - 4
	}
	if panel.Y+panel.Height > viewHeight {
		panel.Y = r.mousePos.Y - panel.Height - 8
	}
	theme := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: panel.X + 2, Y: panel.Y + 2, Width: panel.Width, Height: panel.Height}, Color: r.Fade(theme.text, 0.18)})
	r.record(FrameOp{Kind: FrameOpRect, Bounds: panel, Color: theme.surface, BorderColor: theme.border})
	r.TextWrapped(props.Text, Rectangle{X: panel.X + 8, Y: panel.Y + 7, Width: contentWidth, Height: panel.Height - 14}, font, theme.text)
	return true
}

// TabBar renders a horizontal strip of equal-width tabs and returns the
// index clicked this frame (-1 = none). The selected tab (through *selected)
// carries the highlighted styling; *hover tracks the tab under the pointer.
func (r *runtime) TabBar(bounds Rectangle, labels []string, selected, hover *int32) int32 {
	if len(labels) == 0 || selected == nil {
		return -1
	}
	theme := r.theme()
	b := r.layoutRect(bounds)
	if b.Width <= 0 || b.Height <= 0 {
		return -1
	}
	sel := *selected
	if sel < 0 || sel >= int32(len(labels)) {
		sel = 0
		*selected = 0
	}
	mouse := r.MousePosition()
	tw := b.Width / float32(len(labels))
	fontSize := int32(14)
	if b.Height > 12 && fontSize > int32(b.Height)-6 {
		fontSize = int32(b.Height) - 6
	}
	clicked := int32(-1)
	if hover != nil {
		*hover = -1
	}
	for i, label := range labels {
		tab := Rectangle{X: b.X + float32(i)*tw, Y: b.Y, Width: tw, Height: b.Height}
		inside := mouse.X >= tab.X && mouse.X < tab.X+tab.Width &&
			mouse.Y >= tab.Y && mouse.Y < tab.Y+tab.Height
		press := r.consumeTap(tab)
		active := int32(i) == sel
		fill, border, text := theme.surface, theme.button, theme.icon
		if active {
			fill, border, text = theme.buttonHover, theme.buttonHover, theme.text
		} else if inside {
			fill, text = theme.button, theme.text
		}
		if hover != nil && inside {
			*hover = int32(i)
		}
		r.record(FrameOp{Kind: FrameOpButton, Bounds: tab, Text: fitTabLabel(label, tab.Width-12, fontSize),
			Color: fill, BorderColor: border, TextColor: text, FontSize: fontSize, Pressed: active || press})
		if press {
			*selected = int32(i)
			clicked = int32(i)
		}
	}
	return clicked
}

// fitTabLabel truncates with an ellipsis until the label measures within
// maxWidth (rune-safe; measurement falls back to a width estimate when no
// font face is loaded, e.g. headless tests).
func fitTabLabel(label string, maxWidth float32, fontSize int32) string {
	if maxWidth <= 8 {
		return ""
	}
	runes := []rune(label)
	for len(runes) > 1 {
		s := string(runes)
		if w, ok := measureFontText(s, fontSize, 0); ok {
			if w.X <= maxWidth {
				return s
			}
		} else if float32(len(runes))*float32(fontSize)*0.6 <= maxWidth {
			return s
		}
		runes = runes[:len(runes)-1]
		if w, ok := measureFontText(string(runes)+"\u2026", fontSize, 0); ok && w.X <= maxWidth {
			return string(runes) + "\u2026"
		}
	}
	return string(runes)
}
func (r *runtime) Progress(props ProgressBarProps) {
	theme := r.theme()
	bounds := r.layoutRect(props.Bounds)
	minimum := props.Min
	maximum := props.Max
	if maximum <= minimum {
		maximum = minimum + 1
	}
	t := float32(props.Value-minimum) / float32(maximum-minimum)
	if t < 0 {
		t = 0
	} else if t > 1 {
		t = 1
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: mixColor(theme.background, theme.surface, 0.65), BorderColor: theme.button})
	if bounds.Width > 0 && bounds.Height > 0 && t > 0 {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: bounds.X, Y: bounds.Y, Width: bounds.Width * t, Height: bounds.Height}, Color: theme.buttonHover, Selected: true})
	}
	if props.Label != "" {
		font := Text14
		labelW := float32(runtimeTextWidth(props.Label, font))
		fillW := bounds.Width * t
		fillEnd := bounds.X + fillW
		x := bounds.X + (bounds.Width-labelW)/2
		textColor := theme.text
		pad := float32(6)
		emptyW := bounds.Width - fillW
		if emptyW >= labelW+pad*2 {
			x = fillEnd + pad
		} else if fillW >= labelW+pad*2 {
			x = fillEnd - labelW - pad
			textColor = theme.background
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: x, Y: bounds.Y + (bounds.Height-float32(font))/2, Width: labelW, Height: float32(font)}, Text: props.Label, Color: textColor, FontSize: font})
	}
}

func (r *runtime) PlotLines(props PlotProps)     { r.plot(props, false) }
func (r *runtime) PlotHistogram(props PlotProps) { r.plot(props, true) }
func (r *runtime) plot(props PlotProps, histogram bool) {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: mixColor(t.background, t.surface, 0.65), BorderColor: t.border})
	if count == 0 {
		return
	}
	offset := int(props.Offset) % count
	if offset < 0 {
		offset += count
	}
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
	normalize := func(v float32) float32 {
		v = (v - minValue) / (maxValue - minValue)
		if v < 0 {
			return 0
		}
		if v > 1 {
			return 1
		}
		return v
	}
	if histogram {
		step := props.Bounds.Width / float32(count)
		for i := 0; i < count; i++ {
			h := normalize(props.Values[(offset+i)%count]) * props.Bounds.Height
			w := step - 2
			if w < 1 {
				w = step
			}
			r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: props.Bounds.X + float32(i)*step + 1, Y: props.Bounds.Y + props.Bounds.Height - h, Width: w, Height: h}, Color: t.buttonHover, Row: int32(i)})
		}
	} else if count == 1 {
		y := props.Bounds.Y + (1-normalize(props.Values[offset]))*props.Bounds.Height
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: props.Bounds.X, Y: y, Width: props.Bounds.Width}, Color: t.buttonHover})
	} else {
		for i := 1; i < count; i++ {
			x1 := props.Bounds.X + float32(i-1)*props.Bounds.Width/float32(count-1)
			x2 := props.Bounds.X + float32(i)*props.Bounds.Width/float32(count-1)
			y1 := props.Bounds.Y + (1-normalize(props.Values[(offset+i-1)%count]))*props.Bounds.Height
			y2 := props.Bounds.Y + (1-normalize(props.Values[(offset+i)%count]))*props.Bounds.Height
			r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: x1, Y: y1, Width: x2 - x1, Height: y2 - y1}, Color: t.buttonHover, Row: int32(i - 1)})
		}
	}
	if props.Label != "" {
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X + 6, Y: props.Bounds.Y + 4, Width: props.Bounds.Width - 12, Height: 18}, Text: props.Label, Color: t.text, FontSize: Text14})
	}
	if props.Overlay != "" {
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X + props.Bounds.Width - float32(runtimeTextWidth(props.Overlay, Text14)) - 6, Y: props.Bounds.Y + 4, Width: props.Bounds.Width - 12, Height: 18}, Text: props.Overlay, Color: t.text, FontSize: Text14})
	}
}

func (r *runtime) dragDelta(token int32, bounds Rectangle, disabled bool) (float32, bool) {
	if !disabled && r.mousePressed[MouseButtonLeft] && r.consumeTap(bounds) {
		r.drag = scalarDrag{active: true, token: token, lastX: r.mousePos.X}
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

func (r *runtime) DragFloat(props DragFloatProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	speed := props.Speed
	if speed == 0 {
		speed = 1
	}
	changed := false
	for i := 0; i < count; i++ {
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		if delta, dragged := r.dragDelta(props.ID*16+int32(i)+1, cell, props.Disabled); dragged {
			value := props.Values[i] + delta*speed
			if props.Min < props.Max {
				if value < props.Min {
					value = props.Min
				}
				if value > props.Max {
					value = props.Max
				}
			}
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		r.drawDragCell(cell, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.ID, int32(i))
	}
	r.drawDragLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) DragInt(props DragIntProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	speed := props.Speed
	if speed == 0 {
		speed = 1
	}
	changed := false
	for i := 0; i < count; i++ {
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		if delta, dragged := r.dragDelta(props.ID*16+int32(i)+1, cell, props.Disabled); dragged {
			scaled := delta * speed
			step := int32(scaled + 0.5)
			if scaled < 0 {
				step = int32(scaled - 0.5)
			}
			value := props.Values[i] + step
			if props.Min < props.Max {
				value = clamp32(value, props.Min, props.Max)
			}
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%d"
		}
		r.drawDragCell(cell, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.ID, int32(i))
	}
	r.drawDragLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) DragFloatRange2(props DragFloatRange2Props) bool {
	if props.CurrentMin == nil || props.CurrentMax == nil {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	speed := props.Speed
	if speed == 0 {
		speed = 1
	}
	changed := false
	values := [2]*float32{props.CurrentMin, props.CurrentMax}
	formats := [2]string{props.Format, props.FormatMax}
	for i := 0; i < 2; i++ {
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/2, Y: props.Bounds.Y, Width: props.Bounds.Width / 2, Height: props.Bounds.Height}
		if delta, dragged := r.dragDelta(props.ID*16+int32(i)+1, cell, props.Disabled); dragged {
			value := *values[i] + delta*speed
			low, high := props.Min, props.Max
			if i == 0 && *props.CurrentMax < high {
				high = *props.CurrentMax
			}
			if i == 1 && *props.CurrentMin > low {
				low = *props.CurrentMin
			}
			if low < high {
				if value < low {
					value = low
				}
				if value > high {
					value = high
				}
			}
			changed = changed || value != *values[i]
			*values[i] = value
		}
		format := formats[i]
		if format == "" {
			format = props.Format
		}
		if format == "" {
			format = "%.3f"
		}
		r.drawDragCell(cell, fmt.Sprintf(format, *values[i]), props.Disabled, props.ID, int32(i))
	}
	if *props.CurrentMin > *props.CurrentMax {
		*props.CurrentMin = *props.CurrentMax
	}
	r.drawDragLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) DragIntRange2(props DragIntRange2Props) bool {
	if props.CurrentMin == nil || props.CurrentMax == nil {
		return false
	}
	props.Bounds = r.layoutRect(props.Bounds)
	speed := props.Speed
	if speed == 0 {
		speed = 1
	}
	changed := false
	values := [2]*int32{props.CurrentMin, props.CurrentMax}
	formats := [2]string{props.Format, props.FormatMax}
	for i := 0; i < 2; i++ {
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/2, Y: props.Bounds.Y, Width: props.Bounds.Width / 2, Height: props.Bounds.Height}
		if delta, dragged := r.dragDelta(props.ID*16+int32(i)+1, cell, props.Disabled); dragged {
			scaled := delta * speed
			step := int32(scaled + 0.5)
			if scaled < 0 {
				step = int32(scaled - 0.5)
			}
			value := *values[i] + step
			low, high := props.Min, props.Max
			if i == 0 && *props.CurrentMax < high {
				high = *props.CurrentMax
			}
			if i == 1 && *props.CurrentMin > low {
				low = *props.CurrentMin
			}
			if low < high {
				value = clamp32(value, low, high)
			}
			changed = changed || value != *values[i]
			*values[i] = value
		}
		format := formats[i]
		if format == "" {
			format = props.Format
		}
		if format == "" {
			format = "%d"
		}
		r.drawDragCell(cell, fmt.Sprintf(format, *values[i]), props.Disabled, props.ID, int32(i))
	}
	if *props.CurrentMin > *props.CurrentMax {
		*props.CurrentMin = *props.CurrentMax
	}
	r.drawDragLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) drawDragCell(bounds Rectangle, text string, disabled bool, id, component int32) {
	t := r.theme()
	color := t.button
	textColor := t.text
	if disabled {
		color, textColor = t.surface, t.icon
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: bounds, Text: text, Color: color, BorderColor: t.border, TextColor: textColor, FontSize: Text14, ID: id, Row: component, Disabled: disabled, Pressed: r.drag.active && r.drag.token == id*16+component+1})
}

func (r *runtime) drawDragLabel(bounds Rectangle, label string) {
	if label == "" {
		return
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y - 18, Width: bounds.Width - 12, Height: 16}, Text: label, Color: r.theme().text, FontSize: Text14})
}

func (r *runtime) sliderRatio(token int32, bounds Rectangle, disabled, vertical bool) (float32, bool) {
	pressed := !disabled && r.mousePressed[MouseButtonLeft] && r.consumeTap(bounds)
	if pressed {
		r.slider = scalarDrag{active: true, token: token}
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

func (r *runtime) drawSliderCell(bounds Rectangle, ratio float32, text string, disabled, vertical bool, id, component int32) {
	t := r.theme()
	base, accent, textColor := t.button, t.buttonHover, t.text
	if disabled {
		base, accent, textColor = t.surface, t.icon, t.icon
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: base, BorderColor: t.border, ID: id, Row: component, Disabled: disabled})
	if vertical {
		fill := Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height*(1-ratio), Width: bounds.Width, Height: bounds.Height * ratio}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: fill, Color: accent, ID: id, Row: component, Selected: true, Disabled: disabled})
		y := bounds.Y + bounds.Height*(1-ratio)
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: y, Width: bounds.Width}, Color: textColor, ID: id, Row: component})
	} else {
		fill := Rectangle{X: bounds.X, Y: bounds.Y, Width: bounds.Width * ratio, Height: bounds.Height}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: fill, Color: accent, ID: id, Row: component, Selected: true, Disabled: disabled})
		x := bounds.X + bounds.Width*ratio
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: x, Y: bounds.Y, Height: bounds.Height}, Color: textColor, ID: id, Row: component})
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y + (bounds.Height-float32(Text14))/2, Width: bounds.Width - 12, Height: float32(Text14)}, Text: text, Color: textColor, FontSize: Text14, ID: id, Row: component})
}

func (r *runtime) drawSliderLabel(bounds Rectangle, label string) {
	if label != "" {
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y - 18, Width: bounds.Width - 12, Height: 16}, Text: label, Color: r.theme().text, FontSize: Text14})
	}
}

func (r *runtime) sliderFloat(props SliderFloatProps, vertical bool) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	rangeValue := props.Max - props.Min
	changed := false
	for i := 0; i < count; i++ {
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		ratio := float32(0)
		if rangeValue > 0 {
			ratio = (props.Values[i] - props.Min) / rangeValue
		}
		if ratio < 0 {
			ratio = 0
		} else if ratio > 1 {
			ratio = 1
		}
		if next, active := r.sliderRatio(0x40000000^(props.ID*16+int32(i)+1), cell, props.Disabled, vertical); active && rangeValue > 0 {
			ratio = next
			value := props.Min + ratio*rangeValue
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		r.drawSliderCell(cell, ratio, fmt.Sprintf(format, props.Values[i]), props.Disabled, vertical, props.ID, int32(i))
	}
	r.drawSliderLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) sliderInt(props SliderIntProps, vertical bool) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	count := int(props.ValueCount)
	if count <= 0 || count > len(props.Values) {
		count = len(props.Values)
	}
	if count == 0 {
		return false
	}
	rangeValue := props.Max - props.Min
	changed := false
	for i := 0; i < count; i++ {
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		ratio := float32(0)
		if rangeValue > 0 {
			ratio = float32(props.Values[i]-props.Min) / float32(rangeValue)
		}
		if ratio < 0 {
			ratio = 0
		} else if ratio > 1 {
			ratio = 1
		}
		if next, active := r.sliderRatio(0x50000000^(props.ID*16+int32(i)+1), cell, props.Disabled, vertical); active && rangeValue > 0 {
			ratio = next
			value := props.Min + int32(ratio*float32(rangeValue)+0.5)
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%d"
		}
		r.drawSliderCell(cell, ratio, fmt.Sprintf(format, props.Values[i]), props.Disabled, vertical, props.ID, int32(i))
	}
	r.drawSliderLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) SliderFloat(props SliderFloatProps) bool  { return r.sliderFloat(props, false) }
func (r *runtime) SliderInt(props SliderIntProps) bool      { return r.sliderInt(props, false) }
func (r *runtime) VSliderFloat(props SliderFloatProps) bool { return r.sliderFloat(props, true) }
func (r *runtime) VSliderInt(props SliderIntProps) bool     { return r.sliderInt(props, true) }

func (r *runtime) SliderAngle(props SliderAngleProps) bool {
	if props.Value == nil {
		return false
	}
	degrees := *props.Value * 57.29577951308232
	format := props.Format
	if format == "" {
		format = "%.0f deg"
	}
	values := []float32{degrees}
	changed := r.sliderFloat(SliderFloatProps{Bounds: props.Bounds, ID: props.ID, Label: props.Label, Values: values, ValueCount: 1, Min: props.MinDegrees, Max: props.MaxDegrees, Format: format, Disabled: props.Disabled}, false)
	if changed {
		*props.Value = values[0] * 0.017453292519943295
	}
	return changed
}

func (r *runtime) numericInputState(token int32, formatted string) *numericInputState {
	if r.numericInputs == nil {
		r.numericInputs = make(map[int32]*numericInputState)
	}
	state := r.numericInputs[token]
	if state == nil {
		state = &numericInputState{text: make([]byte, 64)}
		r.numericInputs[token] = state
	}
	if !state.focused {
		clear(state.text)
		copy(state.text, formatted)
		state.cursor = int32(len(formatted))
	}
	return state
}

func (r *runtime) setNumericInputText(token int32, formatted string) {
	state := r.numericInputs[token]
	if state == nil {
		return
	}
	clear(state.text)
	copy(state.text, formatted)
	state.cursor = int32(len(formatted))
}

func (r *runtime) numericInputCell(bounds Rectangle, token int32, formatted string, disabled bool, step, stepFast float64) (string, float64, bool) {
	state := r.numericInputState(token, formatted)
	field := bounds
	minus, plus := bounds, bounds
	if step != 0 {
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
		textChanged = r.editText(field, state.text, &state.cursor, &state.focused, &commit, token, 63, false)
	}
	r.recordTextInput(FrameOpTextField, field, state.text, &state.cursor, &state.focused, token, Text14, false)
	if step == 0 {
		return string(state.text[:zeroIndex(state.text)]), 0, textChanged
	}
	t := r.theme()
	minusPressed := !disabled && r.consumeTap(minus)
	plusPressed := !disabled && r.consumeTap(plus)
	for _, button := range []struct {
		bounds  Rectangle
		label   string
		pressed bool
	}{{minus, "-", minusPressed}, {plus, "+", plusPressed}} {
		r.record(FrameOp{Kind: FrameOpButton, Bounds: button.bounds, Text: button.label, Color: t.button, BorderColor: t.border, TextColor: t.text, FontSize: Text14, ID: token, Disabled: disabled, Pressed: button.pressed})
	}
	if !minusPressed && !plusPressed {
		return string(state.text[:zeroIndex(state.text)]), 0, textChanged
	}
	increment := step
	if (r.keyDown[340] || r.keyDown[344]) && stepFast != 0 {
		increment = stepFast
	}
	if minusPressed {
		increment = -increment
	}
	return string(state.text[:zeroIndex(state.text)]), increment, textChanged
}

func (r *runtime) InputFloat(props InputFloatProps) bool {
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
		token := int32(0x60000000) ^ (props.ID*16 + int32(i) + 1)
		text, increment, edited := r.numericInputCell(cell, token, fmt.Sprintf(format, props.Values[i]), props.Disabled, float64(props.Step), float64(props.StepFast))
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseFloat(text, 32); err == nil {
				value, valid = float32(parsed), true
			}
		}
		if increment != 0 {
			value, valid = value+float32(increment), true
			r.setNumericInputText(token, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) InputInt(props InputIntProps) bool {
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
		token := int32(0x68000000) ^ (props.ID*16 + int32(i) + 1)
		text, increment, edited := r.numericInputCell(cell, token, fmt.Sprintf(format, props.Values[i]), props.Disabled, float64(props.Step), float64(props.StepFast))
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseInt(text, 0, 32); err == nil {
				value, valid = int32(parsed), true
			}
		}
		if increment != 0 {
			value, valid = value+int32(increment), true
			r.setNumericInputText(token, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) InputDouble(props InputDoubleProps) bool {
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
		token := int32(0x70000000) ^ (props.ID*16 + int32(i) + 1)
		text, increment, edited := r.numericInputCell(cell, token, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.Step, props.StepFast)
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseFloat(text, 64); err == nil {
				value, valid = parsed, true
			}
		}
		if increment != 0 {
			value, valid = value+increment, true
			r.setNumericInputText(token, fmt.Sprintf(format, value))
		}
		if valid && value != props.Values[i] {
			props.Values[i] = value
			changed = true
		}
	}
	r.drawSliderLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) Checkbox(id int32, x, y int32, label string, value *int32) bool {
	if value == nil {
		return false
	}
	theme := r.theme()
	font := Text16
	box := float32(22)
	gap := float32(10)
	labelW := float32(runtimeTextWidth(label, font))
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(y), Width: box + gap + labelW, Height: box})
	pressed := r.consumeTap(bounds)
	if dbg := r.tapDebug(bounds, pressed, label); dbg != "" {
		log.Print(dbg)
	}
	if pressed {
		if *value == 0 {
			*value = 1
		} else {
			*value = 0
		}
	}
	boxBounds := Rectangle{X: bounds.X, Y: bounds.Y, Width: box, Height: box}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: boxBounds, Color: theme.button, BorderColor: theme.border, ID: id, Pressed: pressed, Selected: *value != 0})
	if *value != 0 {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: boxBounds.X + 4, Y: boxBounds.Y + 11, Width: 6, Height: 7}, Color: theme.text, ID: id})
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: boxBounds.X + 10, Y: boxBounds.Y + 18, Width: 8, Height: -14}, Color: theme.text, ID: id})
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + box + gap, Y: bounds.Y + 3, Width: labelW, Height: bounds.Height}, Text: label, Color: theme.text, FontSize: font, ID: id})
	return pressed
}
func (r *runtime) Dropdown(id, x, y, w, h int32, options any, rest ...any) bool {
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
	if selected != nil && len(labels) > 0 {
		*selected = clamp32(*selected, 0, int32(len(labels)-1))
	}
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(y), Width: float32(w), Height: float32(h)})
	return r.dropdownAt(id, bounds, labels, selected)
}
func (r *runtime) dropdownAt(id int32, bounds Rectangle, labels []string, selected *int32) bool {
	if selected != nil && len(labels) > 0 {
		*selected = clamp32(*selected, 0, int32(len(labels)-1))
	}
	theme := r.theme()
	pressed := r.consumeTap(bounds)
	if pressed {
		r.openDropdowns[id] = !r.openDropdowns[id]
	}
	open := r.openDropdowns[id]
	r.record(FrameOp{Kind: FrameOpButton, Bounds: bounds, Text: selectedLabel(labels, selected), Color: theme.surface, BorderColor: theme.border, TextColor: theme.text, ID: id, FontSize: Text16, Pressed: pressed})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + bounds.Width - 24, Y: bounds.Y + 5, Width: 16, Height: bounds.Height}, Text: "x", Color: theme.text, FontSize: Text14, ID: id})
	if !open {
		return pressed
	}
	changed := false
	itemH := bounds.Height
	menuY := bounds.Y + bounds.Height + 4
	panel := Rectangle{X: bounds.X, Y: menuY, Width: bounds.Width, Height: itemH * float32(len(labels))}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: panel, Color: theme.surface, BorderColor: theme.border, ID: id})
	for i, label := range labels {
		row := Rectangle{X: bounds.X, Y: menuY + float32(i)*itemH, Width: bounds.Width, Height: itemH}
		selectedRow := selected != nil && int32(i) == *selected
		if selectedRow {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: mixColor(theme.surface, theme.button, 0.35), ID: id, Row: int32(i), Selected: true})
		}
		if selected != nil && r.consumeTap(row) {
			next := int32(i)
			if *selected != next {
				*selected = next
				changed = true
			}
			delete(r.openDropdowns, id)
			selectedRow = true
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + 12, Y: row.Y + 5, Width: row.Width - 24, Height: row.Height}, Text: label, Color: theme.text, FontSize: Text16, ID: id, Row: int32(i), Selected: selectedRow})
	}
	return pressed || changed
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
func (r *runtime) Screen(props ColumnProps) {
	r.pushGroup(props, FrameOpScreen)
}
func (r *runtime) GridLayout(props GridProps) {
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
func (r *runtime) Page(props PageProps) {
	bounds := pageBoundsOrView(props.Bounds, r.GetScreenWidth(), r.GetScreenHeight())
	key := props.Key
	if key == 0 {
		key = Key(props.Title)
	}
	if props.Title != "" {
		r.SetPageTitle(props.Title)
	}
	if props.Description != "" {
		r.SetPageDescription(props.Description)
	}
	if props.CanonicalURL != "" {
		r.SetPageCanonicalURL(props.CanonicalURL)
	}
	if props.ThemeColor.A != 0 {
		r.SetPageThemeColor(props.ThemeColor)
	}
	if props.Background.A != 0 {
		r.Background(props.Background)
	}
	r.record(FrameOp{Kind: FrameOpPage, Bounds: bounds, Text: props.Title, Semantic: UISemanticPage})
	r.Column(ColumnProps{Bounds: bounds, Gap: props.Gap, Padding: props.Padding, Key: key})
}
func (r *runtime) Section(props SectionProps) {
	bounds := pageBoundsOrView(props.Bounds, r.GetScreenWidth(), r.GetScreenHeight())
	key := props.Key
	if key == 0 {
		key = Key(props.Label)
	}
	r.record(FrameOp{Kind: FrameOpSection, Bounds: bounds, Text: props.Label, Semantic: UISemanticSection})
	r.Column(ColumnProps{Bounds: bounds, Gap: props.Gap, Padding: props.Padding, Key: key})
}
func (r *runtime) Heading(props HeadingProps) {
	level := props.Level
	if level < 1 {
		level = 1
	} else if level > 6 {
		level = 6
	}
	font := props.Font
	if font <= 0 {
		font = Text24
	}
	color := props.Color
	if color.A == 0 {
		color = r.theme().text
	}
	bounds := props.Bounds
	if bounds.Width <= 0 {
		bounds.Width = float32(runtimeTextWidth(props.Text, font))
	}
	if bounds.Height <= 0 {
		bounds.Height = float32(font)
	}
	bounds = r.layoutRect(bounds)
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: props.Text, Color: color, FontSize: font, ID: int32(props.Key), Semantic: UISemanticHeading, Level: level})
}
func (r *runtime) ParagraphText(props ParagraphTextProps) {
	font := props.Font
	if font <= 0 {
		font = Text16
	}
	color := props.Color
	if color.A == 0 {
		color = r.theme().text
	}
	width := int32(props.Bounds.Width)
	if width <= 0 {
		width = r.GetScreenWidth() - int32(props.Bounds.X)
	}
	bounds := r.layoutRect(Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: float32(width), Height: float32(font + props.LineGap)})
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: props.Text, Color: color, FontSize: font, ID: int32(props.Key), Semantic: UISemanticParagraph})
}
func (r *runtime) Link(props LinkProps) bool {
	return r.Href(HrefProps{
		Bounds:     props.Bounds,
		Text:       props.Text,
		Href:       props.Href,
		Font:       props.Font,
		FocusID:    props.FocusID,
		Disabled:   props.Disabled,
		Color:      props.Color,
		HoverColor: props.HoverColor,
	})
}
func (r *runtime) PagePicture(props PictureProps, altText string) {
	props.Bounds = r.layoutRect(props.Bounds)
	r.record(FrameOp{Kind: FrameOpPicture, Bounds: props.Bounds, Text: props.AssetPath, Color: props.Tint, Semantic: UISemanticPicture, Role: "img", AltText: altText})
}
func (r *runtime) Flow(props FlowProps) {
	r.Row(ColumnProps(props))
}
func (r *runtime) PageGrid(props GridProps) {
	r.GridLayout(props)
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

// The primary trio mirrors the Material mapping in theme_runtime.go: the
// palette's circle color is Primary, its contrast color OnPrimary, and a
// background tone serves as SurfaceVariant.
func (r *runtime) GetThemePrimary() Color   { return r.theme().circle }
func (r *runtime) GetThemeOnPrimary() Color { return materialOnColor(r.theme().circle) }
func (r *runtime) GetThemeSurfaceVariant() Color {
	return materialTone(r.theme().background, 10, 18, r.effectiveDark())
}
func (r *runtime) GetUIMaterialScheme() MaterialScheme {
	return materialScheme(r.theme(), r.effectiveDark())
}
func (r *runtime) TextInRect(text string, rect Rectangle, fontSize int32, color Color) {
	r.record(FrameOp{Kind: FrameOpText, Bounds: rect, Text: text, Color: color, FontSize: fontSize})
}
func (r *runtime) TextColored(text string, x, y, fontSize int32, color Color) {
	r.Text(text, x, y, fontSize, color)
}
func (r *runtime) TextDisabled(text string, x, y, fontSize int32) {
	r.Text(text, x, y, fontSize, r.Fade(r.GetThemeText(), 0.45))
}
func (r *runtime) TextWrapped(text string, bounds Rectangle, fontSize int32, color Color) {
	lineHeight := float32(fontSize + 2)
	for i, line := range wrapRuntimeText(text, bounds.Width, fontSize) {
		y := bounds.Y + float32(i)*lineHeight
		if bounds.Height > 0 && y+float32(fontSize) > bounds.Y+bounds.Height {
			break
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X, Y: y, Width: bounds.Width, Height: float32(fontSize)}, Text: line, Color: color, FontSize: fontSize})
	}
}
func (r *runtime) LabelText(label, value string, bounds Rectangle, fontSize int32, color Color) {
	labelWidth := float32(runtimeTextWidth(label, fontSize))
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: label, Color: r.Fade(color, 0.72), FontSize: fontSize})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + labelWidth + 8, Y: bounds.Y, Width: bounds.Width - labelWidth - 8, Height: bounds.Height}, Text: value, Color: color, FontSize: fontSize})
}
func (r *runtime) BulletText(text string, bounds Rectangle, fontSize int32, color Color) {
	bulletSize := float32(12)
	bulletHeight := bounds.Height
	if bulletHeight < float32(fontSize) {
		bulletHeight = float32(fontSize)
	}
	r.Bullet(Rectangle{X: bounds.X, Y: bounds.Y, Width: bulletSize, Height: bulletHeight})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + bulletSize + 4, Y: bounds.Y, Width: bounds.Width - bulletSize - 4, Height: bounds.Height}, Text: text, Color: color, FontSize: fontSize})
}

func wrapRuntimeText(text string, width float32, fontSize int32) []string {
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
			if float32(runtimeTextWidth(candidate, fontSize)) <= width {
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
func (r *runtime) Bevel(int32, int32, int32, int32, Color, Color) {}
func (r *runtime) Icon(id, x, y, size int32, iconType int32, tint Color) {
	r.record(FrameOp{
		Kind:     FrameOpIcon,
		Bounds:   Rectangle{X: float32(x), Y: float32(y), Width: float32(size), Height: float32(size)},
		Color:    tint,
		ID:       id,
		IconType: iconType,
		IconSize: size,
	})
}
func (r *runtime) Picture(props PictureProps) {
	props.Bounds = r.layoutRect(props.Bounds)
	r.record(FrameOp{Kind: FrameOpPicture, Bounds: props.Bounds, Text: props.AssetPath, Color: props.Tint})
}
func (r *runtime) Paragraph(spec ParagraphSpec, x int32, y *int32) {
	font := spec.Font
	if font <= 0 {
		font = Text16
	}
	lineGap := spec.LineGap
	color := spec.Color
	if color.A == 0 {
		color = r.theme().text
	}
	textY := int32(0)
	if y != nil {
		textY = *y
	}
	width := spec.Width
	if width <= 0 {
		width = int32(r.config.Width) - x
	}
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(textY), Width: float32(width), Height: float32(font + lineGap)})
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: spec.Text, Color: color, FontSize: font})
	if y != nil {
		*y += font + lineGap
	}
}
func (r *runtime) IconButton(props IconButtonProps) bool {
	theme := r.theme()
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
	background := props.Background
	if background.A == 0 {
		background = theme.button
	}
	hoverBackground := props.HoverBackground
	if hoverBackground.A == 0 {
		hoverBackground = theme.buttonHover
	}
	border := props.Border
	if border.A == 0 {
		border = theme.border
	}
	iconColor := props.IconColor
	if iconColor.A == 0 {
		iconColor = theme.icon
	}
	pressed := false
	if !props.Disabled {
		pressed = r.consumeTap(props.Bounds)
	}
	fill := background
	if pressed {
		fill = hoverBackground
	}
	if props.Disabled {
		fill = mixColor(theme.surface, background, 0.45)
		iconColor = mixColor(theme.icon, theme.surface, 0.55)
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: props.Bounds, Color: fill, BorderColor: border, ID: props.FocusID, Disabled: props.Disabled, Pressed: pressed})
	iconX := int32(props.Bounds.X) + (int32(props.Bounds.Width)-size)/2
	iconY := int32(props.Bounds.Y) + (int32(props.Bounds.Height)-size)/2
	iconType := props.IconType
	if iconType == 0 && props.Icon.ID != 0 {
		iconType = int32(props.Icon.ID)
	}
	r.Icon(props.FocusID, iconX, iconY, size, iconType, iconColor)
	return pressed
}
func (r *runtime) Href(props HrefProps) bool {
	font := props.Font
	if font <= 0 {
		font = Text16
	}
	color := props.Color
	if color.A == 0 {
		color = r.theme().link
	}
	bounds := r.layoutRect(props.Bounds)
	if bounds.Width <= 0 {
		bounds.Width = float32(runtimeTextWidth(props.Text, font))
	}
	if bounds.Height <= 0 {
		bounds.Height = float32(font + 4)
	}
	pressed := false
	if !props.Disabled {
		pressed = r.consumeTap(bounds)
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: props.Text, Color: color, FontSize: font, FocusID: props.FocusID, Disabled: props.Disabled, Pressed: pressed, Semantic: UISemanticLink, Href: props.Href, Role: "link"})
	return pressed
}
func (r *runtime) Slider(id, x, y, w int32, label string, min, max int32, value *int32, rest ...any) bool {
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(y), Width: float32(w), Height: 56})
	return r.sliderAt(id, bounds, label, min, max, value, rest...)
}
func (r *runtime) sliderAt(id int32, bounds Rectangle, label string, min, max int32, value *int32, rest ...any) bool {
	if value == nil {
		return false
	}
	if max < min {
		min, max = max, min
	}
	theme := r.theme()
	*value = clamp32(*value, min, max)
	changed := false
	if tapX, tapped := r.consumeTapPoint(bounds); tapped {
		old := *value
		span := max - min
		if span > 0 && bounds.Width > 0 {
			t := (tapX - bounds.X) / bounds.Width
			if t < 0 {
				t = 0
			} else if t > 1 {
				t = 1
			}
			*value = min + int32(t*float32(span)+0.5)
			*value = clamp32(*value, min, max)
		}
		changed = *value != old
	}
	font := Text16
	trackY := bounds.Y + 28
	track := Rectangle{X: bounds.X, Y: trackY, Width: bounds.Width, Height: 8}
	valueText := fmt.Sprintf("%d%s", *value, sliderSuffix(rest...))
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X, Y: bounds.Y, Width: bounds.Width * 0.5, Height: 18}, Text: label, Color: theme.text, FontSize: font, ID: id})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + bounds.Width - float32(runtimeTextWidth(valueText, font)), Y: bounds.Y, Width: bounds.Width * 0.5, Height: 18}, Text: valueText, Color: theme.text, FontSize: font, ID: id})
	r.record(FrameOp{Kind: FrameOpRect, Bounds: track, Color: mixColor(theme.background, theme.button, 0.45), BorderColor: theme.border, ID: id})
	fillW := float32(0)
	if max > min {
		fillW = float32(*value-min) / float32(max-min) * bounds.Width
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: track.X, Y: track.Y, Width: fillW, Height: track.Height}, Color: theme.buttonHover, ID: id, Selected: true})
	r.record(FrameOp{Kind: FrameOpButton, Bounds: Rectangle{X: bounds.X + fillW - 6, Y: trackY - 7, Width: 12, Height: 22}, Color: theme.button, BorderColor: theme.border, ID: id, Pressed: changed})
	return changed
}
func (r *runtime) Toggle(id, x, y, w, h int32, value *int32, offLabel, onLabel string) bool {
	if value == nil {
		return false
	}
	theme := r.theme()
	if h < 34 {
		h = 34
	}
	minHalf := maxInt(runtimeTextWidth(offLabel, Text16), runtimeTextWidth(onLabel, Text16)) + 16
	minW := int32(minHalf*2 + 6)
	if w < minW {
		w = minW
	}
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(y), Width: float32(w), Height: float32(h)})
	pressed := r.consumeTap(bounds)
	if pressed {
		if *value == 0 {
			*value = 1
		} else {
			*value = 0
		}
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: mixColor(theme.background, theme.surface, 0.65), BorderColor: theme.border, ID: id})
	activeW := (bounds.Width - 6) / 2
	activeX := bounds.X + 3
	if *value != 0 {
		activeX = bounds.X + bounds.Width - activeW - 3
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: Rectangle{X: activeX, Y: bounds.Y + 3, Width: activeW, Height: bounds.Height - 6}, Color: theme.button, BorderColor: theme.buttonHover, ID: id, Pressed: pressed, Selected: *value != 0})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + 6, Width: bounds.Width / 2, Height: bounds.Height}, Text: offLabel, Color: theme.text, FontSize: Text16, ID: id})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + bounds.Width/2, Y: bounds.Y + 6, Width: bounds.Width / 2, Height: bounds.Height}, Text: onLabel, Color: theme.text, FontSize: Text16, ID: id})
	return pressed
}
func (r *runtime) Modal(title, message, cancelBtn, confirmBtn string) int {
	result, _ := r.drawActionModal(title, message, []string{cancelBtn, confirmBtn}, 0)
	return int(result)
}

func (r *runtime) drawActionModal(title, message string, labels []string, fieldHeight float32) (int32, Rectangle) {
	t := r.theme()
	w := float32(420)
	if limit := float32(r.GetScreenWidth() - 16); w > limit {
		w = limit
	}
	if w < 280 {
		w = 280
	}
	if limit := float32(max32(1, r.GetScreenWidth()-8)); w > limit {
		w = limit
	}
	h := float32(160) + fieldHeight
	if message != "" {
		h += 24
	}
	x := (float32(r.GetScreenWidth()) - w) / 2
	y := (float32(r.GetScreenHeight()) - h) / 2
	panel := Rectangle{X: x, Y: y, Width: w, Height: h}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{Width: float32(r.GetScreenWidth()), Height: float32(r.GetScreenHeight())}, Color: Color{A: 180}})
	r.record(FrameOp{Kind: FrameOpRect, Bounds: panel, Color: t.surface, BorderColor: t.border})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: x + 18, Y: y + 14, Width: w - 36, Height: 30}, Text: title, Color: t.text, FontSize: Text20})
	if message != "" {
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: x + 18, Y: y + 54, Width: w - 36, Height: h - 104 - fieldHeight}, Text: message, Color: t.text, FontSize: Text16})
	}

	result := int32(0)
	buttonW := float32(96)
	gap := float32(8)
	buttonY := y + h - 46
	buttonX := x + w - 18 - float32(len(labels))*buttonW - float32(maxInt(0, len(labels)-1))*gap
	for i, label := range labels {
		if label == "" {
			if len(labels) == 1 {
				label = "OK"
			} else if i == 0 {
				label = "Cancel"
			} else {
				label = "OK"
			}
		}
		bounds := Rectangle{X: buttonX + float32(i)*(buttonW+gap), Y: buttonY, Width: buttonW, Height: 30}
		pressed := r.consumeTap(bounds)
		fill := t.button
		if i == len(labels)-1 {
			fill = t.buttonHover
		}
		r.record(FrameOp{Kind: FrameOpButton, Bounds: bounds, Text: label, Color: fill, BorderColor: t.border, TextColor: t.text, FontSize: Text14, Pressed: pressed})
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
	field := Rectangle{X: x + 18, Y: buttonY - fieldHeight - 12, Width: w - 36, Height: fieldHeight}
	return result, field
}
func (r *runtime) TitleBar(title string, height int32) {
	if height <= 0 {
		height = 44
	}
	t := r.theme()
	b := Rectangle{Width: float32(r.GetScreenWidth()), Height: float32(height)}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: b, Color: t.surface, BorderColor: t.border})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: 12, Y: float32(height-Text20) / 2, Width: b.Width - 24, Height: float32(Text20 + 4)}, Text: title, Color: t.text, FontSize: Text20})
}
func (r *runtime) BottomNav(props BottomNavProps) {
	count := int(props.Count)
	if count <= 0 || count > len(props.Items) {
		count = len(props.Items)
	}
	if count == 0 {
		return
	}
	w, h := props.ViewWidth, props.Height
	if w <= 0 {
		w = r.GetScreenWidth()
	}
	if h <= 0 {
		h = 64
	}
	viewH := props.ViewHeight
	if viewH <= 0 {
		viewH = r.GetScreenHeight()
	}
	b := Rectangle{X: float32(props.SideMargin), Y: float32(viewH - props.BottomMargin - h), Width: float32(w - props.SideMargin*2), Height: float32(h)}
	if b.Width < 1 {
		b.Width = 1
	}
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: b, Color: t.surface, BorderColor: t.border})
	itemW := b.Width / float32(count)
	for i := 0; i < count; i++ {
		item := props.Items[i]
		ib := Rectangle{X: b.X + float32(i)*itemW, Y: b.Y, Width: itemW, Height: b.Height}
		pressed := !item.Disabled && r.consumeTap(ib)
		color := t.text
		if item.Disabled {
			color = t.icon
		}
		if item.Active {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: ib, Color: t.button, Selected: true})
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: ib.X + 6, Y: ib.Y + (ib.Height-float32(Text14))/2, Width: ib.Width - 12, Height: float32(Text14 + 4)}, Text: item.Label, Color: color, FontSize: Text14, ID: item.Route, Pressed: pressed, Selected: item.Active, Disabled: item.Disabled})
	}
}
func (r *runtime) TopNav(props TopNavProps) {
	w, h := props.Width, props.Height
	if w <= 0 {
		w = r.GetScreenWidth() - props.X
	}
	if h <= 0 {
		h = 44
	}
	b := Rectangle{X: float32(props.X), Y: float32(props.Y), Width: float32(w), Height: float32(h)}
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: b, Color: t.surface, BorderColor: t.border})
	labels := labelsOf(props.Options)
	count := int(props.OptionCount)
	if count <= 0 || count > len(labels) {
		count = len(labels)
	}
	if count > 0 && props.SelectedIndex != nil {
		pad := props.SidePadding
		if pad <= 0 {
			pad = 8
		}
		dh := props.DropdownHeight
		if dh <= 0 {
			dh = h - pad*2
		}
		drop := Rectangle{X: b.X + float32(pad), Y: b.Y + float32((h-dh)/2), Width: b.Width - float32(pad*2), Height: float32(dh)}
		if props.Disabled {
			r.record(FrameOp{Kind: FrameOpButton, Bounds: drop, Text: selectedLabel(labels[:count], props.SelectedIndex), Color: t.surface, BorderColor: t.border, TextColor: t.icon, FontSize: Text16, ID: props.ID, Disabled: true})
		} else {
			r.dropdownAt(props.ID, drop, labels[:count], props.SelectedIndex)
		}
		return
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: b.X + 12, Y: b.Y + float32(h-Text20)/2, Width: b.Width - 24, Height: float32(Text20 + 4)}, Text: props.Title, Color: t.text, FontSize: Text20, Disabled: props.Disabled})
}
func (r *runtime) Toolbar(props ToolbarProps) ToolbarResult {
	theme := r.theme()
	result := ToolbarResult{SelectedMenuItem: -1, ClickedAction: -1}
	if props.Width <= 0 {
		props.Width = r.GetScreenWidth() - props.X
	}
	if props.Height <= 0 {
		props.Height = 44
	}
	sidePadding := props.SidePadding
	if sidePadding < 0 {
		sidePadding = 0
	} else if sidePadding == 0 {
		sidePadding = 12
	}
	iconSize := props.ActionIconSize
	if iconSize <= 0 {
		iconSize = 20
	}
	iconPadding := props.ActionIconPadding
	if iconPadding <= 0 {
		iconPadding = 6
	}
	gap := props.ActionGap
	if gap <= 0 {
		gap = 6
	}
	bounds := Rectangle{X: float32(props.X), Y: float32(props.Y), Width: float32(props.Width), Height: float32(props.Height)}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: mixColor(theme.background, theme.surface, 0.6)})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height - 1, Width: bounds.Width, Height: 0}, Color: theme.border})
	actionCount := int(props.ActionCount)
	if actionCount <= 0 || actionCount > len(props.Actions) {
		actionCount = len(props.Actions)
	}
	actionW := iconSize + iconPadding*2
	x := props.X + props.Width - sidePadding - actionW
	y := props.Y + (props.Height-actionW)/2
	for i := 0; i < actionCount; i++ {
		action := props.Actions[i]
		if r.IconButton(IconButtonProps{
			Bounds:          Rectangle{X: float32(x), Y: float32(y), Width: float32(actionW), Height: float32(actionW)},
			Icon:            action.Icon,
			IconType:        action.IconType,
			IconSize:        iconSize,
			IconPadding:     iconPadding,
			FocusID:         props.ID*100 + int32(i) + 1,
			Disabled:        action.Disabled,
			Background:      theme.surface,
			HoverBackground: theme.buttonHover,
			IconColor:       theme.icon,
			Border:          theme.border,
		}) {
			result.ClickedAction = int32(i)
		}
		x -= actionW + gap
	}
	return result
}
func (r *runtime) MenuBar(id int32, bounds Rectangle, menus []Menu, openIndex *int32) MenuBarResult {
	theme := r.theme()
	result := MenuBarResult{OpenIndex: -1}
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
	if v, ok := r.openMenus[id]; ok {
		open = v
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: theme.surface, BorderColor: theme.border})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height - 1, Width: bounds.Width, Height: 0}, Color: theme.border})
	x := bounds.X + 4
	font := Text14
	for i, menu := range menus {
		w := float32(maxInt(44, runtimeTextWidth(menu.Label, font)+24))
		item := Rectangle{X: x, Y: bounds.Y + 3, Width: w, Height: bounds.Height - 6}
		if r.consumeTap(item) {
			idx := int32(i)
			if open == idx {
				idx = -1
			}
			open = idx
			if open < 0 {
				delete(r.openMenus, id)
			} else {
				r.openMenus[id] = open
			}
		}
		if open == int32(i) {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: item, Color: theme.button})
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: item.X + 10, Y: item.Y + 5, Width: item.Width - 20, Height: item.Height}, Text: menu.Label, Color: theme.text, FontSize: font})
		x += w + 2
	}
	if open >= 0 && int(open) < len(menus) {
		result.OpenIndex = open
		if openIndex != nil {
			*openIndex = open
		}
		menu := menus[open]
		menuX := bounds.X + 4
		for i := 0; i < int(open); i++ {
			menuX += float32(maxInt(44, runtimeTextWidth(menus[i].Label, font)+24)) + 2
		}
		items := limitedMenuItems(menu.Items, menu.ItemCount)
		result.ActivatedID, _ = r.drawPopupMenu(id, int32(menuX), int32(bounds.Y+bounds.Height), items)
		if result.ActivatedID != 0 {
			delete(r.openMenus, id)
			delete(r.openSubmenus, id)
			open = -1
		}
	}
	if open < 0 && openIndex != nil {
		*openIndex = -1
	}
	return result
}

func limitedMenuItems(items []MenuItem, count int32) []MenuItem {
	if count <= 0 || int(count) > len(items) {
		return items
	}
	return items[:count]
}

func (r *runtime) drawPopupMenu(id, x, y int32, items []MenuItem) (int32, Rectangle) {
	theme := r.theme()
	font := int32(Text14)
	rowH := float32(30)
	width := float32(180)
	for _, item := range items {
		candidate := float32(runtimeTextWidth(item.Label, font) + 36)
		if item.Accelerator != "" {
			candidate += float32(runtimeTextWidth(item.Accelerator, font) + 28)
		}
		if candidate > width {
			width = candidate
		}
	}
	panel := Rectangle{X: float32(x), Y: float32(y), Width: width, Height: rowH*float32(len(items)) + 8}
	if len(items) == 0 {
		return 0, panel
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: panel, Color: theme.surface, BorderColor: theme.border})
	for i, item := range items {
		row := Rectangle{X: panel.X + 4, Y: panel.Y + 4 + float32(i)*rowH, Width: panel.Width - 8, Height: rowH}
		if item.Kind == MenuSeparator {
			r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: row.X + 8, Y: row.Y + row.Height/2, Width: row.Width - 16}, Color: theme.border})
			continue
		}
		hovered := pointInRect(r.mousePos.X, r.mousePos.Y, row)
		if hovered && !item.Disabled {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: theme.buttonHover})
			if item.Kind == MenuSubmenu {
				r.openSubmenus[id] = item.ID
			}
		}
		if !item.Disabled && item.Kind != MenuSubmenu && r.consumeTap(row) {
			return item.ID, panel
		}
		textColor := theme.text
		if item.Disabled {
			textColor = r.Fade(theme.text, 0.45)
		}
		label := item.Label
		if (item.Kind == MenuCheck || item.Kind == MenuRadio) && item.Checked {
			label = "✓ " + label
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + 10, Y: row.Y + 6, Width: row.Width - 20, Height: row.Height}, Text: label, Color: textColor, FontSize: font, Disabled: item.Disabled})
		if item.Accelerator != "" {
			r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + row.Width - float32(runtimeTextWidth(item.Accelerator, font)) - 12, Y: row.Y + 6, Width: 80, Height: row.Height}, Text: item.Accelerator, Color: theme.icon, FontSize: font, Disabled: item.Disabled})
		}
		if item.Kind == MenuSubmenu {
			r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + row.Width - 18, Y: row.Y + 6, Width: 12, Height: row.Height}, Text: ">", Color: textColor, FontSize: font})
			if r.openSubmenus[id] == item.ID {
				subitems := limitedMenuItems(item.Submenu, item.SubmenuCount)
				selected, _ := r.drawPopupMenu(item.ID, int32(row.X+row.Width), int32(row.Y), subitems)
				if selected != 0 {
					return selected, panel
				}
			}
		}
	}
	return 0, panel
}

func (r *runtime) PopupMenu(id, x, y int32, items []MenuItem, itemCount int32) int32 {
	selected, _ := r.drawPopupMenu(id, x, y, limitedMenuItems(items, itemCount))
	return selected
}

func (r *runtime) ContextMenu(props ContextMenuProps) int32 {
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
	if r.mouseReleased[MouseButtonRight] && pointInRect(r.mousePos.X, r.mousePos.Y, props.Trigger) {
		r.contextMenus[props.ID] = r.mousePos
		if props.Open != nil {
			*props.Open = 1
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
	selected, panel := r.drawPopupMenu(props.ID, int32(pos.X), int32(pos.Y), limitedMenuItems(props.Items, props.ItemCount))
	closeMenu := selected != 0
	if !closeMenu {
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
		if props.Open != nil {
			*props.Open = 0
		}
	}
	return selected
}
func (r *runtime) CanvasGrid(bounds Rectangle, step int32, color Color) {
	bounds = r.layoutRect(bounds)
	spacing := r.ScaleUIPx(step)
	if spacing < 4 {
		spacing = 4
	}
	for x := bounds.X; x < bounds.X+bounds.Width; x += float32(spacing) {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: x, Y: bounds.Y, Width: 0, Height: bounds.Height}, Color: color})
	}
	for y := bounds.Y; y < bounds.Y+bounds.Height; y += float32(spacing) {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: y, Width: bounds.Width, Height: 0}, Color: color})
	}
}
func (r *runtime) SelectableText(value string, x, y, fontSize int32, color Color) {
	if fontSize <= 0 {
		fontSize = Text16
	}
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(y), Width: float32(runtimeTextWidth(value, fontSize)), Height: float32(fontSize + 4)})
	key := Key(fmt.Sprintf("%g:%g:%s", bounds.X, bounds.Y, value))
	if r.consumeTap(bounds) {
		r.selectableText = key
	}
	selected := r.selectableText == key
	if selected {
		for _, event := range r.inputEvents {
			if event.shortcut && event.key == KeyC {
				r.clipboard = value
			}
		}
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: value, Color: color, FontSize: fontSize, ID: int32(key), Selected: selected, SelectionStart: 0, SelectionEnd: int32(len(value))})
}
func (r *runtime) ShowToast(message string) { r.ShowToastFor(message, 3) }
func (r *runtime) ShowToastFor(message string, seconds float64) {
	if message == "" {
		r.toastMessage = ""
		r.toastUntil = time.Time{}
		return
	}
	if seconds <= 0 {
		seconds = 3
	}
	r.toastMessage = message
	r.toastUntil = time.Now().Add(time.Duration(seconds * float64(time.Second)))
}
func (r *runtime) recordToast() {
	if r.toastMessage == "" || time.Now().After(r.toastUntil) {
		r.toastMessage = ""
		return
	}
	t := r.theme()
	w := float32(runtimeTextWidth(r.toastMessage, Text14) + 28)
	if max := float32(max32(1, r.GetScreenWidth()-36)); w > max {
		w = max
	}
	b := Rectangle{X: (float32(r.GetScreenWidth()) - w) / 2, Y: float32(r.GetScreenHeight() - 58), Width: w, Height: 40}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: b, Color: t.surface, BorderColor: t.border})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: b.X + 14, Y: b.Y + 11, Width: b.Width - 28, Height: 18}, Text: r.toastMessage, Color: t.text, FontSize: Text14})
}
func (r *runtime) TextArea(props TextAreaProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	changed := r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, nil, props.FocusID, props.MaxCodepoints, false)
	r.recordTextInput(FrameOpTextArea, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, props.Font, false)
	return changed
}
func (r *runtime) Radio(props RadioButtonProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	pressed := !props.Disabled && r.consumeTap(props.Bounds)
	c := r.theme().text
	if props.Disabled {
		c = r.theme().icon
	}
	mark := "○"
	if props.Checked {
		mark = "◉"
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: 24, Height: props.Bounds.Height}, Text: mark, Color: c, FontSize: Text16, ID: props.ID, Pressed: pressed, Disabled: props.Disabled, Selected: props.Checked})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X + 28, Y: props.Bounds.Y, Width: props.Bounds.Width - 28, Height: props.Bounds.Height}, Text: props.Label, Color: c, FontSize: Text16, ID: props.ID, Pressed: pressed, Disabled: props.Disabled, Selected: props.Checked})
	if pressed {
		return props.ID
	}
	return 0
}
func (r *runtime) Spinbox(p SpinboxProps) bool {
	p.Bounds = r.layoutRect(p.Bounds)
	bw := float32(28)
	if p.Bounds.Width < bw*2 {
		bw = p.Bounds.Width / 2
	}
	l := Rectangle{X: p.Bounds.X, Y: p.Bounds.Y, Width: bw, Height: p.Bounds.Height}
	rr := Rectangle{X: p.Bounds.X + p.Bounds.Width - bw, Y: p.Bounds.Y, Width: bw, Height: p.Bounds.Height}
	minus, plus := !p.Disabled && r.consumeTap(l), !p.Disabled && r.consumeTap(rr)
	step := p.Step
	if step <= 0 {
		step = 1
	}
	changed := false
	if p.Value != nil && minus {
		n := *p.Value - step
		if p.Wrap && *p.Value <= p.Min {
			n = p.Max
		}
		n = clamp32(n, p.Min, p.Max)
		changed = n != *p.Value
		*p.Value = n
	}
	if p.Value != nil && plus {
		n := *p.Value + step
		if p.Wrap && *p.Value >= p.Max {
			n = p.Min
		}
		n = clamp32(n, p.Min, p.Max)
		changed = changed || n != *p.Value
		*p.Value = n
	}
	center := Rectangle{X: l.X + bw, Y: p.Bounds.Y, Width: p.Bounds.Width - bw*2, Height: p.Bounds.Height}
	txt := p.ValueText
	if txt == "" {
		v := int32(0)
		if p.Value != nil {
			v = *p.Value
		}
		txt = fmt.Sprint(v)
	}
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: center, Color: t.surface, BorderColor: t.border, ID: p.ID, Disabled: p.Disabled})
	r.record(FrameOp{Kind: FrameOpButton, Bounds: l, Text: "-", Color: t.button, BorderColor: t.border, TextColor: t.text, FontSize: Text16, Pressed: minus, Disabled: p.Disabled})
	r.record(FrameOp{Kind: FrameOpButton, Bounds: rr, Text: "+", Color: t.button, BorderColor: t.border, TextColor: t.text, FontSize: Text16, Pressed: plus, Disabled: p.Disabled})
	r.record(FrameOp{Kind: FrameOpText, Bounds: center, Text: txt, Color: t.text, FontSize: Text16})
	return changed
}
func (r *runtime) Combobox(p ComboboxProps) bool {
	p.Bounds = r.layoutRect(p.Bounds)
	n := p.OptionCount
	if n <= 0 || n > int32(len(p.Options)) {
		n = int32(len(p.Options))
	}
	opts := p.Options[:n]
	if p.Disabled {
		t := r.theme()
		r.record(FrameOp{Kind: FrameOpButton, Bounds: p.Bounds, Text: selectedLabel(opts, p.SelectedIndex), Color: t.surface, BorderColor: t.border, TextColor: t.icon, FontSize: Text16, ID: p.ID, Disabled: true})
		return false
	}
	return r.dropdownAt(p.ID, p.Bounds, opts, p.SelectedIndex)
}
func (r *runtime) LabelFrame(p LabelFrameProps) {
	p.Bounds = r.layoutRect(p.Bounds)
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: p.Bounds, BorderColor: t.border})
	if p.Title != "" {
		w := float32(runtimeTextWidth(p.Title, Text14))
		b := Rectangle{X: p.Bounds.X + 8, Y: p.Bounds.Y - 8, Width: w + 16, Height: 18}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: b, Color: t.background})
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: b.X + 8, Y: b.Y, Width: w, Height: b.Height}, Text: p.Title, Color: t.text, FontSize: Text14})
	}
}
func (r *runtime) Notebook(p NotebookProps) int32 {
	if p.SelectedIndex == nil || len(p.Tabs) == 0 {
		return 0
	}
	p.Bounds = r.layoutRect(p.Bounds)
	t := r.theme()
	x := p.Bounds.X
	changed := int32(0)
	for i, s := range p.Tabs {
		w := float32(runtimeTextWidth(s, Text16) + 28)
		b := Rectangle{X: x, Y: p.Bounds.Y, Width: w, Height: 34}
		pressed := r.consumeTap(b)
		sel := *p.SelectedIndex == int32(i)
		fill := t.button
		if sel {
			fill = t.surface
		}
		r.record(FrameOp{Kind: FrameOpButton, Bounds: b, Text: s, Color: fill, BorderColor: t.border, TextColor: t.text, FontSize: Text16, Pressed: pressed, Selected: sel})
		if pressed && !sel {
			*p.SelectedIndex = int32(i)
			changed = 1
		}
		x += w
	}
	if p.Bounds.Height > 34 {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: p.Bounds.X, Y: p.Bounds.Y + 34, Width: p.Bounds.Width, Height: p.Bounds.Height - 34}, BorderColor: t.border})
	}
	return changed
}
func (r *runtime) PanedView(p PanedViewProps) int32 {
	if p.Split == nil {
		return 0
	}
	p.Bounds = r.layoutRect(p.Bounds)
	split := *p.Split
	limit := int32(p.Bounds.Height) - p.MinSecond
	if p.Vertical {
		limit = int32(p.Bounds.Width) - p.MinSecond
	}
	split = clamp32(split, p.MinFirst, limit)
	h := Rectangle{X: p.Bounds.X, Y: p.Bounds.Y + float32(split) - 4, Width: p.Bounds.Width, Height: 8}
	if p.Vertical {
		h = Rectangle{X: p.Bounds.X + float32(split) - 4, Y: p.Bounds.Y, Width: 8, Height: p.Bounds.Height}
	}
	changed := int32(0)
	if r.mouseDown[MouseButtonLeft] && pointInRect(r.mousePos.X, r.mousePos.Y, h) {
		n := int32(r.mousePos.Y - p.Bounds.Y)
		if p.Vertical {
			n = int32(r.mousePos.X - p.Bounds.X)
		}
		n = clamp32(n, p.MinFirst, limit)
		if n != *p.Split {
			*p.Split = n
			split = n
			changed = 1
		}
	}
	if p.Vertical {
		h.X = p.Bounds.X + float32(split) - 4
	} else {
		h.Y = p.Bounds.Y + float32(split) - 4
	}
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: h, Color: t.button, BorderColor: t.border, ID: p.ID, Pressed: changed != 0})
	return changed
}
func (r *runtime) Collapsible(p CollapsibleProps) int32 {
	p.Bounds = r.layoutRect(p.Bounds)
	p.Bounds.Height = 32
	pressed := p.Open != nil && r.consumeTap(p.Bounds)
	if pressed {
		*p.Open = !*p.Open
	}
	mark := ">"
	if p.Open != nil && *p.Open {
		mark = "v"
	}
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpButton, Bounds: p.Bounds, Text: mark + "  " + p.Label, Color: t.button, BorderColor: t.buttonHover, TextColor: t.text, FontSize: Text16, Pressed: pressed, Selected: p.Open != nil && *p.Open})
	if pressed {
		return 1
	}
	return 0
}
func (r *runtime) ColorPicker(b Rectangle, c *Color) bool {
	if c == nil {
		return false
	}
	b = r.layoutRect(b)
	rv, gv, bv := int32(c.R), int32(c.G), int32(c.B)
	changed := r.sliderAt(8101, Rectangle{X: b.X, Y: b.Y, Width: b.Width, Height: 32}, "R", 0, 255, &rv)
	changed = r.sliderAt(8102, Rectangle{X: b.X, Y: b.Y + 36, Width: b.Width, Height: 32}, "G", 0, 255, &gv) || changed
	changed = r.sliderAt(8103, Rectangle{X: b.X, Y: b.Y + 72, Width: b.Width, Height: 32}, "B", 0, 255, &bv) || changed
	if changed {
		c.R, c.G, c.B = uint8(rv), uint8(gv), uint8(bv)
	}
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: b.X, Y: b.Y + 112, Width: 80, Height: 36}, Color: *c, BorderColor: t.border})
	return changed
}
func (r *runtime) TreeView(props TreeViewProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	count := props.ItemCount
	if count <= 0 || count > int32(len(props.Items)) {
		count = int32(len(props.Items))
	}
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	maxScroll := max32(0, count*rowH-int32(props.Bounds.Height))
	if props.ScrollOffset != nil {
		*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
		if pointInRect(r.mousePos.X, r.mousePos.Y, props.Bounds) && r.mouseWheel != 0 {
			*props.ScrollOffset = clamp32(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, 0, maxScroll)
		}
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	theme := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: theme.surface, BorderColor: theme.border, ID: props.ID})
	changed := int32(0)
	first := scroll / rowH
	yOffset := scroll % rowH
	for index := first; index < count; index++ {
		y := props.Bounds.Y + float32((index-first)*rowH-yOffset)
		if y >= props.Bounds.Y+props.Bounds.Height {
			break
		}
		item := props.Items[index]
		row := Rectangle{X: props.Bounds.X, Y: y, Width: props.Bounds.Width, Height: float32(rowH)}
		selected := props.SelectedID != nil && *props.SelectedID == item.ID
		pressed := item.Selectable != 0 && r.consumeTap(row)
		if pressed && props.SelectedID != nil {
			*props.SelectedID = item.ID
			selected = true
			changed = 1
		}
		if selected {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: theme.button, ID: props.ID, Row: index, Selected: true})
		}
		indent := float32(8 + item.Depth*18)
		mark := ">"
		if item.Expanded != 0 {
			mark = "v"
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + indent, Y: row.Y + 4, Width: 16, Height: row.Height}, Text: mark, Color: theme.icon, FontSize: Text16, ID: item.ID, Row: index})
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + indent + 18, Y: row.Y + 4, Width: row.Width - indent - 26, Height: row.Height}, Text: item.Label, Color: theme.text, FontSize: Text16, ID: item.ID, Row: index, Pressed: pressed, Selected: selected})
	}
	return changed
}
func (r *runtime) SourceView(props SourceViewProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	font := props.FontSize
	if font <= 0 {
		font = Text14
	}
	lineH := props.LineHeight
	if lineH <= 0 {
		lineH = font + 4
	}
	lines := strings.Split(props.Text, "\n")
	if props.Text == "" {
		lines = nil
	}
	pad := int32(12)
	gutter := int32(0)
	if props.ShowLineNumbers {
		gutter = 58
	}
	viewH := max32(0, int32(props.Bounds.Height)-pad*2)
	contentH := int32(len(lines)) * lineH
	maxY := max32(0, contentH-viewH)
	if props.ScrollY != nil {
		*props.ScrollY = clamp32(*props.ScrollY, 0, maxY)
		if pointInRect(r.mousePos.X, r.mousePos.Y, props.Bounds) && r.mouseWheel != 0 {
			*props.ScrollY = clamp32(*props.ScrollY-int32(r.mouseWheel)*lineH*3, 0, maxY)
		}
	}
	maxWidth := int32(0)
	for _, line := range lines {
		line = strings.ReplaceAll(line, "\t", "    ")
		maxWidth = max32(maxWidth, int32(runtimeTextWidth(line, font)))
	}
	viewW := max32(0, int32(props.Bounds.Width)-pad*2-gutter)
	maxX := max32(0, maxWidth-viewW+24)
	if props.ScrollX != nil {
		*props.ScrollX = clamp32(*props.ScrollX, 0, maxX)
	}
	scrollY, scrollX := int32(0), int32(0)
	if props.ScrollY != nil {
		scrollY = *props.ScrollY
	}
	if props.ScrollX != nil {
		scrollX = *props.ScrollX
	}
	theme := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: theme.surface, BorderColor: theme.border})
	first := scrollY / lineH
	yOffset := scrollY % lineH
	for i := first; i < int32(len(lines)); i++ {
		y := props.Bounds.Y + float32(pad+(i-first)*lineH-yOffset)
		if y >= props.Bounds.Y+props.Bounds.Height-float32(pad) {
			break
		}
		if props.ShowLineNumbers {
			r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X + float32(pad), Y: y, Width: float32(gutter), Height: float32(lineH)}, Text: fmt.Sprint(i + 1), Color: theme.icon, FontSize: font, Row: i})
		}
		line := strings.ReplaceAll(lines[i], "\t", "    ")
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X + float32(pad+gutter-scrollX), Y: y, Width: float32(maxWidth), Height: float32(lineH)}, Text: line, Color: theme.text, FontSize: font, Row: i})
	}
	if maxX > 0 || maxY > 0 {
		return 1
	}
	return 0
}
func (r *runtime) ListBox(props ListBoxProps) int32 {
	props = normalizeListBoxProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 30
	}
	maxScroll := max32(0, int32(len(props.Items))*rowH-int32(props.Bounds.Height))
	if props.ScrollOffset != nil {
		*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
	}
	changed := int32(0)
	if pointInRect(r.mousePos.X, r.mousePos.Y, props.Bounds) && props.ScrollOffset != nil && r.mouseWheel != 0 {
		*props.ScrollOffset = clamp32(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, 0, maxScroll)
		changed = 1
	}
	if props.ID != 0 {
		r.registerField(props.ID)
	}
	changed |= r.recordListBoxOps(props, rowH)
	return changed
}
func (r *runtime) TableView(props TableViewProps) int32 {
	props = normalizeTableViewProps(props)
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
			changed |= setTableSelection(props, row, col, row, col)
			r.tableDrag = tableDrag{active: true, id: props.ID, startRow: row, startCol: col}
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

	if props.ID != 0 && r.focusID == props.ID {
		changed |= r.handleTableKeys(props)
	}

	r.record(FrameOp{Kind: FrameOpTable, Bounds: props.Bounds, ID: props.ID})
	r.drawTableOps(props, rowH, headerH)
	return changed
}
func (r *runtime) MessageDialog(props MessageDialogProps) int32 {
	result, _ := r.drawActionModal(props.Title, props.Message, []string{props.OKLabel}, 0)
	return result
}
func (r *runtime) ConfirmDialog(props ConfirmDialogProps) int32 {
	result, _ := r.drawActionModal(props.Title, props.Message, []string{props.CancelLabel, props.ConfirmLabel}, 0)
	return result
}
func (r *runtime) PromptDialog(props PromptDialogProps) int32 {
	escape := false
	for _, event := range r.inputEvents {
		if !event.shortcut && event.key == KeyEscape {
			escape = true
		}
	}
	result, field := r.drawActionModal(props.Title, "", []string{props.CancelLabel, props.ConfirmLabel}, 38)
	commit := false
	if props.Text != nil && props.Cursor != nil && props.Focused != nil {
		r.editText(field, props.Text, props.Cursor, props.Focused, &commit, 7301, int32(len(props.Text)-1), false)
		r.recordTextInput(FrameOpTextField, field, props.Text, props.Cursor, props.Focused, 7301, Text16, false)
	}
	if result == 0 && commit {
		result = 2
	}
	if result == 0 && escape {
		result = 1
	}
	return result
}
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

func themeSettingsText(value, fallback string) string {
	if value != "" {
		return value
	}
	return fallback
}

func themeSettingsThemeLabel(id int32) string {
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

// DefaultThemeForThemeStyle mirrors GetDefaultThemeForThemeStyle(): the
// palette an app should pair with a widget style when it has no opinion.
func DefaultThemeForThemeStyle(style ThemeStyle) ThemeId {
	switch style {
	case ThemeStyleRetro:
		return ThemeMono
	case ThemeStyleMaterial:
		return ThemeSweet
	default:
		return ThemeMono
	}
}

func ThemeSettings(props ThemeSettingsProps, state *UIThemeSettingsState, result *UIThemeSettingsResult) bool {
	if result != nil {
		*result = UIThemeSettingsResult{}
	}
	if props.ThemeSource == nil || props.ThemeMode == nil || props.ThemeId == nil || props.W <= 0 {
		return false
	}

	if state != nil {
		state.DrawSourceMenu = 0
		state.DrawModeMenu = 0
		state.DrawPaletteMenu = 0
		state.DrawStyleMenu = 0
	}

	changed := false
	id := props.IdBase
	if id == 0 {
		id = 9000
	}
	x, y, w := props.X, props.Y, props.W
	rowH := int32(34)
	rowGap := int32(14)
	labelGap := int32(22)

	rowButton := func(buttonID int32, label, value string) bool {
		Text(label, x, y, Text14, GetThemeText())
		pressed := Button(ButtonProps{
			Bounds: NewRectangle(float32(x), float32(y+labelGap), float32(w), float32(rowH)),
			Label:  value,
			Style:  ButtonStyleSecondary,
			Font:   Text14,
			ID:     buttonID,
		})
		y += labelGap + rowH + rowGap
		return pressed
	}

	if *props.ThemeSource < int32(ThemeSourceApp) || *props.ThemeSource > int32(ThemeSourceSystem) {
		*props.ThemeSource = int32(ThemeSourceApp)
	}
	if *props.ThemeMode < int32(ThemeModeSystem) || *props.ThemeMode > int32(ThemeModeDark) {
		*props.ThemeMode = int32(ThemeModeSystem)
	}
	if *props.ThemeId < 0 || *props.ThemeId >= int32(ThemeCount) {
		*props.ThemeId = int32(ThemeMono)
	}

	modeValue := themeSettingsText(props.ModeSystemLabel, "System")
	switch ThemeMode(*props.ThemeMode) {
	case ThemeModeLight:
		modeValue = themeSettingsText(props.ModeLightLabel, "Light")
	case ThemeModeDark:
		modeValue = themeSettingsText(props.ModeDarkLabel, "Dark")
	}
	if rowButton(id+1, themeSettingsText(props.ModeLabel, "Mode"), modeValue) {
		previous := *props.ThemeMode
		if props.AllowSystemMode != 0 {
			*props.ThemeMode = (*props.ThemeMode + 1) % 3
		} else if ThemeMode(*props.ThemeMode) == ThemeModeLight {
			*props.ThemeMode = int32(ThemeModeDark)
		} else {
			*props.ThemeMode = int32(ThemeModeLight)
		}
		if previous != *props.ThemeMode {
			changed = true
			if result != nil {
				result.ModeChanged = 1
			}
		}
	}

	paletteValue := themeSettingsThemeLabel(*props.ThemeId)
	if props.AllowSystemSource != 0 && ThemeSource(*props.ThemeSource) == ThemeSourceSystem {
		paletteValue = themeSettingsText(props.SourceSystemLabel, "System")
	}
	if rowButton(id+2, themeSettingsText(props.PaletteLabel, "Color"), paletteValue) {
		previousSource := *props.ThemeSource
		previousTheme := *props.ThemeId
		if props.AllowSystemSource != 0 && ThemeSource(*props.ThemeSource) != ThemeSourceSystem {
			*props.ThemeSource = int32(ThemeSourceSystem)
		} else {
			*props.ThemeSource = int32(ThemeSourceApp)
			*props.ThemeId = (*props.ThemeId + 1) % int32(ThemeCount)
		}
		if previousSource != *props.ThemeSource {
			changed = true
			if result != nil {
				result.SourceChanged = 1
			}
		}
		if previousTheme != *props.ThemeId {
			changed = true
			if result != nil {
				result.PaletteChanged = 1
			}
		}
	}

	if props.ThemeStyle != nil {
		if *props.ThemeStyle < int32(ThemeStyleSystem) || *props.ThemeStyle > int32(ThemeStyleMaterial) {
			*props.ThemeStyle = int32(ThemeStyleSystem)
		}
		styleValue := themeSettingsText(props.StyleSystemLabel, "System style")
		switch ThemeStyle(*props.ThemeStyle) {
		case ThemeStyleRetro:
			styleValue = themeSettingsText(props.StyleRetroLabel, "Retro")
		case ThemeStyleMaterial:
			styleValue = themeSettingsText(props.StyleMaterialLabel, "Material")
		}
		if rowButton(id+3, themeSettingsText(props.StyleLabel, "Style"), styleValue) {
			previous := *props.ThemeStyle
			*props.ThemeStyle = (*props.ThemeStyle + 1) % 3
			if previous != *props.ThemeStyle {
				changed = true
				if result != nil {
					result.StyleChanged = 1
				}
			}
		}
	}
	if result != nil && changed {
		result.Changed = 1
	}
	return changed
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

func (r *runtime) pushGrid(props GridProps) {
	bounds := r.layoutRect(props.Bounds)
	padding := float32(props.Padding)
	columns := props.Columns
	if columns < 1 {
		columns = 1
	}
	r.layout = append(r.layout, layoutFrame{
		bounds:  bounds,
		cursorX: bounds.X + padding,
		cursorY: bounds.Y + padding,
		gap:     float32(props.Gap),
		padding: padding,
		columns: columns,
	})
	r.record(FrameOp{Kind: FrameOpGrid, Bounds: bounds, ID: int32(props.Key), Columns: columns})
}

func (r *runtime) pushGroup(props ColumnProps, kind FrameOpKind) {
	bounds := props.Bounds
	r.layout = append(r.layout, layoutFrame{
		bounds:   bounds,
		noLayout: true,
	})
	r.record(FrameOp{Kind: kind, Bounds: bounds, ID: int32(props.Key)})
}

func (r *runtime) layoutRect(bounds Rectangle) Rectangle {
	if len(r.layout) == 0 || bounds.X != 0 || bounds.Y != 0 {
		return bounds
	}
	frame := &r.layout[len(r.layout)-1]
	if frame.noLayout {
		return bounds
	}
	if frame.columns > 0 {
		out := bounds
		columns := frame.columns
		innerW := frame.bounds.Width - frame.padding*2
		cellW := innerW
		if columns > 1 {
			cellW = (innerW - frame.gap*float32(columns-1)) / float32(columns)
		}
		if cellW < 0 {
			cellW = 0
		}
		col := frame.cellIndex % columns
		out.X = frame.cursorX + float32(col)*(cellW+frame.gap)
		out.Y = frame.cursorY
		if out.Width <= 0 {
			out.Width = cellW
		}
		if out.Height <= 0 {
			out.Height = frame.bounds.Height - frame.padding*2
		}
		if out.Height > frame.rowHeight {
			frame.rowHeight = out.Height
		}
		frame.cellIndex++
		if frame.cellIndex%columns == 0 {
			frame.cursorY += frame.rowHeight + frame.gap
			frame.rowHeight = 0
		}
		return out
	}
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

func pageBoundsOrView(bounds Rectangle, viewWidth, viewHeight int32) Rectangle {
	if bounds.Width <= 0 {
		bounds.Width = float32(viewWidth)
	}
	if bounds.Height <= 0 {
		bounds.Height = float32(viewHeight)
	}
	return bounds
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
			if os.Getenv("KRYON_DEBUG_TAPS") != "" {
				log.Printf("tap (%.0f,%.0f) consumed by bounds=%v", r.taps[i].x, r.taps[i].y, bounds)
			}
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

func cellW(grid Grid) float32 {
	return (grid.Bounds.Width - float32(grid.PadX*2) - float32(max32(0, grid.Cols-1)*grid.GapX)) / float32(grid.Cols)
}

func cellH(grid Grid) float32 {
	return (grid.Bounds.Height - float32(grid.PadY*2) - float32(max32(0, grid.Rows-1)*grid.GapY)) / float32(grid.Rows)
}

func (r *runtime) recordListBoxOps(props ListBoxProps, rowH int32) int32 {
	theme := r.theme()
	if rowH <= 0 {
		rowH = 30
	}
	changed := int32(0)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: theme.surface, BorderColor: theme.border, ID: props.ID})
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	first := scroll / rowH
	yOffset := scroll % rowH
	visible := int32(props.Bounds.Height) / rowH
	font := Text16
	for i := int32(0); i <= visible && first+i < int32(len(props.Items)); i++ {
		index := first + i
		row := Rectangle{
			X:      props.Bounds.X,
			Y:      props.Bounds.Y + float32(i*rowH-yOffset),
			Width:  props.Bounds.Width,
			Height: float32(rowH),
		}
		if props.SelectedIndex != nil && r.consumeTap(row) {
			if *props.SelectedIndex != index {
				*props.SelectedIndex = index
				changed = 1
			}
			if props.ID != 0 {
				r.setFocus(props.ID)
			}
		}
		selected := props.SelectedIndex != nil && *props.SelectedIndex == index
		if selected {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: theme.button, ID: props.ID, Row: index, Selected: true})
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + 8, Y: row.Y + 4, Width: row.Width - 16, Height: row.Height}, Text: elideText(props.Items[index], row.Width-16, font), Color: theme.text, FontSize: font, ID: props.ID, Row: index, Selected: selected})
	}
	return changed
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

func TableCellRect(props TableViewProps, row, col int32) Rectangle {
	props = normalizeTableViewProps(props)
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
	col := int32(0)
	if selectedCol >= 0 {
		col = clamp32(selectedCol, 0, int32(len(props.Columns)-1))
	}
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
			col = clamp32(col-1, 0, int32(len(props.Columns)-1))
			changed = 1
			selectionChanged = true
		case KeyRight:
			col = clamp32(col+1, 0, int32(len(props.Columns)-1))
			changed = 1
			selectionChanged = true
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
		rect := TableCellRect(TableViewProps{Bounds: props.Bounds, Columns: props.Columns, Rows: []TableRow{{}}, ColumnWidths: props.ColumnWidths, RowHeight: rowH}, 0, col)
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
			cellTextColor := theme.text
			if int(row) < len(props.Rows) {
				tableRow := props.Rows[row]
				if c < len(tableRow.BackgroundColors) && tableRow.BackgroundColors[c].A != 0 {
					r.record(FrameOp{Kind: FrameOpRect, Bounds: rect, Color: tableRow.BackgroundColors[c], Row: row, Column: col})
				}
				if c < len(tableRow.TextColors) && tableRow.TextColors[c].A != 0 {
					cellTextColor = tableRow.TextColors[c]
				}
			}
			if tableCellSelected(props, row, col, selectedRow, selectedCol) {
				r.record(FrameOp{Kind: FrameOpRect, Bounds: rect, Color: theme.selectedHot, Row: row, Column: col, Selected: true, SelectionStartRow: valueOr32(props.SelectionStartRow, -1), SelectionStartCol: valueOr32(props.SelectionStartColumn, -1), SelectionEndRow: valueOr32(props.SelectionEndRow, -1), SelectionEndCol: valueOr32(props.SelectionEndColumn, -1)})
				cellTextColor = theme.selectedText
			}
			text := ""
			if int(row) < len(props.Rows) && c < len(props.Rows[row].Cells) {
				text = props.Rows[row].Cells[c]
			}
			r.record(FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect), Text: elideText(text, rect.Width-12, font), Color: cellTextColor, FontSize: font, Row: row, Column: col})
		}
	}
}

func tableTextBounds(rect Rectangle) Rectangle {
	return Rectangle{X: rect.X + 6, Y: rect.Y + 6, Width: rect.Width - 12, Height: rect.Height - 8}
}

func tableCellAt(props TableViewProps, body Rectangle, rowH int32, x, y float32) (int32, int32) {
	props = normalizeTableViewProps(props)
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
	props = normalizeTableViewProps(props)
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
	props = normalizeTableViewProps(props)
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

func runtimeTextWidth(text string, font int32) int {
	if text == "" {
		return 0
	}
	return int(MeasureTextEx(Font{}, text, float32(font), 1).X)
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
