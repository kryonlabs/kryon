// Complete bindings for Kryon UI toolkit: layout helpers, menus, complex controls
package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo linux,amd64 LDFLAGS: ${SRCDIR}/../../build/linux-x86_64/libkryon.a ${SRCDIR}/../../build/linux-x86_64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl
#cgo linux,arm64 LDFLAGS: ${SRCDIR}/../../build/linux-aarch64/libkryon.a ${SRCDIR}/../../build/linux-aarch64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl

#include <stdlib.h>
#include <string.h>
#include <kryon.h>
#include <ui_tk.h>
*/
import "C"

import "unsafe"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

type UISide int32

const (
	UISideTop    UISide = C.UI_SIDE_TOP
	UISideBottom UISide = C.UI_SIDE_BOTTOM
	UISideLeft   UISide = C.UI_SIDE_LEFT
	UISideRight  UISide = C.UI_SIDE_RIGHT
)

type UIMenuItemKind int32

const (
	UIMenuCommand   UIMenuItemKind = C.UI_MENU_COMMAND
	UIMenuCheck     UIMenuItemKind = C.UI_MENU_CHECK
	UIMenuRadio     UIMenuItemKind = C.UI_MENU_RADIO
	UIMenuSeparator UIMenuItemKind = C.UI_MENU_SEPARATOR
	UIMenuSubmenu   UIMenuItemKind = C.UI_MENU_SUBMENU
)

// ---------------------------------------------------------------------------
// Layout Types
// ---------------------------------------------------------------------------

type UIFrame struct {
	Bounds  Rectangle
	PadX    int32
	PadY    int32
	Gap     int32
	CursorX int32
	CursorY int32
}

func (f UIFrame) toC() C.UIFrame {
	return C.UIFrame{
		bounds:   f.Bounds.toC(),
		pad_x:    C.int(f.PadX),
		pad_y:    C.int(f.PadY),
		gap:      C.int(f.Gap),
		cursor_x: C.int(f.CursorX),
		cursor_y: C.int(f.CursorY),
	}
}

