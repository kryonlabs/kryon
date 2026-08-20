package kryui

// This file completes the Runtime surface so k2g-generated code reaches
// everything the hand-written Go API exposes: positional controls, Props
// widgets, dialogs, the canvas, the Tk layout helpers, toasts, and theme
// control. Everything forwards to existing package functions; no cgo here.

// ThemeStyle constants under their C names for generated code.
const (
	THEME_STYLE_SYSTEM   = ThemeStyleSystem
	THEME_STYLE_RETRO    = ThemeStyleRetro
	THEME_STYLE_MATERIAL = ThemeStyleMaterial
)

func boolPtrFromInt(p *int32) *bool {
	if p == nil {
		return nil
	}
	v := *p != 0
	return &v
}

func intPtrFromBool(p *bool) *int32 {
	if p == nil {
		return nil
	}
	v := int32(0)
	if *p {
		v = 1
	}
	return &v
}

// ---------------------------------------------------------------------------
// Runtime methods: positional controls
// ---------------------------------------------------------------------------

func (r *runtime) GenericButton(id, x, y, w, h int32, label string,
	style UIButtonStyle, disabled int32, hover *int32) bool {
	_ = id
	var hb bool
	res := GenericButton(x, y, w, h, label, style, disabled != 0, &hb)
	if hover != nil {
		if hb {
			*hover = 1
		} else {
			*hover = 0
		}
	}
	return res
}

func (r *runtime) TextButton(id, x, y int32, label string, hover *int32) bool {
	_ = id
	var hb bool
	res := TextButton(x, y, label, &hb)
	if hover != nil {
		if hb {
			*hover = 1
		} else {
			*hover = 0
		}
	}
	return res
}

func (r *runtime) LocaleDropdown(id, x, y, w, h int32, selected *int32) bool {
	return LocaleDropdown(id, x, y, w, h, selected)
}

func (r *runtime) VerticalSlider(id, x, y, h, min, max int32,
	value *int32) bool {
	return VerticalSlider(id, x, y, h, min, max, value)
}

func (r *runtime) CanvasGrid(bounds Rectangle, step int32, color Color) {
	CanvasGrid(bounds, step, color)
}

func (r *runtime) SelectableText(value string, x, y, fontSize int32,
	color Color) {
	SelectableText(value, x, y, fontSize, color)
}

func (r *runtime) ShowUIToast(message string) { ShowToast(message) }

func (r *runtime) ShowUIToastFor(message string, seconds float64) {
	ShowToastFor(message, seconds)
}

// ---------------------------------------------------------------------------
// Runtime methods: Props widgets (types live with their package functions)
// ---------------------------------------------------------------------------

func (r *runtime) TextInputControl(props TextInputProps) bool {
	return TextInputControl(props)
}

func (r *runtime) ReadonlyTextBox(props ReadonlyTextBoxProps) {
	ReadonlyTextBox(props)
}

func (r *runtime) TextArea(props TextAreaProps) bool {
	return DrawUITextArea(props)
}

func (r *runtime) Radio(props RadioButtonProps) int32 { return Radio(props) }

func (r *runtime) Spinbox(props SpinboxProps) bool { return Spinbox(props) }

func (r *runtime) Combobox(props ComboboxProps) bool { return Combobox(props) }

func (r *runtime) LabelFrame(props LabelFrameProps) { LabelFrame(props) }

func (r *runtime) Notebook(props NotebookProps) int32 { return Notebook(props) }

func (r *runtime) PanedView(props PanedViewProps) int32 {
	return PanedView(props)
}

func (r *runtime) Collapsible(props CollapsibleProps) int32 {
	return Collapsible(props)
}

func (r *runtime) ListBox(props ListBoxProps) int32 { return ListBox(props) }

func (r *runtime) SourceView(props SourceViewProps) int32 {
	return SourceView(props)
}

func (r *runtime) TableView(props TableViewProps) int32 {
	return TableView(props)
}

func (r *runtime) MessageDialog(props MessageDialogProps) int32 {
	return MessageDialog(props)
}

func (r *runtime) ConfirmDialog(props ConfirmDialogProps) int32 {
	return ConfirmDialog(props)
}

func (r *runtime) PromptDialog(props PromptDialogProps) int32 {
	return PromptDialog(props)
}

// ---------------------------------------------------------------------------
// Runtime methods: canvas and Tk layout helpers
// ---------------------------------------------------------------------------

func (r *runtime) BeginUICanvas(canvas UICanvas) UICanvasResult {
	return BeginUICanvas(canvas)
}

func (r *runtime) EndUICanvas(canvas UICanvas) { EndUICanvas(canvas) }

func (r *runtime) BeginUIFrameBox(bounds Rectangle, padX, padY,
	gap int32) UIFrame {
	return BeginUIFrameBox(bounds, padX, padY, gap)
}

func (r *runtime) UIFramePack(frame *UIFrame, side UISide,
	size int32) Rectangle {
	return UIFramePack(frame, side, size)
}

func (r *runtime) UIGridCell(grid UIGrid, row, col, rowSpan,
	colSpan int32) Rectangle {
	return UIGridCell(grid, row, col, rowSpan, colSpan)
}

func (r *runtime) UIPlace(parent Rectangle, x, y, w, h int32) Rectangle {
	return UIPlace(parent, x, y, w, h)
}

// ---------------------------------------------------------------------------
// Runtime methods: theme control
// ---------------------------------------------------------------------------

func (r *runtime) SetCurrentTheme(themeID int32, darkMode int32) {
	SetCurrentTheme(themeID, darkMode != 0)
}

func (r *runtime) SetThemeDarkMode(dark int32) { SetThemeDarkMode(dark != 0) }

func (r *runtime) SetThemeStyle(style ThemeStyle) { SetThemeStyle(style) }

func (r *runtime) SetThemeSource(source ThemeSource) { SetThemeSource(source) }

func (r *runtime) SetThemeMode(mode ThemeMode) { SetThemeMode(mode) }
