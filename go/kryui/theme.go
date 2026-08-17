// Kryon theme system bindings - use Kryon's built-in fonts and themes
package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: ${SRCDIR}/../../build/linux-x86_64/libkryon.a ${SRCDIR}/../../build/linux-x86_64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl

#include <stdlib.h>
#include <kryon.h>
#include <theme.h>
#include <ui_core.h>
#include <ui_dpi.h>
#include <ui_text.h>
#include <ui_tree.h>
*/
import "C"

import "unsafe"

// ---------------------------------------------------------------------------
// UI Text Sizes (from ui_core.h)
// ---------------------------------------------------------------------------

const (
	UIText12 = 12
	UIText14 = 14
	UIText16 = 16
	UIText18 = 18
	UIText20 = 20
	UIText24 = 24
	UIText32 = 32
	UIText48 = 48
)

// ---------------------------------------------------------------------------
// Theme Functions
// ---------------------------------------------------------------------------

// SetCurrentTheme sets the theme by ID and dark mode
func SetCurrentTheme(themeID int32, darkMode bool) {
	dark := 0
	if darkMode {
		dark = 1
	}
	C.SetCurrentTheme(C.int(themeID), C.int(dark))
}

// SetThemeDarkMode sets whether the theme is in dark mode
func SetThemeDarkMode(dark bool) {
	C.SetThemeDarkMode(C.bool(dark))
}

// GetThemeDarkMode returns whether the current theme is in dark mode
func GetThemeDarkMode() bool {
	return bool(C.GetThemeDarkMode())
}

// GetThemeBackground returns the theme's background color
func GetThemeBackground() Color {
	c := C.GetThemeBackground()
	return Color{R: uint8(c.r), G: uint8(c.g), B: uint8(c.b), A: uint8(c.a)}
}

// GetThemeSurface returns the theme's surface color
func GetThemeSurface() Color {
	c := C.GetThemeSurface()
	return Color{R: uint8(c.r), G: uint8(c.g), B: uint8(c.b), A: uint8(c.a)}
}

// GetThemeText returns the theme's text color
func GetThemeText() Color {
	c := C.GetThemeText()
	return Color{R: uint8(c.r), G: uint8(c.g), B: uint8(c.b), A: uint8(c.a)}
}

// GetThemeCircle returns the theme's circle color
func GetThemeCircle() Color {
	c := C.GetThemeCircle()
	return Color{R: uint8(c.r), G: uint8(c.g), B: uint8(c.b), A: uint8(c.a)}
}

// GetThemeLink returns the theme's link color
func GetThemeLink() Color {
	c := C.GetThemeLink()
	return Color{R: uint8(c.r), G: uint8(c.g), B: uint8(c.b), A: uint8(c.a)}
}

// GetThemeIcon returns the theme's icon tint color
func GetThemeIcon() Color {
	c := C.GetThemeIcon()
	return Color{R: uint8(c.r), G: uint8(c.g), B: uint8(c.b), A: uint8(c.a)}
}

// GetThemeButton returns the theme's button color
func GetThemeButton() Color {
	c := C.GetThemeButton()
	return Color{R: uint8(c.r), G: uint8(c.g), B: uint8(c.b), A: uint8(c.a)}
}

// GetThemeButtonHover returns the theme's button hover color
func GetThemeButtonHover() Color {
	c := C.GetThemeButtonHover()
	return Color{R: uint8(c.r), G: uint8(c.g), B: uint8(c.b), A: uint8(c.a)}
}

// ---------------------------------------------------------------------------
// DPI Scaling Functions
// ---------------------------------------------------------------------------

// ScaleUIPx scales a pixel value by the current DPI scale
func ScaleUIPx(px int32) int32 {
	return int32(C.ScaleUIPx(C.int(px)))
}

// ---------------------------------------------------------------------------
// UI Core Drawing Functions
// ---------------------------------------------------------------------------

// Background draws a full-screen background with the given color
func Background(color Color) {
	C.Background(color.toC())
}

