package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#include <stdlib.h>
#include <kryon.h>
#include <ui_picture.h>
*/
import "C"

import (
	"reflect"
	"strings"
	"unsafe"
)

// This file carries the widget surface that k2g-generated Go code drives
// through the Runtime interface: the C-named constants widget code references
// verbatim, the Props/Spec types compound literals lower into, and the cgo
// wrappers behind the Runtime methods.

// Raylib color palette under its C names; .kry widget calls reference these
// identifiers verbatim and tx keeps them unchanged.
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
)

// UIPictureFit enum members under their C names.
const (
	UI_PICTURE_FIT_STRETCH = PictureFitStretch
	UI_PICTURE_FIT_CONTAIN = PictureFitContain
	UI_PICTURE_FIT_COVER   = PictureFitCover
)

func boolToInt(b bool) C.int {
	if b {
		return 1
	}
	return 0
}

// cStringList splits a ';'-joined label string the way DropdownLabels does;
// .kry cannot express string arrays, so joined strings are the convention.
func cStringList(joined string) []*C.char {
	parts := strings.Split(joined, ";")
	out := make([]*C.char, len(parts))
	for i, p := range parts {
		out[i] = C.CString(p)
	}
	return out
}

func freeCStringList(list []*C.char) {
	for _, c := range list {
		C.free(unsafe.Pointer(c))
	}
}

