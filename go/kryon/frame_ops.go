package kryon

type FrameOpKind string

const (
	FrameOpBackground FrameOpKind = "background"
	FrameOpText       FrameOpKind = "text"
	FrameOpRect       FrameOpKind = "rect"
	FrameOpLine       FrameOpKind = "line"
	FrameOpButton     FrameOpKind = "button"
	FrameOpTextField  FrameOpKind = "text_field"
	FrameOpTextArea   FrameOpKind = "text_area"
	FrameOpColumn     FrameOpKind = "column"
	FrameOpRow        FrameOpKind = "row"
	FrameOpStack      FrameOpKind = "stack"
	FrameOpEnd        FrameOpKind = "end"
)

type FrameOp struct {
	Kind           FrameOpKind
	Bounds         Rectangle
	Text           string
	Color          Color
	SecondaryColor Color
	FontSize       int32
	ID             int32
	FocusID        int32
	Cursor         int32
	Focused        bool
	Pressed        bool
	Disabled       bool
	Secure         bool
}

type frameOpController interface {
	FrameOps() []FrameOp
}