// Text draws text at the specified position with Kryon's theme font
func Text(text string, x, y, fontSize int32, color Color) {
	ctext := C.CString(text)
	defer C.free(unsafe.Pointer(ctext))
	C.Text(ctext, C.int(x), C.int(y), C.int(fontSize), color.toC())
}

// TextFormat formats and draws text (like printf) - useful for dynamic text
func TextFormat(format string, x, y, fontSize int32, color Color, args ...interface{}) {
	// Note: Go formatting - use fmt.Sprintf before calling Text
	// This is a direct wrapper for the C TextFormat which uses C formatting
	cformat := C.CString(format)
	defer C.free(unsafe.Pointer(cformat))
	// For safety, we'll just call Text with the format string directly
	// Go users should use fmt.Sprintf + Text instead
	C.Text(cformat, C.int(x), C.int(y), C.int(fontSize), color.toC())
}

// MeasureText measures the width of text at the given font size
func MeasureText(text string, fontSize int32) int32 {
	ctext := C.CString(text)
	defer C.free(unsafe.Pointer(ctext))
	return int32(C.MeasureText(ctext, C.int(fontSize)))
}

// RegisterUIFont adds a loaded font to Kryon's fallback-aware text system.
func RegisterUIFont(name string, font Font) bool {
	cn := C.CString(name)
	defer C.free(unsafe.Pointer(cn))
	return C.RegisterUIFont(cn, font.toC()) != 0
}

// RegisterUIFontData registers embedded font bytes directly. Applications do
// not need to create temporary files or manage a raylib Font lifetime.
func RegisterUIFontData(name, fileType string, data []byte, codepoints []rune) bool {
	if len(data) == 0 {
		return false
	}
	cn := C.CString(name)
	ct := C.CString(fileType)
	defer C.free(unsafe.Pointer(cn))
	defer C.free(unsafe.Pointer(ct))
	cps := make([]C.int, len(codepoints))
	for i, cp := range codepoints {
		cps[i] = C.int(cp)
	}
	var cpPtr *C.int
	if len(cps) > 0 {
		cpPtr = &cps[0]
	}
	return C.RegisterUIFontSource(cn, ct,
		(*C.uchar)(unsafe.Pointer(&data[0])), C.uint(len(data)), cpPtr,
		C.int(len(cps))) != 0
}

// UseUIFont selects the named font as the primary UI font.
func UseUIFont(name string) bool {
	cn := C.CString(name)
	defer C.free(unsafe.Pointer(cn))
	return C.UseUIFont(cn) != 0
}

// PushUIFont temporarily selects a registered font; pass the returned token
// to PopUIFont after drawing.
func PushUIFont(name string) int32 {
	cn := C.CString(name)
	defer C.free(unsafe.Pointer(cn))
	return int32(C.PushUIFont(cn))
}

func PopUIFont(token int32) { C.PopUIFont(C.int(token)) }

// DrawUIText uses Kryon's fallback fonts, input capture, I-beam cursor and
// character-level mouse selection/Ctrl+C behavior.
func DrawUIText(text string, x, y, fontSize int32, color Color) {
	ct := C.CString(text)
	defer C.free(unsafe.Pointer(ct))
	C.DrawUIText(ct, C.int(x), C.int(y), C.int(fontSize), color.toC())
}

func MeasureUIText(text string, fontSize int32) int32 {
	ct := C.CString(text)
	defer C.free(unsafe.Pointer(ct))
	return int32(C.MeasureUIText(ct, C.int(fontSize)))
}

func PushUITextSelectable(selectable bool) int32 {
	v := 0
	if selectable {
		v = 1
	}
	return int32(C.PushUITextSelectable(C.int(v)))
}

func PopUITextSelectable(token int32) { C.PopUITextSelectable(C.int(token)) }

func UIInputCapturesClick(point Vector2) bool {
	return C.UIInputCapturesClick(point.toC()) != 0
}

// Slider draws a slider control
func Slider(id, x, y, w int32, label string, min, max int32, value *int32) bool {
	clabel := C.CString(label)
	defer C.free(unsafe.Pointer(clabel))
	return C.Slider(C.int(id), C.int(x), C.int(y), C.int(w), clabel,
		C.int(min), C.int(max), (*C.int)(unsafe.Pointer(value)), nil, nil) != 0
}

