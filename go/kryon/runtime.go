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

type TabItemButtonProps struct {
	Bounds   Rectangle
	ID       int32
	Label    string
	Font     int32
	Disabled bool
}

type Tab struct {
	Label     string
	Icon      Texture2D
	IconSize  int32
	Disabled  bool
	Accent    Color
	Italic    bool
	Closeable bool
}

type TabBarProps struct {
	Bounds             Rectangle
	Tabs               []Tab
	Count              int32
	SelectedIndex      int32
	Font               int32
	MinTabWidth        int32
	MaxTabWidth        int32
	ScrollOffset       *int32
	FocusSelected      bool
	ClosedIndex        *int32
	DoubleClickedIndex *int32
	ReorderedFromIndex *int32
	ReorderedToIndex   *int32
	SelectedTabBounds  *Rectangle
	MiddleClickedIndex *int32
	ID                 int32
	Disabled           bool
}

type ClosableTabBarProps struct {
	Bounds        Rectangle
	Tabs          []Tab
	Count         int32
	SelectedIndex *int32
	Font          int32
	ClosedIndex   *int32
	ID            int32
	Disabled      bool
}

type tabBarScope struct {
	count    int32
	selected int32
	itemOpen bool
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

type DragDropSourceProps struct {
	Bounds   Rectangle
	ID       int32
	Type     string
	Data     []byte
	DataSize int32
	Disabled bool
}

type DragDropTargetProps struct {
	Bounds       Rectangle
	ID           int32
	Type         string
	Output       []byte
	OutputSize   int32
	AcceptedSize *int32
	Disabled     bool
}

type MultiSelectListProps struct {
	Bounds        Rectangle
	ID            int32
	Items         []string
	ItemCount     int32
	Selected      []int32
	SelectedCount *int32
	Anchor        *int32
	RowHeight     int32
	Disabled      bool
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
	ReadOnly       bool
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
	ReadOnly       bool
}

type TextWrap int32

const (
	TextWrapAuto TextWrap = iota
	TextWrapNone
)

type TextAlign int32

const (
	TextAlignStart TextAlign = iota
	TextAlignCenter
	TextAlignEnd
)

type TextProps struct {
	Bounds        Rectangle
	Text          string
	Font          int32
	Color         Color
	Wrap          TextWrap
	Align         TextAlign
	VerticalAlign TextAlign
	Disabled      bool
}

type ComboFlags uint32

const (
	ComboFlagsNone      ComboFlags = 0
	ComboPopupAlignLeft ComboFlags = 1 << (iota - 1)
	ComboHeightSmall
	ComboHeightRegular
	ComboHeightLarge
	ComboHeightLargest
	ComboNoArrowButton
	ComboNoPreview
	ComboWidthFitPreview
)

type ComboProps struct {
	Bounds    Rectangle
	PopupSize Vector2
	Preview   string
	ID        int32
	Open      *bool
	Flags     ComboFlags
	Disabled  bool
}

type PopupFlags uint32

const (
	PopupFlagsNone PopupFlags = 0
	PopupTooltip   PopupFlags = 1 << (iota - 1)
	PopupModal
	PopupContext
)

type PopupProps struct {
	Bounds   Rectangle
	ID       int32
	Open     *bool
	Disabled bool
	Trigger  Rectangle
	Flags    PopupFlags
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
	Align    TextAlign
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
	Disabled      bool
	ContentHeight int32
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
	Disabled     bool
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
	ColumnEnabled        []int32
	ColumnOrder          []int32
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
	SortDirection        *int32
	ScrollOffset         *int32
	RowHeight            int32
	Disabled             bool
	Resizable            bool
	MinColumnWidth       int32
	FreezeRows           int32
	HeaderHeight         int32
	HeaderAngle          float32
	CustomCells          bool
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
	Bounds   Rectangle
	Label    string
	Open     *bool
	Tree     bool
	Depth    int32
	Leaf     bool
	Selected bool
	Disabled bool
	ID       int32
	Visible  *bool
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
	SubmitTextComposition(KryTextCompositionPhase, string, int32, int32) int32
	PollTextComposition(*KryTextCompositionEvent) int32
	ClearTextComposition()
	Close()
	WindowShouldClose() bool
	BeginFrame()
	EndFrame()
	BeginDisabled(bool)
	EndDisabled()
	BeginCombo(ComboProps) bool
	EndCombo()
	CloseCombo()
	BeginPopup(PopupProps) bool
	EndPopup()
	ClosePopup()
	BeginTabBar(TabBarProps, *int32) bool
	BeginTabItem(int32) bool
	EndTabItem()
	EndTabBar()
	AcceleratorPressed(Accelerator) int32
	DispatchAccelerators([]Accelerator, ...int32) int32
	ClearBackground(Color)
	Background(Color)
	Text(TextProps)
	TextFormat(string, ...any) string
	Scale(int32) int32
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
	BeginScroll(Rectangle, int32, *int32) Rectangle
	Button(ButtonProps) bool
	Selectable(SelectableProps) bool
	CheckboxFlags(CheckboxFlagsProps) bool
	ImageWithBg(ImageWithBgProps)
	ImageButton(ImageButtonProps) bool
	TabItemButton(TabItemButtonProps) bool
	ClosableTabBar(ClosableTabBarProps) int32
	SmallButton(ButtonProps) bool
	InvisibleButton(InvisibleButtonProps) bool
	ArrowButton(ArrowButtonProps) bool
	Bullet(Rectangle)
	Separator(Rectangle, int32)
	SeparatorText(SeparatorTextProps)
	DragDropSource(DragDropSourceProps) bool
	DragDropTarget(DragDropTargetProps) bool
	MultiSelectList(MultiSelectListProps) int32
	ColorEdit3(ColorEditProps) bool
	ColorEdit4(ColorEditProps) bool
	ColorPicker3(ColorEditProps) bool
	ColorPicker4(ColorEditProps) bool
	ColorButton(ColorButtonProps) bool
	TabBar(TabBarProps) int32
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
	LabelText(label, value string, bounds Rectangle, fontSize int32, color Color)
	BulletText(text string, bounds Rectangle, fontSize int32, color Color)
	ValueBool(prefix string, value bool, bounds Rectangle, fontSize int32, color Color)
	ValueInt(prefix string, value int32, bounds Rectangle, fontSize int32, color Color)
	ValueUInt(prefix string, value uint32, bounds Rectangle, fontSize int32, color Color)
	ValueFloat(prefix string, value float32, format string, bounds Rectangle, fontSize int32, color Color)
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
	BeginListBox(ListBoxProps) Rectangle
	EndListBox()
	SourceView(props SourceViewProps) int32
	TableView(props TableViewProps) int32
	BeginTableCell(TableViewProps, int32, int32) Rectangle
	EndTableCell()
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
	config            AppConfig
	closed            bool
	frames            int
	focusID           int32
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
	dropdownsSeen     map[int32]bool
	popupPanels       map[int32]popupInputPanel
	popupInputScopes  []popupInputToken
	popupInputOrder   uint64
	paintLayers       []paintLayer
	paintLayerScopes  []paintLayerScope
	paintLayerFrame   uint64
	comboScopes       []comboScope
	openCombos        map[int32]*bool
	combosSeen        map[int32]bool
	popupScopes       []popupScope
	tabBarScopes      []tabBarScope
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
	toastUntil        time.Time
	currentThemeID    ThemeId
	themeSource       ThemeSource
	themeMode         ThemeMode
	themeStyle        ThemeStyle
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
	numericEditDragFloat int32 = 3 + iota
	numericEditDragInt
	numericEditSliderFloat
	numericEditSliderInt
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

type menuNavigation struct {
	Top  int32
	Path []int
}

func New(config AppConfig) Runtime {
	ensureDefaultUIFont()
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
		menuNavigation: map[int32]*menuNavigation{},
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

func (r *runtime) Close()                  { r.closed = true }
func (r *runtime) WindowShouldClose() bool { return r.closed || r.frames > 0 }
func (r *runtime) BeginFrame() {
	if len(r.popupInputScopes) != 0 {
		panic("unclosed popup input scope at frame boundary")
	}
	r.resetPaintLayers()
	if r.dropdownsSeen == nil {
		r.dropdownsSeen = make(map[int32]bool)
	}
	clear(r.dropdownsSeen)
	if r.combosSeen == nil {
		r.combosSeen = make(map[int32]bool)
	}
	clear(r.combosSeen)
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
	if len(r.comboScopes) != 0 {
		panic("unclosed combo scope at frame boundary")
	}
	if len(r.popupScopes) != 0 {
		panic("unclosed popup scope at frame boundary")
	}
	if len(r.tabBarScopes) != 0 {
		panic("unclosed tab bar scope at frame boundary")
	}
	for id, open := range r.openCombos {
		if !r.combosSeen[id] {
			if open != nil {
				*open = false
			}
			delete(r.openCombos, id)
			r.closePopupInput(id)
		}
	}
	for id, open := range r.openPopups {
		if !r.popupsSeen[id] {
			if open != nil {
				*open = false
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
		return r.openDropdowns[id] || r.openCombos[id] != nil && *r.openCombos[id] ||
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
func (r *runtime) BeginDisabled(disabled bool) {
	r.disabledStack = append(r.disabledStack, disabled)
	if disabled {
		r.disabledCount++
	}
}
func (r *runtime) EndDisabled() {
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
func (r *runtime) Background(c Color) {
	r.record(FrameOp{Kind: FrameOpBackground, Color: c})
}
func (r *runtime) Text(props TextProps) {
	r.textWithFont(props, 0)
}
func (r *runtime) textWithFont(props TextProps, fontID uint32) {
	font := props.Font
	if font <= 0 {
		font = Text16
	}
	color := props.Color
	if color.A == 0 {
		color = r.GetThemeText()
	}
	if props.Disabled {
		color = r.Fade(color, 0.45)
	}
	bounded := props.Bounds.Width > 0
	bounds := props.Bounds
	if bounds.Width <= 0 {
		bounds.Width = float32(runtimeTextWidthWithFont(props.Text, font, fontID))
		props.Wrap = TextWrapNone
	}
	lines := []string{props.Text}
	if bounded && props.Wrap == TextWrapAuto {
		lines = wrapRuntimeText(props.Text, bounds.Width, font)
	}
	lineHeight := float32(textHeight(font, fontID) + 2)
	if bounds.Height <= 0 {
		bounds.Height = max(float32(textHeight(font, fontID)), float32(len(lines))*lineHeight-2)
	}
	if bounds.X == 0 && bounds.Y == 0 {
		bounds = r.layoutRect(bounds)
	}
	contentHeight := max(float32(textHeight(font, fontID)), float32(len(lines))*lineHeight-2)
	startY := bounds.Y
	if props.VerticalAlign == TextAlignCenter {
		startY += (bounds.Height - contentHeight) / 2
	} else if props.VerticalAlign == TextAlignEnd {
		startY += bounds.Height - contentHeight
	}
	for i, line := range lines {
		y := startY + float32(i)*lineHeight
		if y+float32(textHeight(font, fontID)) > bounds.Y+bounds.Height {
			break
		}
		x := bounds.X
		lineWidth := float32(runtimeTextWidthWithFont(line, font, fontID))
		if props.Align == TextAlignCenter {
			x += (bounds.Width - lineWidth) / 2
		} else if props.Align == TextAlignEnd {
			x += bounds.Width - lineWidth
		}
		r.record(FrameOp{Kind: FrameOpText,
			Bounds: Rectangle{X: x, Y: y, Width: lineWidth, Height: float32(textHeight(font, fontID))},
			Clip:   bounds, HasClip: true, Text: line, Color: color, FontSize: font,
			FontID: fontID, Disabled: props.Disabled})
	}
}
func (r *runtime) TextFormat(format string, args ...any) string       { return fmt.Sprintf(format, args...) }
func (r *runtime) Scale(px int32) int32                               { return px }
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
func (r *runtime) BeginScroll(bounds Rectangle, contentHeight int32, offset *int32) Rectangle {
	clip := r.scrollClip(bounds)
	if offset != nil {
		maximum := max32(0, contentHeight-int32(bounds.Height))
		*offset = clamp32(*offset, 0, maximum)
		if r.pointerCanReach(clip) && r.mouseWheel != 0 {
			*offset = clamp32(*offset-int32(r.mouseWheel*42), 0, maximum)
			r.mouseWheel = 0
		}
		if maximum > 0 && bounds.Width > 10 && bounds.Height > 0 {
			track := Rectangle{X: bounds.X + bounds.Width - 10, Y: bounds.Y, Width: 10, Height: bounds.Height}
			thumbH := min(bounds.Height, max(float32(16), bounds.Height*bounds.Height/float32(contentHeight)))
			travel := bounds.Height - thumbH
			thumbY := bounds.Y
			if travel > 0 {
				thumbY += travel * float32(*offset) / float32(maximum)
			}
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
			t := r.theme()
			r.record(FrameOp{Kind: FrameOpRect, Bounds: track, Color: t.surface})
			r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: track.X + 2, Y: thumbY, Width: 6, Height: thumbH}, Color: t.button})
			bounds.Width -= 10
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
func (r *runtime) EndScroll() {
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
	props.Bounds = r.layoutRect(props.Bounds)
	return r.buttonAt(props)
}

// buttonAt applies the canonical Button interaction and paint contract to an
// already-laid-out rectangle. Composite widgets use it for embedded buttons
// without advancing their parent's layout a second time.
func (r *runtime) buttonAt(props ButtonProps) bool {
	theme := r.theme()
	pressed, focused := r.focusablePress(props.Bounds, props.ID, props.Disabled)
	fill := theme.button
	if props.Style == ButtonStyleSecondary {
		fill = theme.surface
	}
	if props.Disabled {
		fill = mixColor(theme.surface, theme.button, 0.5)
	}
	if pressed {
		fill = theme.buttonHover
	}
	textColor := theme.text
	if props.Disabled {
		textColor = theme.icon
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: props.Bounds, Text: props.Label, Color: fill, BorderColor: theme.buttonHover, TextColor: textColor, ID: props.ID, FontSize: props.Font, Disabled: props.Disabled, Pressed: pressed, Focused: focused})
	return pressed
}

// focusablePress is the common interaction contract for button-like widgets:
// pointer focus, Enter/Space activation, Tab traversal, disabled scopes and
// popup keyboard ownership all stay identical across their different paints.
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
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X + 8, Y: props.Bounds.Y + 6, Width: props.Bounds.Width - 16, Height: props.Bounds.Height}, Text: props.Label, Color: textColor, FontSize: Text14, ID: props.ID, Disabled: props.Disabled, Pressed: pressed, Selected: selected, Focused: focused})
	return pressed
}

func (r *runtime) CheckboxFlags(props CheckboxFlagsProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	checked := props.Flags != nil && (*props.Flags&props.FlagsValue) == props.FlagsValue
	disabled := props.Disabled || props.Flags == nil
	pressed, focused := r.focusablePress(props.Bounds, props.ID, disabled)
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
	border := theme.border
	if focused {
		border = theme.focus
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: box, Color: theme.surface, BorderColor: border, ID: props.ID, Disabled: disabled, Pressed: pressed, Selected: checked, Focused: focused})
	if checked {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: box.X + 4, Y: box.Y + 4, Width: 12, Height: 12}, Color: theme.circle})
	}
	textColor := theme.text
	if disabled {
		textColor = r.Fade(textColor, 0.45)
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: box.X + 28, Y: props.Bounds.Y + 5, Width: props.Bounds.Width - 28, Height: props.Bounds.Height}, Text: props.Label, Color: textColor, FontSize: Text14, Disabled: disabled})
	return pressed
}