func frameFromC(cf C.UIFrame) UIFrame {
	return UIFrame{
		Bounds:  rectFromC(cf.bounds),
		PadX:    int32(cf.pad_x),
		PadY:    int32(cf.pad_y),
		Gap:     int32(cf.gap),
		CursorX: int32(cf.cursor_x),
		CursorY: int32(cf.cursor_y),
	}
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

func (g UIGrid) toC() C.UIGrid {
	return C.UIGrid{
		bounds: g.Bounds.toC(),
		rows:   C.int(g.Rows),
		cols:   C.int(g.Cols),
		gap_x:  C.int(g.GapX),
		gap_y:  C.int(g.GapY),
		pad_x:  C.int(g.PadX),
		pad_y:  C.int(g.PadY),
	}
}

// ---------------------------------------------------------------------------
// Menu Types
// ---------------------------------------------------------------------------

type UIMenuItem struct {
	Kind        UIMenuItemKind
	Label       string
	Accelerator string
	ID          int32
	Disabled    bool
	Checked     bool
	Submenu     []UIMenuItem
}

type UIMenu struct {
	Bounds Rectangle
	Label  string
	Items  []UIMenuItem
}

type UIMenuBarResult struct {
	ActivatedID int32
	OpenIndex   int32
}

type UIContextMenu struct {
	ID      int32
	Trigger Rectangle
	Items   []UIMenuItem
	Open    *bool
	X       *int32
	Y       *int32
}

// ---------------------------------------------------------------------------
// Control Property Types
// ---------------------------------------------------------------------------

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

type ImageBoxProps struct {
	Bounds  Rectangle
	Texture Texture2D
	Tint    Color
}

type ListBoxProps struct {
	Bounds        Rectangle
	ID            int32
	Items         []string
	SelectedIndex *int32
	ScrollOffset  *int32
	RowHeight     int32
}

type UITreeItem struct {
	Label      string
	Depth      int32
	ID         int32
	Expanded   bool
	Selectable bool
}

type TreeViewProps struct {
	Bounds       Rectangle
	ID           int32
	Items        []UITreeItem
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

// ---------------------------------------------------------------------------
// Canvas Types
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Accelerator Types
// ---------------------------------------------------------------------------

type UIAccelerator struct {
	Key   int32
	Ctrl  bool
	Shift bool
	Alt   bool
	ID    int32
}

// ---------------------------------------------------------------------------
// Layout Functions
// ---------------------------------------------------------------------------

func BeginUIFrameBox(bounds Rectangle, padX, padY, gap int32) UIFrame {
	cf := C.BeginUIFrameBox(bounds.toC(), C.int(padX), C.int(padY), C.int(gap))
	return frameFromC(cf)
}

func UIFramePack(frame *UIFrame, side UISide, size int32) Rectangle {
	cf := frame.toC()
	cr := C.UIFramePack(&cf, C.UISide(side), C.int(size))
	*frame = frameFromC(cf)
	return rectFromC(cr)
}

func UIGridCell(grid UIGrid, row, col, rowSpan, colSpan int32) Rectangle {
	cr := C.UIGridCell(grid.toC(), C.int(row), C.int(col), C.int(rowSpan), C.int(colSpan))
	return rectFromC(cr)
}

func UIPlace(parent Rectangle, x, y, w, h int32) Rectangle {
	cr := C.UIPlace(parent.toC(), C.int(x), C.int(y), C.int(w), C.int(h))
	return rectFromC(cr)
}

// ---------------------------------------------------------------------------
// Canvas Functions
// ---------------------------------------------------------------------------

func BeginUICanvas(canvas UICanvas) UICanvasResult {
	cc := C.UICanvas{
		bounds:   canvas.Bounds.toC(),
		scroll_x: (*C.int)(unsafe.Pointer(canvas.ScrollX)),
		scroll_y: (*C.int)(unsafe.Pointer(canvas.ScrollY)),
		zoom:     (*C.float)(unsafe.Pointer(canvas.Zoom)),
	}
	cr := C.BeginUICanvas(cc)
	return UICanvasResult{
		Active:        cr.active != 0,
		Dragging:      cr.dragging != 0,
		SelectedIndex: int32(cr.selected_index),
		World:         vecFromC(cr.world),
	}
}

func EndUICanvas(canvas UICanvas) {
	cc := C.UICanvas{
		bounds:   canvas.Bounds.toC(),
		scroll_x: (*C.int)(unsafe.Pointer(canvas.ScrollX)),
		scroll_y: (*C.int)(unsafe.Pointer(canvas.ScrollY)),
		zoom:     (*C.float)(unsafe.Pointer(canvas.Zoom)),
	}
	C.EndUICanvas(cc)
}

func UICanvasHitTest(point Vector2, items []Rectangle) int32 {
	if len(items) == 0 {
		return -1
	}
	citems := make([]C.Rectangle, len(items))
	for i, r := range items {
		citems[i] = r.toC()
	}
	return int32(C.UICanvasHitTest(point.toC(), &citems[0], C.int(len(items))))
}

func UICanvasToScreen(canvas UICanvas, point Vector2) Vector2 {
	cc := C.UICanvas{
		bounds:   canvas.Bounds.toC(),
		scroll_x: (*C.int)(unsafe.Pointer(canvas.ScrollX)),
		scroll_y: (*C.int)(unsafe.Pointer(canvas.ScrollY)),
		zoom:     (*C.float)(unsafe.Pointer(canvas.Zoom)),
	}
	return vecFromC(C.UICanvasToScreen(cc, point.toC()))
}

func UICanvasRectToScreen(canvas UICanvas, rect Rectangle) Rectangle {
	cc := C.UICanvas{
		bounds:   canvas.Bounds.toC(),
		scroll_x: (*C.int)(unsafe.Pointer(canvas.ScrollX)),
		scroll_y: (*C.int)(unsafe.Pointer(canvas.ScrollY)),
		zoom:     (*C.float)(unsafe.Pointer(canvas.Zoom)),
	}
	return rectFromC(C.UICanvasRectToScreen(cc, rect.toC()))
}

// ---------------------------------------------------------------------------
// Accelerator Functions
// ---------------------------------------------------------------------------

func UIAcceleratorPressed(accel UIAccelerator) bool {
	ctrl := 0
	if accel.Ctrl {
		ctrl = 1
	}
	shift := 0
	if accel.Shift {
		shift = 1
	}
	alt := 0
	if accel.Alt {
		alt = 1
	}

	caccel := C.UIAccelerator{
		key:   C.int(accel.Key),
		ctrl:  C.int(ctrl),
		shift: C.int(shift),
		alt:   C.int(alt),
		id:    C.int(accel.ID),
	}
	return C.UIAcceleratorPressed(caccel) != 0
}

func DispatchUIAccelerators(accelerators []UIAccelerator) int32 {
	if len(accelerators) == 0 {
		return 0
	}

	caccels := make([]C.UIAccelerator, len(accelerators))
	for i, a := range accelerators {
		ctrl := 0
		if a.Ctrl {
			ctrl = 1
		}
		shift := 0
		if a.Shift {
			shift = 1
		}
		alt := 0
		if a.Alt {
			alt = 1
		}
		caccels[i] = C.UIAccelerator{
			key:   C.int(a.Key),
			ctrl:  C.int(ctrl),
			shift: C.int(shift),
			alt:   C.int(alt),
			id:    C.int(a.ID),
		}
	}

	return int32(C.DispatchUIAccelerators(&caccels[0], C.int(len(accelerators))))
}

func LabelFrame(props LabelFrameProps) {
	title := C.CString(props.Title)
	defer C.free(unsafe.Pointer(title))
	C.LabelFrame(C.LabelFrameProps{bounds: props.Bounds.toC(), title: title})
}

func ListBox(props ListBoxProps) int32 {
	if len(props.Items) == 0 {
		return 0
	}
	items, free := cStringArray(props.Items)
	defer free()
	return int32(C.ListBox(C.ListBoxProps{
		bounds: props.Bounds.toC(), id: C.int(props.ID),
		items: (**C.char)(unsafe.Pointer(&items[0])), item_count: C.int(len(items)),
		selected_index: (*C.int)(unsafe.Pointer(props.SelectedIndex)),
		scroll_offset:  (*C.int)(unsafe.Pointer(props.ScrollOffset)), row_height: C.int(props.RowHeight),
	}))
}

func SourceView(props SourceViewProps) int32 {
	value := C.CString(props.Text)
	defer C.free(unsafe.Pointer(value))
	lines := C.int(0)
	if props.ShowLineNumbers {
		lines = 1
	}
	return int32(C.SourceView(C.SourceViewProps{
		bounds: props.Bounds.toC(), text: value,
		scroll_x: (*C.int)(unsafe.Pointer(props.ScrollX)), scroll_y: (*C.int)(unsafe.Pointer(props.ScrollY)),
		font_size: C.int(props.FontSize), line_height: C.int(props.LineHeight), show_line_numbers: lines,
	}))
}

func TableView(props TableViewProps) int32 {
	if len(props.Columns) == 0 {
		return 0
	}
	columns, freeColumns := cStringArray(props.Columns)
	defer freeColumns()
	crows := make([]C.UITableRow, len(props.Rows))
	rowStrings := make([][]*C.char, len(props.Rows))
	for i, row := range props.Rows {
		var free func()
		rowStrings[i], free = cStringArray(row.Cells)
		defer free()
		if len(rowStrings[i]) != 0 {
			crows[i].cells = (**C.char)(unsafe.Pointer(&rowStrings[i][0]))
		}
		crows[i].cell_count = C.int(len(rowStrings[i]))
	}
	widths := make([]C.int, len(props.ColumnWidths))
	for i, width := range props.ColumnWidths {
		widths[i] = C.int(width)
	}
	var rows *C.UITableRow
	if len(crows) != 0 {
		rows = &crows[0]
	}
	var columnWidths *C.int
	if len(widths) != 0 {
		columnWidths = &widths[0]
	}
	return int32(C.TableView(C.TableViewProps{
		bounds: props.Bounds.toC(), id: C.int(props.ID),
		columns: (**C.char)(unsafe.Pointer(&columns[0])), column_count: C.int(len(columns)),
		rows: rows, row_count: C.int(len(crows)), column_widths: columnWidths,
		selected_row: (*C.int)(unsafe.Pointer(props.SelectedRow)), sort_column: (*C.int)(unsafe.Pointer(props.SortColumn)),
		scroll_offset: (*C.int)(unsafe.Pointer(props.ScrollOffset)), row_height: C.int(props.RowHeight),
	}))
}

func Notebook(props NotebookProps) int32 {
	if len(props.Tabs) == 0 {
		return 0
	}
	tabs, free := cStringArray(props.Tabs)
	defer free()
	return int32(C.Notebook(C.NotebookProps{
		bounds: props.Bounds.toC(), tabs: (**C.char)(unsafe.Pointer(&tabs[0])),
		tab_count: C.int(len(tabs)), selected_index: (*C.int)(unsafe.Pointer(props.SelectedIndex)),
	}))
}

func PanedView(props PanedViewProps) int32 {
	vertical := C.int(0)
	if props.Vertical {
		vertical = 1
	}
	return int32(C.PanedView(C.PanedViewProps{
		bounds: props.Bounds.toC(), id: C.int(props.ID), vertical: vertical,
		split: (*C.int)(unsafe.Pointer(props.Split)), min_first: C.int(props.MinFirst), min_second: C.int(props.MinSecond),
	}))
}

func Collapsible(props CollapsibleProps) int32 {
	label := C.CString(props.Label)
	defer C.free(unsafe.Pointer(label))
	open := C.int(0)
	if props.Open != nil && *props.Open {
		open = 1
	}
	result := int32(C.Collapsible(C.CollapsibleProps{bounds: props.Bounds.toC(), label: label, open: &open}))
	if props.Open != nil {
		*props.Open = open != 0
	}
	return result
}

func CanvasGrid(bounds Rectangle, step int32, color Color) {
	C.CanvasGrid(bounds.toC(), C.int(step), color.toC())
}

func MessageDialog(props MessageDialogProps) int32 {
	title, message, ok := C.CString(props.Title), C.CString(props.Message), C.CString(props.OKLabel)
	defer C.free(unsafe.Pointer(title))
	defer C.free(unsafe.Pointer(message))
	defer C.free(unsafe.Pointer(ok))
	return int32(C.MessageDialog(C.MessageDialogProps{title: title, message: message, ok_label: ok}))
}

func ConfirmDialog(props ConfirmDialogProps) int32 {
	title, message := C.CString(props.Title), C.CString(props.Message)
	cancel, confirm := C.CString(props.CancelLabel), C.CString(props.ConfirmLabel)
	defer C.free(unsafe.Pointer(title))
	defer C.free(unsafe.Pointer(message))
	defer C.free(unsafe.Pointer(cancel))
	defer C.free(unsafe.Pointer(confirm))
	return int32(C.ConfirmDialog(C.ConfirmDialogProps{title: title, message: message, cancel_label: cancel, confirm_label: confirm}))
}

func PromptDialog(props PromptDialogProps) int32 {
	if len(props.Text) == 0 {
		return 0
	}
	title, cancel, confirm := C.CString(props.Title), C.CString(props.CancelLabel), C.CString(props.ConfirmLabel)
	defer C.free(unsafe.Pointer(title))
	defer C.free(unsafe.Pointer(cancel))
	defer C.free(unsafe.Pointer(confirm))
	focused := C.int(0)
	if props.Focused != nil && *props.Focused {
		focused = 1
	}
	result := int32(C.PromptDialog(C.PromptDialogProps{
		title: title, text: (*C.char)(unsafe.Pointer(&props.Text[0])), text_size: C.int(len(props.Text)),
		cursor_position: (*C.int)(unsafe.Pointer(props.Cursor)), focused: &focused,
		cancel_label: cancel, confirm_label: confirm,
	}))
	if props.Focused != nil {
		*props.Focused = focused != 0
	}
	return result
}

func cStringArray(values []string) ([]*C.char, func()) {
	result := make([]*C.char, len(values))
	for i, value := range values {
		result[i] = C.CString(value)
	}
	return result, func() {
		for _, value := range result {
			C.free(unsafe.Pointer(value))
		}
	}
}