// ---------------------------------------------------------------------------
// Button Helper (from ui_tk.h via ui_core.h)
// ---------------------------------------------------------------------------

// ButtonProps holds properties for a simple button
type ButtonProps struct {
	Bounds   Rectangle
	Label    string
	Style    UIButtonStyle
	ID       int32
	Disabled bool
}

// Button draws a button with the given properties and returns true if clicked
func Button(props ButtonProps) bool {
	clabel := C.CString(props.Label)
	defer C.free(unsafe.Pointer(clabel))

	disabled := 0
	if props.Disabled {
		disabled = 1
	}

	// Use the generic button function
	var hover C.int
	result := C.DrawUIGenericButton(
		C.int(props.Bounds.X),
		C.int(props.Bounds.Y),
		C.int(props.Bounds.Width),
		C.int(props.Bounds.Height),
		clabel,
		C.UIButtonStyle(props.Style),
		C.int(disabled),
		&hover,
	)
	return result != 0
}

// ---------------------------------------------------------------------------
// Additional Control Wrappers (using Kryon widgets)
// ---------------------------------------------------------------------------

// Radio draws a radio button and returns the ID if clicked (0 otherwise)
func Radio(props RadioButtonProps) int32 {
	clabel := C.CString(props.Label)
	defer C.free(unsafe.Pointer(clabel))

	disabled := 0
	if props.Disabled {
		disabled = 1
	}
	checked := 0
	if props.Checked {
		checked = 1
	}

	// Create the C struct
	cprops := C.RadioButtonProps{
		bounds:   props.Bounds.toC(),
		label:    clabel,
		id:       C.int(props.ID),
		checked:  C.int(checked),
		disabled: C.int(disabled),
	}

	result := C.Radio(cprops)
	return int32(result)
}

// Spinbox draws a spinbox control
func Spinbox(props SpinboxProps) bool {
	disabled := 0
	if props.Disabled {
		disabled = 1
	}
	wrap := 0
	if props.Wrap {
		wrap = 1
	}

	var cvalueText *C.char
	if props.ValueText != "" {
		cvalueText = C.CString(props.ValueText)
		defer C.free(unsafe.Pointer(cvalueText))
	}

	cprops := C.SpinboxProps{
		bounds:     props.Bounds.toC(),
		id:         C.int(props.ID),
		min:        C.int(props.Min),
		max:        C.int(props.Max),
		step:       C.int(props.Step),
		value:      (*C.int)(unsafe.Pointer(props.Value)),
		disabled:   C.int(disabled),
		value_text: cvalueText,
		wrap:       C.int(wrap),
	}

	return C.Spinbox(cprops) != 0
}

// Combobox draws a combobox dropdown control
func Combobox(props ComboboxProps) bool {
	if len(props.Options) == 0 {
		return false
	}

	disabled := 0
	if props.Disabled {
		disabled = 1
	}

	// Create C string array
	cstrings := make([]*C.char, len(props.Options))
	for i, opt := range props.Options {
		cstrings[i] = C.CString(opt)
	}
	defer func() {
		for _, cs := range cstrings {
			C.free(unsafe.Pointer(cs))
		}
	}()

	cprops := C.ComboboxProps{
		bounds:         props.Bounds.toC(),
		id:             C.int(props.ID),
		options:        (**C.char)(unsafe.Pointer(&cstrings[0])),
		option_count:   C.int(len(props.Options)),
		selected_index: (*C.int)(unsafe.Pointer(props.SelectedIndex)),
		disabled:       C.int(disabled),
	}

	return C.Combobox(cprops) != 0
}

// Progress draws a progress bar
func Progress(props ProgressBarProps) {
	clabel := C.CString(props.Label)
	defer C.free(unsafe.Pointer(clabel))

	cprops := C.ProgressBarProps{
		bounds: props.Bounds.toC(),
		min:    C.int(props.Min),
		max:    C.int(props.Max),
		value:  C.int(props.Value),
		label:  clabel,
	}

	C.Progress(cprops)
}
