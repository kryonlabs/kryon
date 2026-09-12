package kryon

type FrameOpKind string

const (
	FrameOpBackground FrameOpKind = "background"
	FrameOpText       FrameOpKind = "text"
	FrameOpRect       FrameOpKind = "rect"
	FrameOpCircle     FrameOpKind = "circle"
	FrameOpRing       FrameOpKind = "ring"
	FrameOpSurface    FrameOpKind = "surface"
	FrameOpLine       FrameOpKind = "line"
	FrameOpButton     FrameOpKind = "button"
	FrameOpIcon       FrameOpKind = "icon"
	FrameOpTextField  FrameOpKind = "text_field"
	FrameOpTextArea   FrameOpKind = "text_area"
	FrameOpTable      FrameOpKind = "table"
	FrameOpColumn     FrameOpKind = "column"
	FrameOpRow        FrameOpKind = "row"
	FrameOpStack      FrameOpKind = "stack"
	FrameOpScreen     FrameOpKind = "screen"
	FrameOpGrid       FrameOpKind = "grid"
	FrameOpPage       FrameOpKind = "page"
	FrameOpSection    FrameOpKind = "section"
	FrameOpImage      FrameOpKind = "image"
	FrameOpEnd        FrameOpKind = "end"
)

type FrameOp struct {
	Kind              FrameOpKind
	Bounds            Rectangle
	SurfaceBounds     Rectangle
	Button            ButtonFrame
	AmbientColor      Color
	Disclosure        bool
	Polygon           [4]Vector2
	HasPolygon        bool
	Clip              Rectangle
	HasClip           bool
	Text              string
	Color             Color
	SecondaryColor    Color
	BackgroundEnd     Color
	HasBackgroundEnd  bool
	FillStates        FillStates
	FillStatesValid   bool
	BorderColor       Color
	FocusColor        Color
	TextColor         Color
	SelectionColor    Color
	SelectedTextColor Color
	CursorColor       Color
	FontSize          int32
	LetterSpacing     int32
	Rotation          float32
	Radius            float32
	BorderWidth       float32
	Opacity           float32
	Material          MaterialKind
	ContentOffset     Vector2
	Gap               float32
	FontID            uint32
	ID                int32
	FocusID           int32
	Cursor            int32
	SelectionStart    int32
	SelectionEnd      int32
	CompositionStart  int32
	CompositionEnd    int32
	Focused           bool
	Hovered           bool
	MotionValid       bool
	HoverAmount       float32
	PressAmount       float32
	FocusAmount       float32
	ElapsedMS         float64
	Pressed           bool
	Disabled          bool
	Secure            bool
	ReadOnly          bool
	Row               int32
	Column            int32
	Selected          bool
	Loading           bool
	Pill              bool
	IconOnly          bool
	IconPlacement     int32
	SelectionStartRow int32
	SelectionStartCol int32
	SelectionEndRow   int32
	SelectionEndCol   int32
	IconType          int32
	IconSize          float32
	Semantic          SemanticKind
	Link              string
	Role              string
	AltText           string
	Level             int32
	Columns           int32
	ScrollY           int32
	Wrap              bool
}

type frameOpController interface {
	FrameOps() []FrameOp
}

// Legacy control producers convert their operation once, before it enters the
// frame stream. Button itself records the shared frame without this adapter.
func buttonOperation(op FrameOp) FrameOp {
	op.Button = ButtonFrame{
		Props: ButtonProps{Bounds: op.Bounds, ID: op.ID, Label: op.Text, Disabled: op.Disabled, Loading: op.Loading, IconType: op.IconType,
			IconOnly: op.IconOnly, IconPlacement: IconPlacement(op.IconPlacement)},
		Appearance: StyleFrame{Value: StyleData{IconSize: op.IconSize, Gap: op.Gap,
			OffsetX: op.ContentOffset.X, OffsetY: op.ContentOffset.Y}},
		Material: frameMaterial(op), Font: op.FontSize,
		Foreground: Surface_Opacity(packRGBA(op.TextColor), op.Opacity),
	}
	return op
}

func (r *runtime) recordButton(op FrameOp) {
	r.record(op)
	index := len(r.ops) - 1
	r.ops[index] = buttonOperation(r.ops[index])
}