// labelsOf accepts every way generated code carries a string list: a
// ';'-joined string, a []string, or a fixed-size [N]string array (the
// lowering of a .kry 'name: [N] const char*' declaration).
func labelsOf(v any) []string {
	switch l := v.(type) {
	case string:
		return DropdownLabels(l)
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

// ---------------------------------------------------------------------------
// Display widgets
// ---------------------------------------------------------------------------

func TextInRect(text string, rect Rectangle, fontSize int32, color Color) {
	ctext := C.CString(text)
	defer C.free(unsafe.Pointer(ctext))
	C.TextInRect(ctext, rect.toC(), C.int(fontSize), color.toC())
}

// UIParagraphSpec mirrors the C spec for Paragraph; the Texture2D icon field
// is derived from IconType (UI_ICON_TYPE_NONE = 0 loads no texture), since
// .kry code has no way to hold a Texture2D value.
type UIParagraphSpec struct {
	Text     string
	IconType int32
	IconSize int32
	Width    int32
	Font     int32
	LineGap  int32
	Color    Color
}

func Paragraph(spec UIParagraphSpec, x int32, y *int32) {
	ctext := C.CString(spec.Text)
	defer C.free(unsafe.Pointer(ctext))
	var icon C.Texture
	if spec.IconType > 0 {
		icon = C.LoadUIIconTexture(C.UIIconType(spec.IconType))
	}
	C.Paragraph(C.UIParagraphSpec{
		text:      ctext,
		icon:      icon,
		icon_type: C.UIIconType(spec.IconType),
		icon_size: C.int(spec.IconSize),
		width:     C.int(spec.Width),
		font:      C.int(spec.Font),
		line_gap:  C.int(spec.LineGap),
		color:     spec.Color.toC(),
	}, C.int(x), (*C.int)(unsafe.Pointer(y)))
}

// TextLines mirrors the C call with lines joined by ';' (see cStringList).
func TextLines(lines string, count int32, x int32, y *int32, font, lineH int32, color Color) {
	list := cStringList(lines)
	defer freeCStringList(list)
	n := len(list)
	if count > 0 && int(count) < n {
		n = int(count)
	}
	C.TextLines(&list[0], C.int(n), C.int(x), (*C.int)(unsafe.Pointer(y)),
		C.int(font), C.int(lineH), color.toC())
}

func Bevel(x, y, w, h int32, light, dark Color) {
	C.Bevel(C.int(x), C.int(y), C.int(w), C.int(h), light.toC(), dark.toC())
}

// IconTexture loads the embedded icon identified by iconType (a UIIconType
// value) and draws it; .kry passes the icon type, not a raw texture.
func IconTexture(id, x, y, size int32, iconType int32, tint Color) {
	var icon C.Texture
	if iconType > 0 {
		icon = C.LoadUIIconTexture(C.UIIconType(iconType))
	}
	C.IconTexture(C.int(id), C.int(x), C.int(y), C.int(size), icon, tint.toC())
}

// PictureProps mirrors the C struct; the cgo call fills the defaults C would
// zero for fields the caller left out.
type PictureProps struct {
	AssetPath string
	Bounds    Rectangle
	Source    Rectangle
	Origin    Vector2
	Rotation  float32
	Tint      Color
	Fit       PictureFit
}

func Picture(props PictureProps) {
	cpath := C.CString(props.AssetPath)
	defer C.free(unsafe.Pointer(cpath))
	C.Picture(C.PictureProps{
		asset_path: cpath,
		bounds:     props.Bounds.toC(),
		source:     props.Source.toC(),
		origin:     C.Vector2{x: C.float(props.Origin.X), y: C.float(props.Origin.Y)},
		rotation:   C.float(props.Rotation),
		tint:       props.Tint.toC(),
		fit:        C.UIPictureFit(props.Fit),
	})
}

// Rect is the C-shaped fill+border rectangle; the Runtime method keeps the
// border optional for the fill-only form.
func Rect(x, y, w, h int32, fill, border Color) {
	C.Rect(C.int(x), C.int(y), C.int(w), C.int(h), fill.toC(), border.toC())
}

// ---------------------------------------------------------------------------
// Input widgets
// ---------------------------------------------------------------------------

func IconButton(props IconButtonProps) bool {
	return DrawUIIconButton(props)
}

func Href(props HrefProps) bool {
	return DrawUIHref(props)
}

func Toggle(id, x, y, w, h int32, value *int32, offLabel, onLabel string) bool {
	coff := C.CString(offLabel)
	con := C.CString(onLabel)
	defer C.free(unsafe.Pointer(coff))
	defer C.free(unsafe.Pointer(con))
	return C.Toggle(C.int(id), C.int(x), C.int(y), C.int(w), C.int(h),
		(*C.int)(unsafe.Pointer(value)), coff, con) != 0
}

// ---------------------------------------------------------------------------
// Overlays and navigation
// ---------------------------------------------------------------------------

func Modal(title, message, cancelBtn, confirmBtn string) int {
	ctitle := C.CString(title)
	cmessage := C.CString(message)
	ccancel := C.CString(cancelBtn)
	cconfirm := C.CString(confirmBtn)
	defer C.free(unsafe.Pointer(ctitle))
	defer C.free(unsafe.Pointer(cmessage))
	defer C.free(unsafe.Pointer(ccancel))
	defer C.free(unsafe.Pointer(cconfirm))
	return int(C.Modal(ctitle, cmessage, ccancel, cconfirm))
}

func TitleBar(title string, height int32) {
	ctitle := C.CString(title)
	defer C.free(unsafe.Pointer(ctitle))
	C.TitleBar(ctitle, C.int(height))
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
	Count          int32 // accepted for C parity; Items length wins
	Items          []BottomNavItem
	Height         int32
	IconSize       int32
	IconPadding    int32
	SideMargin     int32
	BottomMargin   int32
	MaxButtonWidth int32
}

type UIBottomNavResult struct {
	ClickedIndex int32
	ClickedRoute int32
	Y            int32
	Height       int32
}

func BottomNav(props BottomNavProps) UIBottomNavResult {
	n := len(props.Items)
	var items *C.UIBottomNavItem
	if n > 0 {
		clabels := make([]*C.char, n)
		for i, it := range props.Items {
			clabels[i] = C.CString(it.Label)
		}
		defer freeCStringList(clabels)
		citems := make([]C.UIBottomNavItem, n)
		for i, it := range props.Items {
			citems[i] = C.UIBottomNavItem{
				route:    C.int(it.Route),
				label:    clabels[i],
				icon: C.Texture{
					id:      C.uint(it.Icon.ID),
					width:   C.int(it.Icon.Width),
					height:  C.int(it.Icon.Height),
					mipmaps: C.int(it.Icon.Mipmaps),
					format:  C.int(it.Icon.Format),
				},
				active:   boolToInt(it.Active),
				disabled: boolToInt(it.Disabled),
			}
		}
		items = &citems[0]
	}
	res := C.BottomNav(C.BottomNavProps{
		view_width:      C.int(props.ViewWidth),
		view_height:     C.int(props.ViewHeight),
		count:           C.int(n),
		items:           items,
		height:          C.int(props.Height),
		icon_size:       C.int(props.IconSize),
		icon_padding:    C.int(props.IconPadding),
		side_margin:     C.int(props.SideMargin),
		bottom_margin:   C.int(props.BottomMargin),
		max_button_width: C.int(props.MaxButtonWidth),
	})
	return UIBottomNavResult{
		ClickedIndex: int32(res.clicked_index),
		ClickedRoute: int32(res.clicked_route),
		Y:            int32(res.y),
		Height:       int32(res.height),
	}
}

type TopNavProps struct {
	ID                int32
	X, Y              int32
	Width, Height     int32
	Title             string
	Options           string // ';'-joined (see cStringList)
	OptionCount       int32  // accepted for C parity; Options split wins
	SelectedIndex     *int32
	Disabled          bool
	DropdownMinWidth  int32
	DropdownHeight    int32
	ActionIconSize    int32
	ActionIconPadding int32
	ActionGap         int32
	SidePadding       int32
}

type UITopNavResult struct {
	SelectedMenuItem int32
	ClickedAction    int32
}

func TopNav(props TopNavProps) UITopNavResult {
	ctitle := C.CString(props.Title)
	defer C.free(unsafe.Pointer(ctitle))
	options := cStringList(props.Options)
	defer freeCStringList(options)
	var selected *C.int
	if props.SelectedIndex != nil {
		selected = (*C.int)(unsafe.Pointer(props.SelectedIndex))
	}
	res := C.TopNav(C.TopNavProps{
		id:                 C.int(props.ID),
		x:                  C.int(props.X),
		y:                  C.int(props.Y),
		width:              C.int(props.Width),
		height:             C.int(props.Height),
		title:              ctitle,
		options:            &options[0],
		option_count:       C.int(len(options)),
		selected_index:     selected,
		disabled:           boolToInt(props.Disabled),
		dropdown_min_width: C.int(props.DropdownMinWidth),
		dropdown_height:    C.int(props.DropdownHeight),
		action_icon_size:   C.int(props.ActionIconSize),
		action_icon_padding: C.int(props.ActionIconPadding),
		action_gap:         C.int(props.ActionGap),
		side_padding:       C.int(props.SidePadding),
	})
	return UITopNavResult{
		SelectedMenuItem: int32(res.selected_menu_item),
		ClickedAction:    int32(res.clicked_action),
	}
}

type ToolbarProps struct {
	ID                 int32
	X, Y               int32
	Width, Height      int32
	DrawMenu           bool
	Options            string // ';'-joined (see cStringList)
	OptionCount        int32  // accepted for C parity; Options split wins
	SelectedIndex      *int32
	DropdownMinWidth   int32
	DropdownMaxWidth   int32
	DropdownHeight     int32
	ActionIconSize     int32
	ActionIconPadding  int32
	ActionGap          int32
	SidePadding        int32
}

type UIToolbarResult struct {
	SelectedMenuItem int32
	ClickedAction    int32
}

func Toolbar(props ToolbarProps) UIToolbarResult {
	options := cStringList(props.Options)
	defer freeCStringList(options)
	var selected *C.int
	if props.SelectedIndex != nil {
		selected = (*C.int)(unsafe.Pointer(props.SelectedIndex))
	}
	res := C.Toolbar(C.ToolbarProps{
		id:                  C.int(props.ID),
		x:                   C.int(props.X),
		y:                   C.int(props.Y),
		width:               C.int(props.Width),
		height:              C.int(props.Height),
		draw_menu:           boolToInt(props.DrawMenu),
		options:             &options[0],
		option_count:        C.int(len(options)),
		selected_index:      selected,
		dropdown_min_width:  C.int(props.DropdownMinWidth),
		dropdown_max_width:  C.int(props.DropdownMaxWidth),
		dropdown_height:     C.int(props.DropdownHeight),
		actions:             nil,
		action_count:        0,
		action_icon_size:    C.int(props.ActionIconSize),
		action_icon_padding: C.int(props.ActionIconPadding),
		action_gap:          C.int(props.ActionGap),
		side_padding:        C.int(props.SidePadding),
	})
	return UIToolbarResult{
		SelectedMenuItem: int32(res.selected_menu_item),
		ClickedAction:    int32(res.clicked_action),
	}
}

// ---------------------------------------------------------------------------
// Runtime methods for the widget surface above
// ---------------------------------------------------------------------------

func (r *runtime) Key(text string) UIKey             { return Key(text) }
func (r *runtime) Fade(c Color, alpha float32) Color { return Fade(c, alpha) }

func (r *runtime) GetThemeSurface() Color     { return GetThemeSurface() }
func (r *runtime) GetThemeButton() Color      { return GetThemeButton() }
func (r *runtime) GetThemeButtonHover() Color { return GetThemeButtonHover() }
func (r *runtime) GetThemeLink() Color        { return GetThemeLink() }

func (r *runtime) TextInRect(text string, rect Rectangle, fontSize int32, color Color) {
	TextInRect(text, rect, fontSize, color)
}

func (r *runtime) TextLines(lines any, count int32, x int32, y *int32, font, lineH int32, color Color) {
	s, ok := lines.(string)
	if !ok {
		s = strings.Join(labelsOf(lines), ";")
	}
	TextLines(s, count, x, y, font, lineH, color)
}

func (r *runtime) Bevel(x, y, w, h int32, light, dark Color) {
	Bevel(x, y, w, h, light, dark)
}

func (r *runtime) IconTexture(id, x, y, size int32, iconType int32, tint Color) {
	IconTexture(id, x, y, size, iconType, tint)
}

func (r *runtime) Picture(props PictureProps) { Picture(props) }

func (r *runtime) Paragraph(spec UIParagraphSpec, x int32, y *int32) {
	Paragraph(spec, x, y)
}

func (r *runtime) IconButton(props IconButtonProps) bool { return IconButton(props) }
func (r *runtime) Href(props HrefProps) bool             { return Href(props) }

func (r *runtime) Slider(id, x, y, w int32, label string, min, max int32, value *int32, rest ...any) bool {
	suffix, override := "", ""
	if len(rest) > 0 {
		if s, ok := rest[0].(string); ok {
			suffix = s
		}
	}
	if len(rest) > 1 {
		if s, ok := rest[1].(string); ok {
			override = s
		}
	}
	return DrawUISlider(id, x, y, w, label, min, max, value, suffix, override)
}

func (r *runtime) Toggle(id, x, y, w, h int32, value *int32, offLabel, onLabel string) bool {
	return Toggle(id, x, y, w, h, value, offLabel, onLabel)
}

func (r *runtime) Row(props ColumnProps)   { Row(RowProps(props)) }
func (r *runtime) Stack(props ColumnProps) { Stack(props) }

func (r *runtime) Modal(title, message, cancelBtn, confirmBtn string) int {
	return Modal(title, message, cancelBtn, confirmBtn)
}

func (r *runtime) TitleBar(title string, height int32) { TitleBar(title, height) }

func (r *runtime) BottomNav(props BottomNavProps) { BottomNav(props) }
func (r *runtime) TopNav(props TopNavProps)       { TopNav(props) }
func (r *runtime) Toolbar(props ToolbarProps)     { Toolbar(props) }

// ---------------------------------------------------------------------------
// Bare widget names. The DrawUI*-prefixed immediate-mode spellings mirror
// the C API; these aliases give every widget its plain name so generated
// code and new hand-written code never need the prefix. Existing callers
// keep compiling — the prefixed names remain as the definitions.
// ---------------------------------------------------------------------------

var (
	TextInputControl = DrawUITextInputControl
	GenericButton    = DrawUIGenericButton
	TextButton       = DrawUITextButton
	ReadonlyTextBox  = DrawUIReadonlyTextBox
	Dropdown         = DrawUIDropdown
	DropdownEx       = DrawUIDropdownEx
	LocaleDropdown   = DrawUILocaleDropdown
	VerticalSlider   = DrawUIVerticalSlider
	Checkbox         = DrawUICheckboxToggle
	ToggleSwitch     = DrawUIToggleSwitch
	Hyperlink        = DrawUIHref
	IconBtn          = DrawUIIconButton
)
