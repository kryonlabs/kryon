// Complete bindings for Kryon UI controls with theme fonts and widgets.
// This exposes all DrawUI* functions from ui_controls.h for full Kryon usage.
package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: ${SRCDIR}/../../build/linux-x86_64/libkryon.a ${SRCDIR}/../../build/linux-x86_64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl

#include <stdlib.h>
#include <string.h>
#include <kryon.h>
#include <ui_controls.h>
*/
import "C"

import (
	goruntime "runtime"
	"unsafe"
)

// ---------------------------------------------------------------------------
// Enums and Constants
// ---------------------------------------------------------------------------

type UIIconSize int32

const (
	UIIconSizeTiny   UIIconSize = C.UI_ICON_SIZE_TINY
	UIIconSizeSmall  UIIconSize = C.UI_ICON_SIZE_SMALL
	UIIconSizeMedium UIIconSize = C.UI_ICON_SIZE_MEDIUM
	UIIconSizeLarge  UIIconSize = C.UI_ICON_SIZE_LARGE
)

type UIButtonStyle int32

const (
	UIButtonStylePrimary     UIButtonStyle = C.UI_BUTTON_STYLE_PRIMARY
	UIButtonStyleSecondary   UIButtonStyle = C.UI_BUTTON_STYLE_SECONDARY
	UIButtonStyleDanger      UIButtonStyle = C.UI_BUTTON_STYLE_DANGER
	UIButtonStyleTab         UIButtonStyle = C.UI_BUTTON_STYLE_TAB
	UIButtonStyleTabSelected UIButtonStyle = C.UI_BUTTON_STYLE_TAB_SELECTED
)

type UISyntaxMode int32

const (
	UISyntaxNone UISyntaxMode = C.UI_SYNTAX_NONE
	UISyntaxKry  UISyntaxMode = C.UI_SYNTAX_KRY
	UISyntaxC    UISyntaxMode = C.UI_SYNTAX_C
	UISyntaxMake UISyntaxMode = C.UI_SYNTAX_MAKE
)

// ---------------------------------------------------------------------------
// Style and Theme Types
// ---------------------------------------------------------------------------

type UITextInputStyle struct {
	Background  Color
	Border      Color
	FocusBorder Color
	Text        Color
	Cursor      Color
	Radius      float32
	PaddingX    int32
	PaddingY    int32
}

func (s UITextInputStyle) toC() C.UITextInputStyle {
	return C.UITextInputStyle{
		background:   s.Background.toC(),
		border:       s.Border.toC(),
		focus_border: s.FocusBorder.toC(),
		text:         s.Text.toC(),
		cursor:       s.Cursor.toC(),
		radius:       C.float(s.Radius),
		padding_x:    C.int(s.PaddingX),
		padding_y:    C.int(s.PaddingY),
	}
}

type UIStyleTokens struct {
	ControlRadius  float32
	PanelRadius    float32
	ControlAlpha   uint8
	PanelAlpha     uint8
	BorderAlpha    uint8
	ShadowAlpha    uint8
	ShineAlpha     uint8
	BevelEnabled   bool
	TouchTargetMin int32
	ShadowOffsetY  int32
}

func (t UIStyleTokens) toC() C.UIStyleTokens {
	bevel := 0
	if t.BevelEnabled {
		bevel = 1
	}
	return C.UIStyleTokens{
		control_radius:   C.float(t.ControlRadius),
		panel_radius:     C.float(t.PanelRadius),
		control_alpha:    C.uchar(t.ControlAlpha),
		panel_alpha:      C.uchar(t.PanelAlpha),
		border_alpha:     C.uchar(t.BorderAlpha),
		shadow_alpha:     C.uchar(t.ShadowAlpha),
		shine_alpha:      C.uchar(t.ShineAlpha),
		bevel_enabled:    C.int(bevel),
		touch_target_min: C.int(t.TouchTargetMin),
		shadow_offset_y:  C.int(t.ShadowOffsetY),
	}
}