func (r *runtime) ImageWithBg(props ImageWithBgProps) {
	bounds := r.layoutRect(props.Picture.Bounds)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: props.Background})
	r.record(FrameOp{Kind: FrameOpPicture, Bounds: bounds, Text: props.Picture.AssetPath, Color: props.Picture.Tint})
}

func (r *runtime) ImageButton(props ImageButtonProps) bool {
	bounds := r.layoutRect(props.Picture.Bounds)
	pressed, focused := r.focusablePress(bounds, props.ID, props.Disabled)
	border := r.theme().border
	if focused {
		border = r.theme().focus
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: bounds, Color: props.Background, BorderColor: border, ID: props.ID, Disabled: props.Disabled, Pressed: pressed, Focused: focused})
	r.record(FrameOp{Kind: FrameOpPicture, Bounds: bounds, Text: props.Picture.AssetPath, Color: props.Picture.Tint, Disabled: props.Disabled})
	return pressed
}

func (r *runtime) TabItemButton(props TabItemButtonProps) bool {
	font := props.Font
	if font <= 0 {
		font = Text14
	}
	return r.Button(ButtonProps{Bounds: props.Bounds, Label: props.Label, Style: ButtonStyleSecondary, Font: font, ID: props.ID, Disabled: props.Disabled})
}

func (r *runtime) SmallButton(props ButtonProps) bool {
	if props.Font <= 0 {
		props.Font = Text14
	}
	return r.Button(props)
}

func (r *runtime) InvisibleButton(props InvisibleButtonProps) bool {
	props.Bounds = r.layoutRect(props.Bounds)
	pressed, _ := r.focusablePress(props.Bounds, props.ID, props.Disabled)
	return pressed
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

func (r *runtime) DragDropSource(props DragDropSourceProps) bool {
	if r.dragDrop.active && r.dragDrop.sourceID == props.ID &&
		!r.mouseDown[MouseButtonLeft] && !r.mouseReleased[MouseButtonLeft] {
		r.dragDrop = dragDropState{}
	}
	if props.Disabled || r.contentDisabled() || props.Type == "" {
		return false
	}
	bounds := r.layoutRect(props.Bounds)
	if r.mousePressed[MouseButtonLeft] && r.pointerCanReach(bounds) {
		size := int(props.DataSize)
		if size <= 0 || size > len(props.Data) {
			size = len(props.Data)
		}
		r.dragDrop = dragDropState{active: true, sourceID: props.ID, typeName: props.Type, data: append([]byte(nil), props.Data[:size]...)}
	}
	return r.dragDrop.active && r.dragDrop.sourceID == props.ID &&
		(r.mouseDown[MouseButtonLeft] || r.mouseReleased[MouseButtonLeft])
}

func (r *runtime) DragDropTarget(props DragDropTargetProps) bool {
	if props.AcceptedSize != nil {
		*props.AcceptedSize = 0
	}
	bounds := r.layoutRect(props.Bounds)
	matches := r.dragDrop.active && props.Type != "" && r.dragDrop.typeName == props.Type
	hot := !props.Disabled && r.pointerCanReach(bounds)
	if matches {
		color := r.theme().border
		if hot {
			color = r.theme().link
		}
		r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, BorderColor: color, Disabled: props.Disabled, Selected: hot})
	}
	if props.Disabled || r.contentDisabled() || !matches || !hot || !r.mouseReleased[MouseButtonLeft] {
		return false
	}
	size := int(props.OutputSize)
	if size <= 0 || size > len(props.Output) {
		size = len(props.Output)
	}
	if size > len(r.dragDrop.data) {
		size = len(r.dragDrop.data)
	}
	copy(props.Output[:size], r.dragDrop.data[:size])
	if props.AcceptedSize != nil {
		*props.AcceptedSize = int32(size)
	}
	r.dragDrop = dragDropState{}
	r.mouseReleased[MouseButtonLeft] = false
	return true
}

