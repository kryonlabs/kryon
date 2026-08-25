package kryon

type FrameOpKind string

const (
	FrameOpBackground FrameOpKind = "background"
	FrameOpText       FrameOpKind = "text"
	FrameOpRect       FrameOpKind = "rect"
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
	FrameOpEnd        FrameOpKind = "end"
)

type FrameOp struct {
	Kind              FrameOpKind
	Bounds            Rectangle
	Text              string
	Color             Color
	SecondaryColor    Color
	BorderColor       Color
	TextColor         Color
	SelectionColor    Color
	SelectedTextColor Color
	CursorColor       Color
	FontSize          int32
	FontID            uint32
	ID                int32
	FocusID           int32
	Cursor            int32
	SelectionStart    int32
	SelectionEnd      int32
	Focused           bool
	Pressed           bool
	Disabled          bool
	Secure            bool
	Row               int32
	Column            int32
	Selected          bool
	SelectionStartRow int32
	SelectionStartCol int32
	SelectionEndRow   int32
	SelectionEndCol   int32
	IconType          int32
	IconSize          int32
}

type frameOpController interface {
	FrameOps() []FrameOp
}