func styleTokensFromC(ct C.UIStyleTokens) UIStyleTokens {
	return UIStyleTokens{
		ControlRadius:  float32(ct.control_radius),
		PanelRadius:    float32(ct.panel_radius),
		ControlAlpha:   uint8(ct.control_alpha),
		PanelAlpha:     uint8(ct.panel_alpha),
		BorderAlpha:    uint8(ct.border_alpha),
		ShadowAlpha:    uint8(ct.shadow_alpha),
		ShineAlpha:     uint8(ct.shine_alpha),
		BevelEnabled:   ct.bevel_enabled != 0,
		TouchTargetMin: int32(ct.touch_target_min),
		ShadowOffsetY:  int32(ct.shadow_offset_y),
	}
}

type UIMaterialScheme struct {
	Primary           Color
	OnPrimary         Color
	Secondary         Color
	OnSecondary       Color
	Surface           Color
	OnSurface         Color
	SurfaceContainer  Color
	SurfaceVariant    Color
	OnSurfaceVariant  Color
	Outline           Color
	Error             Color
	OnError           Color
	DisabledContainer Color
	DisabledContent   Color
}

// ---------------------------------------------------------------------------
// Button Types
// ---------------------------------------------------------------------------

type UIButtonSpec struct {
	Bounds          Rectangle
	Label           string
	Font            int32
	FocusID         int32
	Disabled        bool
	Background      Color
	HoverBackground Color
	Text            Color
	Border          Color
	Radius          float32
}