func (r *runtime) MultiSelectList(props MultiSelectListProps) int32 {
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
	rowHeight := props.RowHeight
	if rowHeight <= 0 {
		rowHeight = 28
	}
	disabled := props.Disabled || r.contentDisabled()
	if !disabled {
		r.registerField(props.ID)
	}
	clicked := int32(-1)
	rangeAnchor := int32(-1)
	if !disabled {
		for i := 0; i < count; i++ {
			row := Rectangle{X: bounds.X, Y: bounds.Y + float32(i*int(rowHeight)), Width: bounds.Width, Height: float32(rowHeight)}
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
		if props.Anchor != nil && *props.Anchor >= 0 && int(*props.Anchor) < count {
			cursor = *props.Anchor
		} else {
			for i := 0; i < count; i++ {
				if props.Selected[i] != 0 {
					cursor = int32(i)
					break
				}
			}
		}
		if cursor < 0 {
			cursor = 0
		}
		next := cursor
		navigate := true
		switch {
		case r.keyDown[KeyHome]:
			next = 0
		case r.keyDown[KeyEnd]:
			next = int32(count - 1)
		case r.keyDown[KeyUp]:
			if next > 0 {
				next--
			}
		case r.keyDown[KeyDown]:
			if int(next)+1 < count {
				next++
			}
		case r.keyDown[KeySpace]:
			clicked, control, shift = cursor, true, false
			navigate = false
		case r.keyDown[KeyEnter]:
			clicked, control, shift = cursor, false, false
			navigate = false
		default:
			navigate = false
		}
		if navigate {
			if shift {
				rangeAnchor = cursor
				control = true
			}
			if props.Anchor != nil {
				*props.Anchor = next
			}
			if !control || shift {
				clicked = next
			}
		}
	}
	if clicked >= 0 {
		i := int(clicked)
		anchor := int32(-1)
		if rangeAnchor >= 0 {
			anchor = rangeAnchor
		} else if props.Anchor != nil {
			anchor = *props.Anchor
		}
		if shift && anchor >= 0 && int(anchor) < count {
			first, last := int(anchor), i
			if first > last {
				first, last = last, first
			}
			if !control {
				clear(props.Selected[:count])
			}
			for j := first; j <= last; j++ {
				props.Selected[j] = 1
			}
		} else if control {
			if props.Selected[i] != 0 {
				props.Selected[i] = 0
			} else {
				props.Selected[i] = 1
			}
			if props.Anchor != nil {
				*props.Anchor = clicked
			}
		} else {
			clear(props.Selected[:count])
			props.Selected[i] = 1
			if props.Anchor != nil {
				*props.Anchor = clicked
			}
		}
	}
	theme := r.theme()
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
	for i := 0; i < count; i++ {
		selected := props.Selected[i] != 0
		if selected {
			selectedCount++
		}
		row := Rectangle{X: bounds.X, Y: bounds.Y + float32(i*int(rowHeight)), Width: bounds.Width, Height: float32(rowHeight)}
		fill := theme.surface
		if selected {
			fill = theme.buttonHover
		}
		textColor := theme.text
		if disabled {
			textColor = r.Fade(textColor, 0.45)
		}
		rowFocused := focusRow == int32(i)
		border := theme.border
		if rowFocused {
			border = theme.focus
		}
		r.record(FrameOp{Kind: FrameOpButton, Bounds: row, Text: props.Items[i], Color: fill, BorderColor: border, TextColor: textColor, FontSize: Text14, ID: props.ID, Row: int32(i), Selected: selected, Disabled: disabled, Pressed: int32(i) == clicked, Focused: rowFocused})
	}
	if props.SelectedCount != nil {
		*props.SelectedCount = selectedCount
	}
	return clicked
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
	pressed, focused := r.focusablePress(props.Bounds, props.ID, props.Disabled)
	t := r.theme()
	halfW, halfH := props.Bounds.Width/2, props.Bounds.Height/2
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: Color{180, 180, 180, 255}})
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: halfW, Height: halfH}, Color: Color{220, 220, 220, 255}})
	r.record(FrameOp{Kind: FrameOpRect, Bounds: Rectangle{X: props.Bounds.X + halfW, Y: props.Bounds.Y + halfH, Width: halfW, Height: halfH}, Color: Color{220, 220, 220, 255}})
	border := t.border
	if focused {
		border = t.focus
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: props.Bounds, Text: props.Label, Color: props.Color, BorderColor: border, TextColor: t.text, FontSize: Text14, ID: props.ID, Disabled: props.Disabled, Pressed: pressed, Focused: focused})
	return pressed
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
	font := props.Font
	if font <= 0 {
		font = Text12
	}
	minWidth := float32(props.MinTabWidth)
	if minWidth <= 0 {
		minWidth = 120
	}
	maxWidth := float32(props.MaxTabWidth)
	if maxWidth <= 0 {
		maxWidth = minWidth
	}
	if maxWidth < minWidth {
		maxWidth = minWidth
	}
	widths := make([]float32, count)
	totalWidth := float32(0)
	for i := 0; i < count; i++ {
		w := float32(runtimeTextWidth(props.Tabs[i].Label, font) + 16)
		if props.Tabs[i].Closeable {
			w += 24
		}
		w = min(maxWidth, max(minWidth, w))
		widths[i], totalWidth = w, totalWidth+w
	}
	equalTabs := totalWidth <= bounds.Width
	if equalTabs {
		for i := range widths {
			widths[i] = bounds.Width / float32(count)
		}
		totalWidth = bounds.Width
	}
	localScroll := int32(0)
	scroll := props.ScrollOffset
	if scroll == nil {
		if props.ID > 0 {
			if r.tabScroll == nil {
				r.tabScroll = make(map[int32]int32)
			}
			localScroll = r.tabScroll[props.ID]
			r.tabBarsSeen[props.ID] = true
		}
		scroll = &localScroll
	}
	maxScroll := int32(max(float32(0), totalWidth-bounds.Width))
	*scroll = min(maxScroll, max(0, *scroll))
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
			x += widths[i]
		}
		if x < bounds.X {
			*scroll = max(0, *scroll-int32(bounds.X-x))
		} else if end := x + widths[selected]; end > bounds.X+bounds.Width {
			*scroll = min(maxScroll, *scroll+int32(end-(bounds.X+bounds.Width)))
		}
	}
	theme := r.theme()
	x := bounds.X - float32(*scroll)
	for i := 0; i < count; i++ {
		item := props.Tabs[i]
		tab := Rectangle{X: x, Y: bounds.Y, Width: widths[i], Height: bounds.Height}
		x += widths[i]
		itemDisabled := disabled || item.Disabled
		isSelected := int32(i) == selected
		if isSelected && props.SelectedTabBounds != nil {
			*props.SelectedTabBounds = tab
		}
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
		if !itemDisabled {
			if _, middle := r.consumeMouseButtonPoint(MouseButtonMiddle, intersectRectangles(tab, bounds)); middle && props.MiddleClickedIndex != nil {
				*props.MiddleClickedIndex = int32(i)
			}
		}
		fill, textColor, border := theme.surface, theme.icon, theme.border
		hovered := !itemDisabled && r.pointerCanReach(intersectRectangles(tab, bounds))
		if isSelected || int32(i) == selected {
			fill, textColor = theme.buttonHover, theme.text
		} else if hovered {
			fill, textColor = theme.button, theme.text
		}
		if itemDisabled {
			textColor = r.Fade(textColor, 0.45)
		}
		if focused && int32(i) == selected {
			border = theme.focus
		}
		r.record(FrameOp{Kind: FrameOpButton, Bounds: tab, Clip: bounds, HasClip: true,
			Text: fitTabLabel(item.Label, tab.Width-closeWidth-12, font), Color: fill,
			BorderColor: border, TextColor: textColor, FontSize: font, ID: props.ID,
			Disabled: itemDisabled, Pressed: int32(i) == selected, Focused: focused && int32(i) == selected, Row: int32(i)})
		if item.Closeable {
			r.record(FrameOp{Kind: FrameOpText, Bounds: closeBounds, Clip: bounds, HasClip: true, Text: "×", Color: textColor,
				FontSize: font, Disabled: itemDisabled, Pressed: closed, Row: int32(i)})
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
				x += width
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
				x += width
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

func (r *runtime) ClosableTabBar(props ClosableTabBarProps) int32 {
	selected := int32(0)
	if props.SelectedIndex != nil {
		selected = *props.SelectedIndex
	}
	minWidth := int32(0)
	if props.Count > 0 && props.Bounds.Width > 0 {
		minWidth = int32(props.Bounds.Width) / props.Count
	}
	clicked := r.TabBar(TabBarProps{Bounds: props.Bounds, Tabs: props.Tabs,
		Count: props.Count, SelectedIndex: selected, Font: props.Font,
		MinTabWidth: minWidth, MaxTabWidth: minWidth, ClosedIndex: props.ClosedIndex,
		ID: props.ID, Disabled: props.Disabled})
	if clicked >= 0 && props.SelectedIndex != nil {
		*props.SelectedIndex = clicked
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
			switch event.key {
			case KeyHome:
				if minimum < maximum {
					next, handled = minimum, true
				}
			case KeyEnd:
				if minimum < maximum {
					next, handled = maximum, true
				}
			default:
				direction := r.sliderKeyboardDirection(false, event.key)
				if direction == 0 {
					break
				}
				step := speed
				if r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt] {
					step *= 0.1
				}
				if event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift] {
					step *= 10
				}
				next += float32(direction) * step
				if minimum < maximum {
					next = min(maximum, max(minimum, next))
				}
				handled = true
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
	next := int64(value)
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			switch event.key {
			case KeyHome:
				if minimum < maximum {
					next, handled = int64(minimum), true
				}
			case KeyEnd:
				if minimum < maximum {
					next, handled = int64(maximum), true
				}
			default:
				direction := r.sliderKeyboardDirection(false, event.key)
				if direction == 0 {
					break
				}
				step := speed
				if r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt] {
					step *= 0.1
				}
				if event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift] {
					step *= 10
				}
				delta := int64(math.Round(float64(step)))
				if delta == 0 {
					if step < 0 {
						delta = -1
					} else {
						delta = 1
					}
				}
				next += int64(direction) * delta
				if minimum < maximum {
					next = min(int64(maximum), max(int64(minimum), next))
				}
				handled = true
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return int32(next), int32(next) != value
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
		focusID := sliderFocusID(props.ID, int32(i), false)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempFloat(cell,
			numericInputKey{kind: numericEditDragFloat, widgetID: props.ID, component: int32(i)},
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
		if delta, dragged := r.dragDelta(props.ID*16+int32(i)+1, focusID, cell, props.Disabled); dragged {
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
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, props.Values[i]), !enabled, focused, props.ID, int32(i))
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
		focusID := sliderFocusID(props.ID, int32(i), true)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempInt(cell,
			numericInputKey{kind: numericEditDragInt, widgetID: props.ID, component: int32(i)},
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
		if delta, dragged := r.dragDelta(props.ID*16+int32(i)+1, focusID, cell, props.Disabled); dragged {
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
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, props.Values[i]), !enabled, focused, props.ID, int32(i))
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
		if delta, dragged := r.dragDelta(props.ID*16+int32(i)+1, focusID, cell, props.Disabled); dragged {
			value := *values[i] + delta*speed
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
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, *values[i]), !enabled, focused, props.ID, int32(i))
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
		if delta, dragged := r.dragDelta(props.ID*16+int32(i)+1, focusID, cell, props.Disabled); dragged {
			scaled := delta * speed
			step := int32(scaled + 0.5)
			if scaled < 0 {
				step = int32(scaled - 0.5)
			}
			value := *values[i] + step
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
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawDragCell(cell, fmt.Sprintf(format, *values[i]), !enabled, focused, props.ID, int32(i))
	}
	if *props.CurrentMin > *props.CurrentMax {
		*props.CurrentMin = *props.CurrentMax
	}
	r.drawDragLabel(props.Bounds, props.Label)
	return changed
}

func (r *runtime) drawDragCell(bounds Rectangle, text string, disabled, focused bool, id, component int32) {
	t := r.theme()
	color := t.button
	textColor := t.text
	if disabled {
		color, textColor = t.surface, t.icon
	}
	border := t.border
	if focused {
		border = t.focus
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: bounds, Text: text, Color: color, BorderColor: border, TextColor: textColor, FontSize: Text14, ID: id, Row: component, Disabled: disabled, Pressed: r.drag.active && r.drag.token == id*16+component+1, Focused: focused})
}

func (r *runtime) drawDragLabel(bounds Rectangle, label string) {
	if label == "" {
		return
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + 6, Y: bounds.Y - 18, Width: bounds.Width - 12, Height: 16}, Text: label, Color: r.theme().text, FontSize: Text14})
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
	textChanged := r.editText(bounds, state.text, &state.cursor, &state.focused, &commit, focusID, 63, false, false)
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
	if id <= 0 {
		return 0
	}
	if component == 0 {
		return id
	}
	prefix := int32(0x40000000)
	if integer {
		prefix = 0x50000000
	}
	return (prefix ^ (id*16 + component + 1)) & 0x7fffffff
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
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) || maximum <= minimum {
		return value, false
	}
	next := value
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			switch event.key {
			case KeyHome:
				next, handled = minimum, true
			case KeyEnd:
				next, handled = maximum, true
			default:
				if direction := r.sliderKeyboardDirection(vertical, event.key); direction != 0 {
					step := (maximum - minimum) * 0.01
					if r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt] {
						step *= 0.1
					}
					if event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift] {
						step *= 10
					}
					next = min(maximum, max(minimum, next+float32(direction)*step))
					handled = true
				}
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
	if focusID <= 0 || r.focusID != focusID || r.popupFocusCaptures(focusID) || maximum <= minimum {
		return value, false
	}
	next := int64(value)
	rangeValue := int64(maximum) - int64(minimum)
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if !event.shortcut {
			switch event.key {
			case KeyHome:
				next, handled = int64(minimum), true
			case KeyEnd:
				next, handled = int64(maximum), true
			default:
				if direction := r.sliderKeyboardDirection(vertical, event.key); direction != 0 {
					step := int64(1)
					if rangeValue > 100 {
						step = (rangeValue + 50) / 100
					}
					if r.keyDown[KeyLeftAlt] || r.keyDown[KeyRightAlt] {
						step /= 10
						if step < 1 {
							step = 1
						}
					}
					if event.shift || r.keyDown[KeyLeftShift] || r.keyDown[KeyRightShift] {
						step *= 10
					}
					next += int64(direction) * step
					if next < int64(minimum) {
						next = int64(minimum)
					} else if next > int64(maximum) {
						next = int64(maximum)
					}
					handled = true
				}
			}
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	return int32(next), int32(next) != value
}