type IconButtonProps struct {
	Bounds          Rectangle
	Icon            Texture2D
	IconType        int32 // UIIconType from ui_icon_types.h
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

// ---------------------------------------------------------------------------
// Text Input Types
// ---------------------------------------------------------------------------

type TextInputProps struct {
	Bounds         Rectangle
	Text           string
	CursorPosition int32
	Focused        bool
	CursorVisible  bool
	Font           int32
	FocusID        int32
	Style          UITextInputStyle
}

type TextFieldProps struct {
	Bounds         Rectangle
	Text           []byte // mutable buffer
	CursorPosition *int32
	Focused        *bool
	MaxCodepoints  int32
	Font           int32
	FocusID        int32
	Style          UITextInputStyle
	CommitPressed  *bool
}

type TextAreaProps struct {
	Bounds         Rectangle
	Text           []byte // mutable buffer
	CursorPosition *int32
	Focused        *bool
	ScrollY        *int32
	MaxCodepoints  int32
	Font           int32
	LineGap        int32
	FocusID        int32
	Placeholder    string
	Syntax         UISyntaxMode
	Style          UITextInputStyle
}

type ReadonlyTextBoxProps struct {
	Bounds  Rectangle
	Text    string
	Font    int32
	Style   UITextInputStyle
	LineGap int32
}

// ---------------------------------------------------------------------------
// Dropdown Types
// ---------------------------------------------------------------------------

type UIDropdownOption struct {
	Label    string
	FontName string
}

// ---------------------------------------------------------------------------
// Style Functions
// ---------------------------------------------------------------------------

func GetUIStyleTokens() UIStyleTokens {
	return styleTokensFromC(C.GetUIStyleTokens())
}

func SetUIStyleTokens(tokens UIStyleTokens) {
	ct := tokens.toC()
	C.SetUIStyleTokens(ct)
}

func ClearUIStyleTokensOverride() {
	C.ClearUIStyleTokensOverride()
}

func GetUIMaterialScheme() UIMaterialScheme {
	cs := C.GetUIMaterialScheme()
	return UIMaterialScheme{
		Primary:           Color{R: uint8(cs.primary.r), G: uint8(cs.primary.g), B: uint8(cs.primary.b), A: uint8(cs.primary.a)},
		OnPrimary:         Color{R: uint8(cs.on_primary.r), G: uint8(cs.on_primary.g), B: uint8(cs.on_primary.b), A: uint8(cs.on_primary.a)},
		Secondary:         Color{R: uint8(cs.secondary.r), G: uint8(cs.secondary.g), B: uint8(cs.secondary.b), A: uint8(cs.secondary.a)},
		OnSecondary:       Color{R: uint8(cs.on_secondary.r), G: uint8(cs.on_secondary.g), B: uint8(cs.on_secondary.b), A: uint8(cs.on_secondary.a)},
		Surface:           Color{R: uint8(cs.surface.r), G: uint8(cs.surface.g), B: uint8(cs.surface.b), A: uint8(cs.surface.a)},
		OnSurface:         Color{R: uint8(cs.on_surface.r), G: uint8(cs.on_surface.g), B: uint8(cs.on_surface.b), A: uint8(cs.on_surface.a)},
		SurfaceContainer:  Color{R: uint8(cs.surface_container.r), G: uint8(cs.surface_container.g), B: uint8(cs.surface_container.b), A: uint8(cs.surface_container.a)},
		SurfaceVariant:    Color{R: uint8(cs.surface_variant.r), G: uint8(cs.surface_variant.g), B: uint8(cs.surface_variant.b), A: uint8(cs.surface_variant.a)},
		OnSurfaceVariant:  Color{R: uint8(cs.on_surface_variant.r), G: uint8(cs.on_surface_variant.g), B: uint8(cs.on_surface_variant.b), A: uint8(cs.on_surface_variant.a)},
		Outline:           Color{R: uint8(cs.outline.r), G: uint8(cs.outline.g), B: uint8(cs.outline.b), A: uint8(cs.outline.a)},
		Error:             Color{R: uint8(cs.error.r), G: uint8(cs.error.g), B: uint8(cs.error.b), A: uint8(cs.error.a)},
		OnError:           Color{R: uint8(cs.on_error.r), G: uint8(cs.on_error.g), B: uint8(cs.on_error.b), A: uint8(cs.on_error.a)},
		DisabledContainer: Color{R: uint8(cs.disabled_container.r), G: uint8(cs.disabled_container.g), B: uint8(cs.disabled_container.b), A: uint8(cs.disabled_container.a)},
		DisabledContent:   Color{R: uint8(cs.disabled_content.r), G: uint8(cs.disabled_content.g), B: uint8(cs.disabled_content.b), A: uint8(cs.disabled_content.a)},
	}
}

// ---------------------------------------------------------------------------
// Button Drawing Functions
// ---------------------------------------------------------------------------

func DrawUIButton(spec UIButtonSpec) bool {
	clabel := C.CString(spec.Label)
	defer C.free(unsafe.Pointer(clabel))

	disabled := 0
	if spec.Disabled {
		disabled = 1
	}

	cspec := C.UIButtonSpec{
		bounds:           spec.Bounds.toC(),
		label:            clabel,
		font:             C.int(spec.Font),
		focus_id:         C.int(spec.FocusID),
		disabled:         C.int(disabled),
		background:       spec.Background.toC(),
		hover_background: spec.HoverBackground.toC(),
		text:             spec.Text.toC(),
		border:           spec.Border.toC(),
		radius:           C.float(spec.Radius),
	}
	return C.DrawUIButton(cspec) != 0
}

func DrawUIIconButton(props IconButtonProps) bool {
	disabled := 0
	if props.Disabled {
		disabled = 1
	}

	ctex := C.Texture{
		id:      C.uint(props.Icon.ID),
		width:   C.int(props.Icon.Width),
		height:  C.int(props.Icon.Height),
		mipmaps: C.int(props.Icon.Mipmaps),
		format:  C.int(props.Icon.Format),
	}

	cprops := C.IconButtonProps{
		bounds:           props.Bounds.toC(),
		icon:             ctex,
		icon_type:        C.UIIconType(props.IconType),
		icon_size:        C.int(props.IconSize),
		icon_padding:     C.int(props.IconPadding),
		focus_id:         C.int(props.FocusID),
		disabled:         C.int(disabled),
		background:       props.Background.toC(),
		hover_background: props.HoverBackground.toC(),
		icon_color:       props.IconColor.toC(),
		border:           props.Border.toC(),
		radius:           C.float(props.Radius),
	}
	return C.DrawUIIconButton(cprops) != 0
}

func DrawUIHref(props HrefProps) bool {
	ctext := C.CString(props.Text)
	chref := C.CString(props.Href)
	defer C.free(unsafe.Pointer(ctext))
	defer C.free(unsafe.Pointer(chref))

	disabled := 0
	if props.Disabled {
		disabled = 1
	}

	cprops := C.HrefProps{
		bounds:      props.Bounds.toC(),
		text:        ctext,
		href:        chref,
		font:        C.int(props.Font),
		focus_id:    C.int(props.FocusID),
		disabled:    C.int(disabled),
		color:       props.Color.toC(),
		hover_color: props.HoverColor.toC(),
	}
	return C.DrawUIHref(cprops) != 0
}

func DrawUITextButton(x, y int32, label string, hover *bool) bool {
	clabel := C.CString(label)
	defer C.free(unsafe.Pointer(clabel))

	var chover C.int
	clicked := C.DrawUITextButton(C.int(x), C.int(y), clabel, &chover) != 0
	if hover != nil {
		*hover = chover != 0
	}
	return clicked
}

func DrawUIGenericButton(x, y, w, h int32, label string, style UIButtonStyle, disabled bool, hover *bool) bool {
	clabel := C.CString(label)
	defer C.free(unsafe.Pointer(clabel))

	cdisabled := 0
	if disabled {
		cdisabled = 1
	}

	var chover C.int
	clicked := C.DrawUIGenericButton(C.int(x), C.int(y), C.int(w), C.int(h),
		clabel, C.UIButtonStyle(style), C.int(cdisabled), &chover) != 0
	if hover != nil {
		*hover = chover != 0
	}
	return clicked
}

func GetUIIconButtonSize(size UIIconSize) int32 {
	return int32(C.GetUIIconButtonSize(C.UIIconSize(size)))
}

func GetUIIconButtonPadding(size UIIconSize) int32 {
	return int32(C.GetUIIconButtonPadding(C.UIIconSize(size)))
}

// ---------------------------------------------------------------------------
// Text Input Functions
// ---------------------------------------------------------------------------

func DrawUITextInputControl(props TextInputProps) bool {
	ctext := C.CString(props.Text)
	defer C.free(unsafe.Pointer(ctext))

	focused := 0
	if props.Focused {
		focused = 1
	}
	cursorVisible := 0
	if props.CursorVisible {
		cursorVisible = 1
	}

	cprops := C.TextInputProps{
		bounds:          props.Bounds.toC(),
		text:            ctext,
		cursor_position: C.int(props.CursorPosition),
		focused:         C.int(focused),
		cursor_visible:  C.int(cursorVisible),
		font:            C.int(props.Font),
		focus_id:        C.int(props.FocusID),
		style:           props.Style.toC(),
	}
	return C.DrawUITextInputControl(cprops) != 0
}

func DrawUITextField(props TextFieldProps) bool {
	if len(props.Text) == 0 {
		return false
	}
	focused := C.int(0)
	if props.Focused != nil && *props.Focused {
		focused = 1
	}
	commit := C.int(0)
	if props.CommitPressed != nil && *props.CommitPressed {
		commit = 1
	}
	var pins goruntime.Pinner
	pins.Pin(&props.Text[0])
	if props.CursorPosition != nil {
		pins.Pin(props.CursorPosition)
	}
	pins.Pin(&focused)
	pins.Pin(&commit)
	defer pins.Unpin()

	cprops := C.TextFieldProps{
		bounds:           props.Bounds.toC(),
		text:             (*C.char)(unsafe.Pointer(&props.Text[0])),
		text_size:        C.size_t(len(props.Text)),
		cursor_position:  (*C.int)(unsafe.Pointer(props.CursorPosition)),
		focused:          &focused,
		max_codepoints:   C.int(props.MaxCodepoints),
		font:             C.int(props.Font),
		focus_id:         C.int(props.FocusID),
		style:            props.Style.toC(),
		filter:           nil,
		filter_user_data: nil,
		commit_pressed:   &commit,
	}
	changed := C.DrawUITextField(cprops) != 0
	if props.Focused != nil {
		*props.Focused = focused != 0
	}
	if props.CommitPressed != nil {
		*props.CommitPressed = commit != 0
	}
	return changed
}

func DrawUITextArea(props TextAreaProps) bool {
	if len(props.Text) == 0 {
		return false
	}

	var cplaceholder *C.char
	if props.Placeholder != "" {
		cplaceholder = C.CString(props.Placeholder)
		defer C.free(unsafe.Pointer(cplaceholder))
	}

	focused := C.int(0)
	if props.Focused != nil && *props.Focused {
		focused = 1
	}
	var pins goruntime.Pinner
	pins.Pin(&props.Text[0])
	if props.CursorPosition != nil {
		pins.Pin(props.CursorPosition)
	}
	if props.ScrollY != nil {
		pins.Pin(props.ScrollY)
	}
	pins.Pin(&focused)
	defer pins.Unpin()
	cprops := C.TextAreaProps{
		bounds:           props.Bounds.toC(),
		text:             (*C.char)(unsafe.Pointer(&props.Text[0])),
		text_size:        C.size_t(len(props.Text)),
		cursor_position:  (*C.int)(unsafe.Pointer(props.CursorPosition)),
		focused:          &focused,
		scroll_y:         (*C.int)(unsafe.Pointer(props.ScrollY)),
		max_codepoints:   C.int(props.MaxCodepoints),
		font:             C.int(props.Font),
		line_gap:         C.int(props.LineGap),
		focus_id:         C.int(props.FocusID),
		placeholder:      cplaceholder,
		syntax:           C.UISyntaxMode(props.Syntax),
		style:            props.Style.toC(),
		filter:           nil,
		filter_user_data: nil,
	}
	changed := C.DrawUITextArea(cprops) != 0
	if props.Focused != nil {
		*props.Focused = focused != 0
	}
	return changed
}

func DrawUIReadonlyTextBox(props ReadonlyTextBoxProps) bool {
	ctext := C.CString(props.Text)
	defer C.free(unsafe.Pointer(ctext))

	cprops := C.ReadonlyTextBoxProps{
		bounds:   props.Bounds.toC(),
		text:     ctext,
		font:     C.int(props.Font),
		style:    props.Style.toC(),
		line_gap: C.int(props.LineGap),
	}
	return C.DrawUIReadonlyTextBox(cprops) != 0
}

// ---------------------------------------------------------------------------
// Dropdown and Combobox Functions
// ---------------------------------------------------------------------------

func DrawUIDropdown(id, x, y, w, h int32, options []string, selectedIndex *int32) bool {
	if len(options) == 0 {
		return false
	}

	// Create C string array
	cstrings := make([]*C.char, len(options))
	for i, opt := range options {
		cstrings[i] = C.CString(opt)
	}
	defer func() {
		for _, cs := range cstrings {
			C.free(unsafe.Pointer(cs))
		}
	}()

	return C.DrawUIDropdown(C.int(id), C.int(x), C.int(y), C.int(w), C.int(h),
		(**C.char)(unsafe.Pointer(&cstrings[0])), C.int(len(options)),
		(*C.int)(unsafe.Pointer(selectedIndex))) != 0
}

func DrawUIDropdownEx(id, x, y, w, h int32, options []UIDropdownOption, selectedIndex *int32) bool {
	if len(options) == 0 {
		return false
	}

	// Create C option array
	copts := make([]C.UIDropdownOption, len(options))
	labels := make([]*C.char, len(options))
	fonts := make([]*C.char, len(options))

	for i, opt := range options {
		labels[i] = C.CString(opt.Label)
		if opt.FontName != "" {
			fonts[i] = C.CString(opt.FontName)
		}
		copts[i].label = labels[i]
		copts[i].font_name = fonts[i]
	}

	defer func() {
		for i := range labels {
			C.free(unsafe.Pointer(labels[i]))
			if fonts[i] != nil {
				C.free(unsafe.Pointer(fonts[i]))
			}
		}
	}()

	return C.DrawUIDropdownEx(C.int(id), C.int(x), C.int(y), C.int(w), C.int(h),
		(*C.UIDropdownOption)(unsafe.Pointer(&copts[0])), C.int(len(options)),
		(*C.int)(unsafe.Pointer(selectedIndex))) != 0
}

func DrawUILocaleDropdown(id, x, y, w, h int32, selectedIndex *int32) bool {
	return C.DrawUILocaleDropdown(C.int(id), C.int(x), C.int(y), C.int(w), C.int(h),
		(*C.int)(unsafe.Pointer(selectedIndex))) != 0
}

func SetUIDropdownClipTop(top int32) {
	C.SetUIDropdownClipTop(C.int(top))
}

func SetUIDropdownClipBottom(bottom int32) {
	C.SetUIDropdownClipBottom(C.int(bottom))
}

// ---------------------------------------------------------------------------
// Slider and Toggle Functions
// ---------------------------------------------------------------------------

func DrawUISlider(id, x, y, w int32, label string, min, max int32, value *int32, suffix, valueTextOverride string) bool {
	clabel := C.CString(label)
	defer C.free(unsafe.Pointer(clabel))

	var csuffix *C.char
	if suffix != "" {
		csuffix = C.CString(suffix)
		defer C.free(unsafe.Pointer(csuffix))
	}

	var cvalueText *C.char
	if valueTextOverride != "" {
		cvalueText = C.CString(valueTextOverride)
		defer C.free(unsafe.Pointer(cvalueText))
	}

	return C.DrawUISlider(C.int(id), C.int(x), C.int(y), C.int(w), clabel,
		C.int(min), C.int(max), (*C.int)(unsafe.Pointer(value)), csuffix, cvalueText) != 0
}

func DrawUIVerticalSlider(id, x, y, h, min, max int32, value *int32) bool {
	return C.DrawUIVerticalSlider(C.int(id), C.int(x), C.int(y), C.int(h),
		C.int(min), C.int(max), (*C.int)(unsafe.Pointer(value))) != 0
}

func DrawUIToggleSwitch(x, y, w, h int32, value *bool, offLabel, onLabel string) bool {
	coffLabel := C.CString(offLabel)
	conLabel := C.CString(onLabel)
	defer C.free(unsafe.Pointer(coffLabel))
	defer C.free(unsafe.Pointer(conLabel))

	var cvalue C.int
	if *value {
		cvalue = 1
	}

	changed := C.DrawUIToggleSwitch(C.int(x), C.int(y), C.int(w), C.int(h),
		&cvalue, coffLabel, conLabel) != 0

	*value = (cvalue != 0)
	return changed
}

func DrawUICheckboxToggle(x, y int32, label string, value *bool) bool {
	clabel := C.CString(label)
	defer C.free(unsafe.Pointer(clabel))

	var cvalue C.int
	if *value {
		cvalue = 1
	}

	changed := C.DrawUICheckboxToggle(C.int(x), C.int(y), clabel, &cvalue) != 0
	*value = (cvalue != 0)
	return changed
}

func DrawDisabledUICheckboxToggle(x, y int32, label string, value *bool, disabled bool) bool {
	clabel := C.CString(label)
	defer C.free(unsafe.Pointer(clabel))

	var cvalue C.int
	if *value {
		cvalue = 1
	}

	cdisabled := 0
	if disabled {
		cdisabled = 1
	}

	changed := C.DrawDisabledUICheckboxToggle(C.int(x), C.int(y), clabel,
		&cvalue, C.int(cdisabled)) != 0
	*value = (cvalue != 0)
	return changed
}

// ---------------------------------------------------------------------------
// Text Input Queue Functions
// ---------------------------------------------------------------------------

func QueueUITextInputCodepoint(codepoint int32) {
	C.QueueUITextInputCodepoint(C.int(codepoint))
}

func QueueUITextInputBackspace() {
	C.QueueUITextInputBackspace()
}

func QueueUITextInputEnter() {
	C.QueueUITextInputEnter()
}

func GetUITextAreaSelection(focusID int32) (start, end int32, ok bool) {
	var cstart, cend C.int
	ok = C.GetUITextAreaSelection(C.int(focusID), &cstart, &cend) != 0
	if ok {
		start = int32(cstart)
		end = int32(cend)
	}
	return
}

func SetUITextAreaSelection(focusID, anchor, cursor int32) {
	C.SetUITextAreaSelection(C.int(focusID), C.int(anchor), C.int(cursor))
}

// ---------------------------------------------------------------------------
// Clipboard
// ---------------------------------------------------------------------------

func SetUIClipboardTextValue(text string) bool {
	ctext := C.CString(text)
	defer C.free(unsafe.Pointer(ctext))
	return C.SetUIClipboardTextValue(ctext) != 0
}

func GetUIClipboardTextValue() string {
	return C.GoString(C.GetUIClipboardTextValue())
}