func (r *runtime) drawSliderCell(bounds Rectangle, ratio float32, text string, disabled, vertical, focused bool, id, component int32) {
	t := r.theme()
	base, accent, textColor := t.button, t.buttonHover, t.text
	if disabled {
		base, accent, textColor = t.surface, t.icon, t.icon
	}
	border := t.border
	if focused {
		border = t.focus
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: base, BorderColor: border, ID: id, Row: component, Disabled: disabled, Focused: focused})
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
		focusID := sliderFocusID(props.ID, int32(i), false)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempFloat(cell,
			numericInputKey{kind: numericEditSliderFloat, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		ratio := float32(0)
		if rangeValue > 0 {
			ratio = (props.Values[i] - props.Min) / rangeValue
		}
		if ratio < 0 {
			ratio = 0
		} else if ratio > 1 {
			ratio = 1
		}
		if enabled {
			if next, keyboardChanged := r.sliderFloatKeyboard(focusID, vertical, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				ratio = (next - props.Min) / rangeValue
				changed = true
			}
		}
		if next, active := r.sliderRatio(0x40000000^(props.ID*16+int32(i)+1), focusID, cell, props.Disabled, vertical); active && rangeValue > 0 {
			ratio = next
			value := props.Min + ratio*rangeValue
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%.3f"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawSliderCell(cell, ratio, fmt.Sprintf(format, props.Values[i]), props.Disabled, vertical, focused, props.ID, int32(i))
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
	rangeValue := int64(props.Max) - int64(props.Min)
	changed := false
	for i := 0; i < count; i++ {
		focusID := sliderFocusID(props.ID, int32(i), true)
		cell := Rectangle{X: props.Bounds.X + float32(i)*props.Bounds.Width/float32(count), Y: props.Bounds.Y, Width: props.Bounds.Width / float32(count), Height: props.Bounds.Height}
		enabled := !props.Disabled && !r.contentDisabled()
		if enabled {
			r.registerField(focusID)
		}
		edited, editing := r.numericTempInt(cell,
			numericInputKey{kind: numericEditSliderInt, widgetID: props.ID, component: int32(i)},
			focusID, props.Values, i, props.Format, props.Disabled)
		changed = changed || edited
		if editing {
			continue
		}
		ratio := float32(0)
		if rangeValue > 0 {
			ratio = float32(float64(int64(props.Values[i])-int64(props.Min)) / float64(rangeValue))
		}
		if ratio < 0 {
			ratio = 0
		} else if ratio > 1 {
			ratio = 1
		}
		if enabled {
			if next, keyboardChanged := r.sliderIntKeyboard(focusID, vertical, props.Min, props.Max, props.Values[i]); keyboardChanged {
				props.Values[i] = next
				ratio = float32(float64(int64(next)-int64(props.Min)) / float64(rangeValue))
				changed = true
			}
		}
		if next, active := r.sliderRatio(0x50000000^(props.ID*16+int32(i)+1), focusID, cell, props.Disabled, vertical); active && rangeValue > 0 {
			ratio = next
			value := int32(int64(props.Min) + int64(float64(ratio)*float64(rangeValue)+0.5))
			changed = changed || value != props.Values[i]
			props.Values[i] = value
		}
		format := props.Format
		if format == "" {
			format = "%d"
		}
		focused := enabled && focusID > 0 && r.focusID == focusID && !r.popupFocusCaptures(focusID)
		r.drawSliderCell(cell, ratio, fmt.Sprintf(format, props.Values[i]), props.Disabled, vertical, focused, props.ID, int32(i))
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

func (r *runtime) numericInputCell(bounds Rectangle, key numericInputKey, formatted string, disabled bool, step, stepFast float64) (string, float64, bool) {
	state := r.numericInputState(key, formatted)
	token := state.token
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
		textChanged = r.editText(field, state.text, &state.cursor, &state.focused, &commit, token, 63, false, false)
	}
	r.recordTextInput(FrameOpTextField, field, state.text, &state.cursor, &state.focused, token, Text14, false, false)
	if step == 0 {
		return string(state.text[:zeroIndex(state.text)]), 0, textChanged
	}
	minusPressed := r.buttonAt(ButtonProps{Bounds: minus, Label: "-", Font: Text14,
		ID: token + 1, Disabled: disabled})
	plusPressed := r.buttonAt(ButtonProps{Bounds: plus, Label: "+", Font: Text14,
		ID: token + 2, Disabled: disabled})
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
		key := numericInputKey{kind: 0, widgetID: props.ID, component: int32(i)}
		text, increment, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, float64(props.Step), float64(props.StepFast))
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseFloat(text, 32); err == nil {
				value, valid = float32(parsed), true
			}
		}
		if increment != 0 {
			value, valid = value+float32(increment), true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
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
		key := numericInputKey{kind: 1, widgetID: props.ID, component: int32(i)}
		text, increment, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, float64(props.Step), float64(props.StepFast))
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseInt(text, 0, 32); err == nil {
				value, valid = int32(parsed), true
			}
		}
		if increment != 0 {
			value, valid = value+int32(increment), true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
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
		key := numericInputKey{kind: 2, widgetID: props.ID, component: int32(i)}
		text, increment, edited := r.numericInputCell(cell, key, fmt.Sprintf(format, props.Values[i]), props.Disabled, props.Step, props.StepFast)
		value := props.Values[i]
		valid := false
		if edited {
			if parsed, err := strconv.ParseFloat(text, 64); err == nil {
				value, valid = parsed, true
			}
		}
		if increment != 0 {
			value, valid = value+increment, true
			r.setNumericInputText(key, fmt.Sprintf(format, value))
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
	pressed, focused := r.focusablePress(bounds, id, value == nil)
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
	border := theme.border
	if focused {
		border = theme.focus
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: boxBounds, Color: theme.button, BorderColor: border, ID: id, Pressed: pressed, Selected: *value != 0, Focused: focused})
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
	if r.dropdownsSeen == nil {
		r.dropdownsSeen = make(map[int32]bool)
	}
	r.dropdownsSeen[id] = true
	if r.contentDisabled() {
		r.closeDropdown(id)
	}
	if selected != nil && len(labels) > 0 {
		*selected = clamp32(*selected, 0, int32(len(labels)-1))
	}
	theme := r.theme()
	pressed := r.consumeTap(bounds)
	if !r.contentDisabled() && id > 0 {
		r.registerField(id)
		if pressed {
			r.setFocus(id)
		}
		if r.focusID == id && !r.openDropdowns[id] && !r.popupKeyboardCaptures() &&
			(r.keyDown[KeyEnter] || r.keyDown[335] || r.keyDown[KeySpace] || r.keyDown[KeyDown]) {
			pressed = true
		}
	}
	if pressed {
		for other := range r.openDropdowns {
			if other != id {
				r.closeDropdown(other)
			}
		}
		r.openDropdowns[id] = !r.openDropdowns[id]
		if r.openDropdowns[id] {
			if r.dropdownHighlight == nil {
				r.dropdownHighlight = make(map[int32]int32)
			}
			r.dropdownHighlight[id] = 0
			if selected != nil {
				r.dropdownHighlight[id] = *selected
			}
		}
	}
	open := r.openDropdowns[id]
	panel := r.dropdownPanel(bounds, len(labels))
	if open && !pressed {
		if r.mousePressed[MouseButtonLeft] && !pointInRect(r.mousePos.X, r.mousePos.Y, bounds) && !pointInRect(r.mousePos.X, r.mousePos.Y, panel) {
			open = false
		}
		for _, tap := range r.taps {
			if !pointInRect(tap.x, tap.y, bounds) && !pointInRect(tap.x, tap.y, panel) {
				open = false
			}
		}
	}
	if len(labels) == 0 || bounds.Height <= 0 || r.keyDown[KeyEscape] {
		open = false
	}
	changed := false
	if open && !pressed {
		highlight := clamp32(r.dropdownHighlight[id], 0, int32(len(labels)-1))
		if r.keyDown[KeyUp] {
			highlight = max32(0, highlight-1)
		} else if r.keyDown[KeyDown] {
			highlight = min32(int32(len(labels)-1), highlight+1)
		} else if r.keyDown[KeyHome] {
			highlight = 0
		} else if r.keyDown[KeyEnd] {
			highlight = int32(len(labels) - 1)
		}
		if r.dropdownHighlight == nil {
			r.dropdownHighlight = make(map[int32]int32)
		}
		r.dropdownHighlight[id] = highlight
		if r.keyDown[KeyEnter] || r.keyDown[335] {
			if selected != nil {
				changed = *selected != highlight
				*selected = highlight
			}
			open = false
		}
	}
	if !open {
		r.closeDropdown(id)
	}
	border := theme.border
	focused := !r.contentDisabled() && id > 0 && r.focusID == id
	if focused {
		border = theme.focus
	}
	r.record(FrameOp{Kind: FrameOpButton, Bounds: bounds, Text: selectedLabel(labels, selected), Color: theme.surface, BorderColor: border, TextColor: theme.text, ID: id, FontSize: Text16, Pressed: pressed, Focused: focused})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: bounds.X + bounds.Width - 24, Y: bounds.Y + 5, Width: 16, Height: bounds.Height}, Text: "x", Color: theme.text, FontSize: Text14, ID: id})
	if !open {
		return pressed || changed
	}
	itemH := bounds.Height
	layer := r.beginPaintLayer(id)
	input := r.beginPopupInput(id, panel)
	defer func() {
		r.endPopupInput(input)
		r.endPaintLayer(layer)
	}()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: panel, Color: theme.surface, BorderColor: theme.border, ID: id})
	if r.dropdownOffsets == nil {
		r.dropdownOffsets = make(map[int32]*int32)
	}
	offset := r.dropdownOffsets[id]
	if offset == nil {
		offset = new(int32)
		r.dropdownOffsets[id] = offset
	}
	viewport := panel
	viewport.Y += 4
	viewport.Height = max(float32(0), viewport.Height-8)
	if pressed || r.keyDown[KeyUp] || r.keyDown[KeyDown] || r.keyDown[KeyHome] || r.keyDown[KeyEnd] {
		top := float32(r.dropdownHighlight[id]) * itemH
		if top < float32(*offset) {
			*offset = int32(top)
		} else if top+itemH > float32(*offset)+viewport.Height {
			*offset = int32(top + itemH - viewport.Height)
		}
	}
	content := r.BeginScroll(viewport, int32(min(float64(itemH)*float64(len(labels)), float64(2147483647))), offset)
	defer r.EndScroll()
	first := max(0, int(float32(*offset)/itemH))
	last := min(len(labels), int(math.Ceil(float64((float32(*offset)+viewport.Height)/itemH))))
	for i := first; i < last; i++ {
		label := labels[i]
		row := Rectangle{X: content.X, Y: content.Y + float32(i)*itemH, Width: content.Width, Height: itemH}
		selectedRow := selected != nil && int32(i) == *selected
		if selectedRow || r.dropdownHighlight[id] == int32(i) {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: mixColor(theme.surface, theme.button, 0.35), ID: id, Row: int32(i), Selected: selectedRow, Focused: r.dropdownHighlight[id] == int32(i)})
		}
		if selected != nil && r.consumeTap(row) {
			next := int32(i)
			if *selected != next {
				*selected = next
				changed = true
			}
			r.closeDropdown(id)
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

func (r *runtime) ValueBool(prefix string, value bool, bounds Rectangle, fontSize int32, color Color) {
	r.LabelText(prefix, strconv.FormatBool(value), bounds, fontSize, color)
}

func (r *runtime) ValueInt(prefix string, value int32, bounds Rectangle, fontSize int32, color Color) {
	r.LabelText(prefix, strconv.FormatInt(int64(value), 10), bounds, fontSize, color)
}

func (r *runtime) ValueUInt(prefix string, value uint32, bounds Rectangle, fontSize int32, color Color) {
	r.LabelText(prefix, strconv.FormatUint(uint64(value), 10), bounds, fontSize, color)
}

func (r *runtime) ValueFloat(prefix string, value float32, format string, bounds Rectangle, fontSize int32, color Color) {
	if format == "" {
		format = "%.3f"
	}
	r.LabelText(prefix, fmt.Sprintf(format, value), bounds, fontSize, color)
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
	pressed, focused := r.focusablePress(bounds, id, false)
	if pressed {
		if *value == 0 {
			*value = 1
		} else {
			*value = 0
		}
	}
	border := theme.border
	if focused {
		border = theme.focus
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: mixColor(theme.background, theme.surface, 0.65), BorderColor: border, ID: id, Focused: focused})
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
		if item.Icon.ID != 0 {
			tint := props.IconColor
			if tint.A == 0 {
				tint = Color{R: 255, G: 255, B: 255, A: 255}
			}
			if item.Disabled {
				tint.A = uint8(uint32(tint.A) * 150 / 255)
			}
			size := props.IconSize
			if size <= 0 {
				size = 26
			}
			r.Icon(item.Route, int32(ib.X)+(int32(ib.Width)-size)/2, int32(ib.Y)+6, size, int32(item.Icon.ID), tint)
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

func (r *runtime) MenuBar(id int32, bounds Rectangle, menus []Menu, openIndex *int32) MenuBarResult {
	theme := r.theme()
	result := MenuBarResult{OpenIndex: -1}
	state := r.menuNav(id)
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
		if open < 0 {
			switch {
			case r.keyDown[KeyLeft]:
				state.Top = (state.Top + int32(len(menus)) - 1) % int32(len(menus))
			case r.keyDown[KeyRight]:
				state.Top = (state.Top + 1) % int32(len(menus))
			case r.keyDown[KeyHome]:
				state.Top = 0
			case r.keyDown[KeyEnd]:
				state.Top = int32(len(menus) - 1)
			case r.keyDown[KeyEnter] || r.keyDown[335] || r.keyDown[KeySpace] || r.keyDown[KeyDown]:
				open = state.Top
				r.openMenus[id] = open
				resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
				openedByKeyboard = true
			}
		} else if r.keyDown[KeyEscape] {
			open = -1
			delete(r.openMenus, id)
			delete(r.openSubmenus, id)
			state.Path = state.Path[:0]
		} else if len(state.Path) <= 1 && r.keyDown[KeyLeft] {
			open = (open + int32(len(menus)) - 1) % int32(len(menus))
			state.Top = open
			r.openMenus[id] = open
			delete(r.openSubmenus, id)
			resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
			openedByKeyboard = true
		} else if len(state.Path) <= 1 && r.keyDown[KeyRight] {
			selected := -1
			if len(state.Path) == 1 {
				selected = state.Path[0]
			}
			opensSubmenu := selected >= 0 && selected < len(limitedMenuItems(menus[open].Items, menus[open].ItemCount)) &&
				limitedMenuItems(menus[open].Items, menus[open].ItemCount)[selected].Kind == MenuSubmenu &&
				!limitedMenuItems(menus[open].Items, menus[open].ItemCount)[selected].Disabled
			if !opensSubmenu {
				open = (open + 1) % int32(len(menus))
				state.Top = open
				r.openMenus[id] = open
				delete(r.openSubmenus, id)
				resetMenuPath(state, limitedMenuItems(menus[open].Items, menus[open].ItemCount))
				openedByKeyboard = true
			}
		}
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: bounds, Color: theme.surface, BorderColor: theme.border})
	r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: bounds.X, Y: bounds.Y + bounds.Height - 1, Width: bounds.Width, Height: 0}, Color: theme.border})
	x := bounds.X + 4
	font := Text14
	for i, menu := range menus {
		w := float32(maxInt(44, runtimeTextWidth(menu.Label, font)+24))
		item := Rectangle{X: x, Y: bounds.Y + 3, Width: w, Height: bounds.Height - 6}
		if !r.contentDisabled() && r.consumeTap(item) {
			r.setFocus(id)
			idx := int32(i)
			if open == idx {
				idx = -1
			}
			open = idx
			if open < 0 {
				delete(r.openMenus, id)
			} else {
				r.openMenus[id] = open
				state.Top = open
				resetMenuPath(state, limitedMenuItems(menu.Items, menu.ItemCount))
			}
		}
		if open == int32(i) || focused && open < 0 && state.Top == int32(i) {
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
		handled := openedByKeyboard
		result.ActivatedID, _ = r.drawPopupMenu(id, int32(menuX), int32(bounds.Y+bounds.Height), items, id, 0, &handled)
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

func limitedMenuItems(items []MenuItem, count int32) []MenuItem {
	if count <= 0 || int(count) > len(items) {
		return items
	}
	return items[:count]
}

func (r *runtime) drawPopupMenu(id, x, y int32, items []MenuItem, focusID int32, depth int, handled *bool) (int32, Rectangle) {
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
			switch {
			case r.keyDown[KeyUp]:
				state.Path[depth] = menuItemAt(items, selected, -1)
				*handled = true
			case r.keyDown[KeyDown]:
				state.Path[depth] = menuItemAt(items, selected, 1)
				*handled = true
			case r.keyDown[KeyHome]:
				state.Path[depth] = menuFirstItem(items)
				*handled = true
			case r.keyDown[KeyEnd]:
				state.Path[depth] = menuLastItem(items)
				*handled = true
			case r.keyDown[KeyLeft] && depth > 0:
				state.Path = state.Path[:depth]
				*handled = true
			case r.keyDown[KeyRight] || r.keyDown[KeyEnter] || r.keyDown[335] || r.keyDown[KeySpace]:
				item := items[selected]
				if item.Kind == MenuSubmenu && len(limitedMenuItems(item.Submenu, item.SubmenuCount)) > 0 {
					r.openSubmenus[id] = item.ID
					state.Path = append(state.Path, menuFirstItem(limitedMenuItems(item.Submenu, item.SubmenuCount)))
					*handled = true
				} else if !item.Disabled && item.Kind != MenuSeparator {
					*handled = true
					return item.ID, panel
				}
			}
		}
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: panel, Color: theme.surface, BorderColor: theme.border})
	for i, item := range items {
		row := Rectangle{X: panel.X + 4, Y: panel.Y + 4 + float32(i)*rowH, Width: panel.Width - 8, Height: rowH}
		if item.Kind == MenuSeparator {
			r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: row.X + 8, Y: row.Y + row.Height/2, Width: row.Width - 16}, Color: theme.border})
			continue
		}
		hovered := !r.contentDisabled() && pointInRect(r.mousePos.X, r.mousePos.Y, row)
		selected := keyboard && len(state.Path) > depth && state.Path[depth] == i
		if hovered && !item.Disabled {
			if len(state.Path) > depth {
				state.Path[depth] = i
				state.Path = state.Path[:depth+1]
			}
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: theme.buttonHover})
			if item.Kind == MenuSubmenu {
				r.openSubmenus[id] = item.ID
			}
		}
		if selected && !hovered {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: theme.buttonHover})
		}
		if !item.Disabled && r.consumeTap(row) {
			r.setFocus(focusID)
			if item.Kind == MenuSubmenu {
				r.openSubmenus[id] = item.ID
			} else {
				return item.ID, panel
			}
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
			submenuOpen := r.openSubmenus[id] == item.ID
			if keyboard {
				submenuOpen = selected && len(state.Path) > depth+1
			}
			if submenuOpen {
				subitems := limitedMenuItems(item.Submenu, item.SubmenuCount)
				activated, _ := r.drawPopupMenu(item.ID, int32(row.X+row.Width), int32(row.Y), subitems, focusID, depth+1, handled)
				if activated != 0 {
					return activated, panel
				}
			}
		}
	}
	return 0, panel
}

func (r *runtime) PopupMenu(id, x, y int32, items []MenuItem, itemCount int32) int32 {
	items = limitedMenuItems(items, itemCount)
	if !r.contentDisabled() {
		r.registerField(id)
	}
	state := r.menuNav(id)
	if r.focusID == id && r.keyDown[KeyEscape] {
		state.Path = state.Path[:0]
		r.setFocus(0)
		return 0
	}
	handled := false
	selected, _ := r.drawPopupMenu(id, x, y, items, id, 0, &handled)
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
	if !r.contentDisabled() && r.mouseReleased[MouseButtonRight] && pointInRect(r.mousePos.X, r.mousePos.Y, props.Trigger) {
		r.contextMenus[props.ID] = r.mousePos
		r.setFocus(props.ID)
		resetMenuPath(r.menuNav(props.ID), limitedMenuItems(props.Items, props.ItemCount))
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
	if !r.contentDisabled() {
		r.registerField(props.ID)
	}
	if !r.contentDisabled() && r.focusID == props.ID && !r.popupFocusCaptures(props.ID) && r.keyDown[KeyEscape] {
		delete(r.contextMenus, props.ID)
		delete(r.openSubmenus, props.ID)
		r.menuNav(props.ID).Path = r.menuNav(props.ID).Path[:0]
		if props.Open != nil {
			*props.Open = 0
		}
		return 0
	}
	handled := false
	selected, panel := r.drawPopupMenu(props.ID, int32(pos.X), int32(pos.Y), limitedMenuItems(props.Items, props.ItemCount), props.ID, 0, &handled)
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
			*props.Open = 0
		}
	}
	return selected
}
func (r *runtime) CanvasGrid(bounds Rectangle, step int32, color Color) {
	bounds = r.layoutRect(bounds)
	spacing := r.Scale(step)
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
	if selected && !r.contentDisabled() && !r.popupKeyboardCaptures() {
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
	changed := r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, nil, props.FocusID, props.MaxCodepoints, false, props.ReadOnly)
	r.recordTextInput(FrameOpTextArea, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, props.Font, false, props.ReadOnly)
	return changed
}
func (r *runtime) Radio(props RadioButtonProps) int32 {
	props.Bounds = r.layoutRect(props.Bounds)
	pressed, focused := r.focusablePress(props.Bounds, props.ID, props.Disabled)
	c := r.theme().text
	if props.Disabled {
		c = r.theme().icon
	}
	mark := "○"
	if props.Checked {
		mark = "◉"
	}
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: 24, Height: props.Bounds.Height}, Text: mark, Color: c, FontSize: Text16, ID: props.ID, Pressed: pressed, Disabled: props.Disabled, Selected: props.Checked, Focused: focused})
	r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: props.Bounds.X + 28, Y: props.Bounds.Y, Width: props.Bounds.Width - 28, Height: props.Bounds.Height}, Text: props.Label, Color: c, FontSize: Text16, ID: props.ID, Pressed: pressed, Disabled: props.Disabled, Selected: props.Checked, Focused: focused})
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
	minus := r.buttonAt(ButtonProps{Bounds: l, Label: "-", Font: Text16,
		ID: p.ID*10 + 1, Disabled: p.Disabled})
	plus := r.buttonAt(ButtonProps{Bounds: rr, Label: "+", Font: Text16,
		ID: p.ID*10 + 2, Disabled: p.Disabled})
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
		r.closeDropdown(p.ID)
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
	if limit < p.MinFirst {
		limit = p.MinFirst
	}
	split = clamp32(split, p.MinFirst, limit)
	h := Rectangle{X: p.Bounds.X, Y: p.Bounds.Y + float32(split) - 4, Width: p.Bounds.Width, Height: 8}
	if p.Vertical {
		h = Rectangle{X: p.Bounds.X + float32(split) - 4, Y: p.Bounds.Y, Width: 8, Height: p.Bounds.Height}
	}
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
	if !r.mouseDown[MouseButtonLeft] && r.drag.token == p.ID {
		r.drag = scalarDrag{}
	}
	if *p.Split != split {
		*p.Split = split
		changed = 1
	}
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: h, Color: t.button, BorderColor: t.border, ID: p.ID, Pressed: changed != 0})
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
	p.Bounds.Height = 32
	p.Bounds = r.layoutRect(p.Bounds)
	if p.Tree && p.Depth > 0 {
		indent := min(float32(p.Depth)*20, p.Bounds.Width)
		p.Bounds.X += indent
		p.Bounds.Width -= indent
	}
	enabled := !p.Disabled && !r.contentDisabled()
	if enabled {
		r.registerField(p.ID)
		if p.Tree && p.ID > 0 {
			r.treeHeaders = append(r.treeHeaders, treeHeaderNav{p.ID, max(p.Depth, 0)})
		}
	}
	header := p.Bounds
	closeBounds := Rectangle{}
	body := header
	closed := false
	if p.Visible != nil {
		closeWidth := min(float32(28), header.Width)
		closeBounds = Rectangle{X: header.X + header.Width - closeWidth, Y: header.Y, Width: closeWidth, Height: header.Height}
		body.Width = max(float32(0), body.Width-closeWidth)
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
		*p.Open = !*p.Open
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
					old := *p.Open
					switch event.key {
					case KeyRight:
						*p.Open = true
						handled = true
					case KeyLeft:
						*p.Open = false
						handled = true
					case KeyEnter, KeySpace:
						*p.Open = !*p.Open
						handled = true
					}
					pressed = pressed || old != *p.Open
				}
			}
			if !handled {
				remaining = append(remaining, event)
			}
		}
		r.inputEvents = remaining
	}
	mark := ">"
	if p.Open != nil && *p.Open {
		mark = "v"
	}
	if p.Leaf {
		mark = "•"
	}
	t := r.theme()
	bg, border, fg := t.button, t.buttonHover, t.text
	if p.Tree {
		bg, border = BLANK, BLANK
	}
	if p.Selected {
		bg = t.buttonHover
	}
	if !enabled {
		fg.A = uint8(float32(fg.A) * 0.45)
	}
	label := elideText(mark+"  "+p.Label, body.Width-12, Text16)
	r.record(FrameOp{Kind: FrameOpButton, Bounds: header, Text: label, Color: bg, BorderColor: border, TextColor: fg, FontSize: Text16, Pressed: pressed, Selected: p.Selected, ID: p.ID, Focused: enabled && p.ID != 0 && r.focusID == p.ID, Disabled: !enabled})
	if p.Visible != nil {
		r.record(FrameOp{Kind: FrameOpText, Bounds: closeBounds, Text: "×", Color: fg, FontSize: Text16, Pressed: closed, Disabled: !enabled})
	}
	if pressed || closed {
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
		if !props.Disabled && r.pointerCanReach(props.Bounds) && r.mouseWheel != 0 {
			*props.ScrollOffset = clamp32(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, 0, maxScroll)
		}
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	theme := r.theme()
	panelColor, borderColor := theme.surface, theme.border
	if props.Disabled {
		panelColor, borderColor = r.Fade(panelColor, 0.45), r.Fade(borderColor, 0.45)
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: panelColor, BorderColor: borderColor, ID: props.ID, Disabled: props.Disabled})
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
		pressed := !props.Disabled && item.Selectable != 0 && r.consumeTap(row)
		if pressed && props.SelectedID != nil {
			*props.SelectedID = item.ID
			selected = true
			changed = 1
		}
		if selected {
			color := theme.button
			if props.Disabled {
				color = r.Fade(color, 0.45)
			}
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: color, ID: props.ID, Row: index, Selected: true, Disabled: props.Disabled})
		}
		indent := float32(8 + item.Depth*18)
		mark := ">"
		if item.Expanded != 0 {
			mark = "v"
		}
		iconColor, textColor := theme.icon, theme.text
		if props.Disabled {
			iconColor, textColor = r.Fade(iconColor, 0.45), r.Fade(textColor, 0.45)
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + indent, Y: row.Y + 4, Width: 16, Height: row.Height}, Text: mark, Color: iconColor, FontSize: Text16, ID: item.ID, Row: index, Disabled: props.Disabled})
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + indent + 18, Y: row.Y + 4, Width: row.Width - indent - 26, Height: row.Height}, Text: item.Label, Color: textColor, FontSize: Text16, ID: item.ID, Row: index, Pressed: pressed, Selected: selected, Disabled: props.Disabled})
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
		if r.pointerCanReach(props.Bounds) && r.mouseWheel != 0 {
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
func (r *runtime) BeginListBox(props ListBoxProps) Rectangle {
	props.Bounds = r.layoutRect(props.Bounds)
	r.BeginDisabled(props.Disabled)
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: t.surface, ID: props.ID, Role: "listbox"})
	for _, edge := range []Rectangle{
		{X: props.Bounds.X, Y: props.Bounds.Y, Width: props.Bounds.Width},
		{X: props.Bounds.X, Y: props.Bounds.Y + props.Bounds.Height - 1, Width: props.Bounds.Width},
		{X: props.Bounds.X, Y: props.Bounds.Y, Height: props.Bounds.Height},
		{X: props.Bounds.X + props.Bounds.Width - 1, Y: props.Bounds.Y, Height: props.Bounds.Height},
	} {
		r.record(FrameOp{Kind: FrameOpLine, Bounds: edge, Color: t.border, ID: props.ID})
	}
	inner := Rectangle{X: props.Bounds.X + 1, Y: props.Bounds.Y + 1, Width: max(float32(0), props.Bounds.Width-2), Height: max(float32(0), props.Bounds.Height-2)}
	height := props.ContentHeight
	if height <= 0 {
		rowH := props.RowHeight
		if rowH <= 0 {
			rowH = 30
		}
		height = int32(min(int64(1<<31-1), int64(max32(0, props.ItemCount))*int64(rowH)))
	}
	return r.BeginScroll(inner, height, props.ScrollOffset)
}

func (r *runtime) EndListBox() { r.EndScroll(); r.EndDisabled() }

func (r *runtime) ListBox(props ListBoxProps) int32 {
	props = normalizeListBoxProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	props.Disabled = props.Disabled || r.contentDisabled()
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 30
	}
	maxScroll := max32(0, int32(len(props.Items))*rowH-int32(props.Bounds.Height))
	if props.ScrollOffset != nil {
		*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
	}
	changed := int32(0)
	if !props.Disabled && r.pointerCanReach(props.Bounds) && props.ScrollOffset != nil && r.mouseWheel != 0 {
		*props.ScrollOffset = clamp32(*props.ScrollOffset-int32(r.mouseWheel)*rowH*3, 0, maxScroll)
		changed = 1
	}
	if !props.Disabled && props.ID != 0 {
		r.registerField(props.ID)
	}
	if !props.Disabled && props.ID != 0 && r.focusID == props.ID &&
		!r.popupFocusCaptures(props.ID) && props.SelectedIndex != nil && len(props.Items) > 0 {
		next := *props.SelectedIndex
		navigate := true
		switch {
		case r.keyDown[KeyHome]:
			next = 0
		case r.keyDown[KeyEnd]:
			next = int32(len(props.Items) - 1)
		case r.keyDown[KeyUp]:
			if next < 0 {
				next = int32(len(props.Items) - 1)
			} else {
				next--
			}
		case r.keyDown[KeyDown]:
			if next < 0 {
				next = 0
			} else {
				next++
			}
		default:
			navigate = false
		}
		if navigate {
			next = clamp32(next, 0, int32(len(props.Items)-1))
			if next != *props.SelectedIndex {
				*props.SelectedIndex = next
				changed = 1
			}
			if props.ScrollOffset != nil {
				top := next * rowH
				bottom := top + rowH
				viewport := int32(props.Bounds.Height)
				if top < *props.ScrollOffset {
					*props.ScrollOffset = top
				} else if bottom > *props.ScrollOffset+viewport {
					*props.ScrollOffset = bottom - viewport
				}
				*props.ScrollOffset = clamp32(*props.ScrollOffset, 0, maxScroll)
			}
		}
	}
	changed |= r.recordListBoxOps(props, rowH)
	return changed
}
func (r *runtime) TableView(props TableViewProps) int32 {
	props = normalizeTableViewProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	props.Disabled = props.Disabled || r.contentDisabled()
	if len(props.Columns) == 0 {
		return 0
	}

	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	headerH := max32(30, props.HeaderHeight)
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
	if r.tableResize.active && r.popupInputOwnerCaptures(r.tableResize.owner) {
		r.tableResize = tableResize{}
	}
	if r.tableResize.active && r.mouseReleased[MouseButtonLeft] &&
		(r.tableResize.id != props.ID || props.Disabled || !props.Resizable || len(props.ColumnWidths) == 0) {
		r.tableResize = tableResize{}
	}
	frozenRows := tableFrozenRows(props, rowH, int32(body.Height))
	maxScroll := max32(0, (int32(len(props.Rows))-frozenRows)*rowH-(int32(body.Height)-frozenRows*rowH))
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
			column, separatorX := tableSeparatorAtX(props, click.x-shift, 5)
			separatorX += shift
			if column < 0 || int(column) >= len(props.ColumnWidths) {
				continue
			}
			click.consumed = true
			r.consumeTap(Rectangle{X: separatorX - 5, Y: props.Bounds.Y, Width: 10, Height: float32(headerH)})
			r.tableResize = tableResize{active: true, id: props.ID, column: column, startX: click.x, startWidth: tableColumnWidth(props, column), owner: r.currentPopupInputOwner()}
			break
		}
		if r.tableResize.active && r.tableResize.id == props.ID {
			if r.mouseDown[MouseButtonLeft] {
				minimum := props.MinColumnWidth
				if minimum <= 0 {
					minimum = 32
				}
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
		r.editText(field, props.Text, props.Cursor, props.Focused, &commit, 7301, int32(len(props.Text)-1), false, false)
		r.recordTextInput(FrameOpTextField, field, props.Text, props.Cursor, props.Focused, 7301, Text16, false, false)
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
		Text(TextProps{Bounds: Rectangle{X: float32(x), Y: float32(y)}, Text: label, Font: Text14, Color: GetThemeText(), Wrap: TextWrapNone})
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
	r.editText(props.Bounds, props.Text, props.CursorPosition, props.Focused, props.CommitPressed, props.FocusID, props.MaxCodepoints, props.Secure, props.ReadOnly)
	r.recordTextInput(FrameOpTextField, props.Bounds, props.Text, props.CursorPosition, props.Focused, props.FocusID, props.Font, props.Secure, props.ReadOnly)
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

func (r *runtime) recordTextInput(kind FrameOpKind, bounds Rectangle, buf []byte, cursor *int32, focused *bool, focusID, font int32, secure, readOnly bool) {
	text := string(buf[:zeroIndex(buf)])
	if preedit, ok := r.preedit[focusID]; ok && r.focusID == focusID && !secure {
		pos := len(text)
		if cursor != nil {
			pos = clampCursor(text, int(*cursor))
		}
		text = text[:pos] + preedit.Text + text[pos:]
	}
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
		ReadOnly:          readOnly,
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

func (r *runtime) editText(bounds Rectangle, buf []byte, cursor *int32, focused *bool, commit *bool, focusID int32, maxCodepoints int32, secure, readOnly bool) bool {
	if readOnly {
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
		sel = selection{Anchor: pos, Cursor: pos}
	}
	changed := false
	for _, event := range r.inputEvents {
		if event.text != "" {
			if readOnly {
				continue
			}
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
				if readOnly {
					continue
				}
				if !secure && sel.Anchor != sel.Cursor {
					start, end := selectionRange(sel)
					r.clipboard = text[start:end]
					text = text[:start] + text[end:]
					pos = start
					sel = selection{Anchor: pos, Cursor: pos}
					changed = true
				}
			case KeyV:
				if readOnly {
					continue
				}
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
			if readOnly {
				continue
			}
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
			if readOnly {
				continue
			}
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
	var composed bool
	if readOnly {
		r.ClearTextComposition()
	} else {
		text, pos, sel, composed = r.editComposition(focusID, text, pos, sel, textLimit(buf, maxCodepoints))
	}
	changed = changed || composed
	if !readOnly {
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
	disabledColor := func(color Color) Color {
		if props.Disabled {
			return r.Fade(color, 0.45)
		}
		return color
	}
	if rowH <= 0 {
		rowH = 30
	}
	changed := int32(0)
	focused := !props.Disabled && props.ID != 0 && r.focusID == props.ID && !r.popupFocusCaptures(props.ID)
	border := theme.border
	if focused {
		border = theme.focus
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: disabledColor(theme.surface), BorderColor: disabledColor(border), ID: props.ID, Disabled: props.Disabled, Focused: focused})
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
		if selected {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: row, Color: disabledColor(theme.button), ID: props.ID, Row: index, Selected: true, Disabled: props.Disabled})
		}
		r.record(FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: row.X + 8, Y: row.Y + 4, Width: row.Width - 16, Height: row.Height}, Text: elideText(props.Items[index], row.Width-16, font), Color: disabledColor(theme.text), FontSize: font, ID: props.ID, Row: index, Selected: selected, Disabled: props.Disabled})
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

func (r *runtime) BeginTableCell(props TableViewProps, row, col int32) Rectangle {
	props = normalizeTableViewProps(props)
	cell := TableCellRect(props, row, col)
	header := max32(30, props.HeaderHeight)
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	bodyHeight := max32(0, int32(props.Bounds.Height)-header)
	frozen := tableFrozenRows(props, rowH, bodyHeight)
	top := props.Bounds.Y + float32(header)
	bottom := props.Bounds.Y + props.Bounds.Height
	if row < frozen {
		bottom = top + float32(frozen*rowH)
	} else {
		top += float32(frozen * rowH)
	}
	left, right := max(cell.X, props.Bounds.X), min(cell.X+cell.Width, props.Bounds.X+props.Bounds.Width)
	top, bottom = max(top, cell.Y), min(bottom, cell.Y+cell.Height)
	clip := Rectangle{X: left, Y: top, Width: max(float32(0), right-left), Height: max(float32(0), bottom-top)}
	r.BeginDisabled(props.Disabled)
	r.BeginScroll(clip, int32(clip.Height), nil)
	return cell
}

func (r *runtime) EndTableCell() { r.EndScroll(); r.EndDisabled() }

func TableCellRect(props TableViewProps, row, col int32) Rectangle {
	props = normalizeTableViewProps(props)
	if len(props.Columns) == 0 || row < 0 || col < 0 || int(row) >= len(props.Rows) || int(col) >= len(props.Columns) {
		return Rectangle{}
	}
	rowH := props.RowHeight
	if rowH <= 0 {
		rowH = 28
	}
	headerH := max32(30, props.HeaderHeight)
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
	bodyHeight := max32(0, int32(props.Bounds.Height)-headerH)
	frozenRows := tableFrozenRows(props, rowH, bodyHeight)
	y := props.Bounds.Y + float32(headerH)
	if row < frozenRows {
		y += float32(row * rowH)
	} else {
		y += float32(frozenRows*rowH + (row-frozenRows)*rowH - scroll)
	}
	return Rectangle{
		X:      x,
		Y:      y,
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
	theme := r.theme()
	disabledColor := func(color Color) Color {
		if props.Disabled {
			return r.Fade(color, 0.45)
		}
		return color
	}
	r.record(FrameOp{Kind: FrameOpRect, Bounds: props.Bounds, Color: disabledColor(theme.surface), Disabled: props.Disabled})
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
	font := Text12
	if rowH >= 28 {
		font = Text14
	}
	for _, col := range displayColumns {
		c := int(col)
		rect := TableCellRect(TableViewProps{Bounds: props.Bounds, Columns: props.Columns, Rows: []TableRow{{}}, ColumnWidths: props.ColumnWidths, ColumnEnabled: props.ColumnEnabled, ColumnOrder: props.ColumnOrder, RowHeight: rowH}, 0, col)
		rect.Y = props.Bounds.Y
		rect.Height = float32(headerH)
		fill := theme.button
		selected := false
		if selectedRow < 0 && selectedCol == col {
			fill = theme.selectedHot
			selected = true
		}
		shift := tableHeaderShift(props, rect.Y)
		var polygon [4]Vector2
		if shift != 0 {
			polygon = [4]Vector2{{rect.X + shift, rect.Y}, {rect.X + rect.Width + shift, rect.Y}, {rect.X + rect.Width, rect.Y + rect.Height}, {rect.X, rect.Y + rect.Height}}
		}
		headerClip := r.scrollClip(Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: props.Bounds.Width, Height: float32(headerH)})
		r.record(FrameOp{Kind: FrameOpRect, Bounds: rect, Polygon: polygon, HasPolygon: shift != 0, Color: disabledColor(fill), Row: -1, Column: col, Selected: selected, Disabled: props.Disabled})
		if shift != 0 {
			r.ops[len(r.ops)-1].Clip = headerClip
			r.ops[len(r.ops)-1].HasClip = true
		}
		textOp := FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect), Text: elideText(props.Columns[c], rect.Width-12, font), Color: disabledColor(theme.text), FontSize: font, Row: -1, Column: col, Disabled: props.Disabled}
		angle := props.HeaderAngle
		if math.IsNaN(float64(angle)) || math.IsInf(float64(angle), 0) {
			angle = 0
		}
		angle = max(float32(-89), min(float32(89), angle))
		if angle != 0 {
			textOp.Polygon = polygon
			textOp.HasPolygon = true
			textOp.Text, textOp.Rotation = props.Columns[c], angle
			textOp.Bounds.X, textOp.Bounds.Y = rect.X+6, rect.Y+6
			if angle > 0 {
				textOp.Bounds.X += shift
			}
			if angle < 0 {
				textOp.Bounds.Y = rect.Y + rect.Height - 6
			}
		}
		r.record(textOp)
		if angle != 0 {
			r.ops[len(r.ops)-1].Clip = headerClip
			r.ops[len(r.ops)-1].HasClip = true
		}
		if props.Resizable && c < len(props.ColumnWidths) {
			r.record(FrameOp{Kind: FrameOpLine, Bounds: Rectangle{X: rect.X + rect.Width + shift - 1, Y: rect.Y, Width: -shift, Height: rect.Height}, Color: disabledColor(theme.border), Column: col, Disabled: props.Disabled})
			r.ops[len(r.ops)-1].Clip, r.ops[len(r.ops)-1].HasClip = headerClip, true
		}
	}
	scroll := int32(0)
	if props.ScrollOffset != nil {
		scroll = *props.ScrollOffset
	}
	bodyHeight := max32(0, int32(props.Bounds.Height)-headerH)
	frozenRows := tableFrozenRows(props, rowH, bodyHeight)
	first := frozenRows
	if rowH > 0 {
		first += scroll / rowH
	}
	visible := int32(0)
	if rowH > 0 {
		visible = int32(props.Bounds.Height-float32(headerH))/rowH + 2
	}
	drawRow := func(row int32) {
		clip := Rectangle{X: props.Bounds.X, Y: props.Bounds.Y + float32(headerH), Width: props.Bounds.Width, Height: float32(frozenRows * rowH)}
		if row >= frozenRows {
			clip.Y += clip.Height
			clip.Height = props.Bounds.Y + props.Bounds.Height - clip.Y
		}
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
			r.record(FrameOp{Kind: FrameOpRect, Bounds: rowRect, Color: disabledColor(mixColor(theme.surface, theme.button, 0.16)), Row: row, Disabled: props.Disabled})
		}
		if row == selectedRow && selectedCol < 0 {
			r.record(FrameOp{Kind: FrameOpRect, Bounds: rowRect, Color: disabledColor(theme.selectedHot), Row: row, Column: -1, Selected: true, Disabled: props.Disabled})
		}
		for _, col := range displayColumns {
			c := int(col)
			rect := TableCellRect(props, row, col)
			if rect.Y+rect.Height < props.Bounds.Y+float32(headerH) || rect.Y > props.Bounds.Y+props.Bounds.Height {
				continue
			}
			cellTextColor := theme.text
			if int(row) < len(props.Rows) {
				tableRow := props.Rows[row]
				if c < len(tableRow.BackgroundColors) && tableRow.BackgroundColors[c].A != 0 {
					r.record(FrameOp{Kind: FrameOpRect, Bounds: rect, Color: disabledColor(tableRow.BackgroundColors[c]), Row: row, Column: col, Disabled: props.Disabled})
				}
				if c < len(tableRow.TextColors) && tableRow.TextColors[c].A != 0 {
					cellTextColor = tableRow.TextColors[c]
				}
			}
			if tableCellSelected(props, row, col, selectedRow, selectedCol) {
				r.record(FrameOp{Kind: FrameOpRect, Bounds: rect, Color: disabledColor(theme.selectedHot), Row: row, Column: col, Selected: true, Disabled: props.Disabled, SelectionStartRow: valueOr32(props.SelectionStartRow, -1), SelectionStartCol: valueOr32(props.SelectionStartColumn, -1), SelectionEndRow: valueOr32(props.SelectionEndRow, -1), SelectionEndCol: valueOr32(props.SelectionEndColumn, -1)})
				cellTextColor = theme.selectedText
			}
			text := ""
			if int(row) < len(props.Rows) && c < len(props.Rows[row].Cells) {
				text = props.Rows[row].Cells[c]
			}
			r.record(FrameOp{Kind: FrameOpText, Bounds: tableTextBounds(rect), Text: elideText(text, rect.Width-12, font), Color: disabledColor(cellTextColor), FontSize: font, Row: row, Column: col, Disabled: props.Disabled})
		}
	}
	for row := int32(0); row < frozenRows; row++ {
		drawRow(row)
	}
	for i := int32(0); i < visible && first+i < int32(len(props.Rows)); i++ {
		drawRow(first + i)
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
	frozenRows := tableFrozenRows(props, rowH, int32(body.Height))
	frozenHeight := float32(frozenRows * rowH)
	localY := y - body.Y
	row := int32(0)
	if localY < frozenHeight {
		row = int32(localY / float32(rowH))
	} else {
		row = frozenRows + int32((localY-frozenHeight+float32(scroll))/float32(rowH))
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
	return int32(props.Bounds.Width) / int32(visible)
}

func tableFrozenRows(props TableViewProps, rowH, bodyHeight int32) int32 {
	if rowH <= 0 || bodyHeight <= 0 {
		return 0
	}
	return clamp32(props.FreezeRows, 0, min32(int32(len(props.Rows)), bodyHeight/rowH))
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
