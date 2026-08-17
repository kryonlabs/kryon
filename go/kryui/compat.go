// Package kryui binds the vendored kryon static library (libkryon.a +
// libraylib.a, SDL2 backend) for the app GUI.
//
// This file is the raylib-compatibility layer: it mirrors the subset of the
// raylib-go API the GUI used (types, window/input/draw calls, fonts) so tab
// code reads unchanged after the `rl.` -> `kryui.` rename. Higher-level kryon
// toolkit bindings (ui_tk, themes, scroll) layer on top of this later.
package kryui

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: ${SRCDIR}/../../build/linux-x86_64/libkryon.a ${SRCDIR}/../../build/linux-x86_64/raylib/libraylib.a -lSDL2 -lGL -lX11 -lm -lpthread -ldl

#include <stdlib.h>
#include <kryon.h>
*/
import "C"

import (
	"math"
	"unsafe"
)

// ---------------------------------------------------------------------------
// Value types (field names match the old raylib-go shapes)
// ---------------------------------------------------------------------------

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

type GlyphInfo struct {
	Value    int32
	OffsetX  int32
	OffsetY  int32
	AdvanceX int32
}

// Font mirrors the fields app reads; the C font travels along unexported so
// it can be handed back to draw/measure calls without re-marshalling.
type Font struct {
	Texture    Texture2D
	BaseSize   int32
	GlyphCount int32
	cf         C.Font
}

// Image is an opaque CPU-side image (used for the window icon).
type Image struct {
	ci C.Image
}

// Width reports the image width in pixels; 0 means the image failed to load.
func (i Image) Width() int32 { return int32(i.ci.width) }

func NewVector2(x, y float32) Vector2 { return Vector2{X: x, Y: y} }

func NewRectangle(x, y, w, h float32) Rectangle {
	return Rectangle{X: x, Y: y, Width: w, Height: h}
}

func (r Rectangle) toC() C.Rectangle {
	return C.Rectangle{x: C.float(r.X), y: C.float(r.Y),
		width: C.float(r.Width), height: C.float(r.Height)}
}

func rectFromC(cr C.Rectangle) Rectangle {
	return Rectangle{X: float32(cr.x), Y: float32(cr.y),
		Width: float32(cr.width), Height: float32(cr.height)}
}

func (v Vector2) toC() C.Vector2 {
	return C.Vector2{x: C.float(v.X), y: C.float(v.Y)}
}

func vecFromC(cv C.Vector2) Vector2 {
	return Vector2{X: float32(cv.x), Y: float32(cv.y)}
}

func (c Color) toC() C.Color {
	return C.Color{r: C.uchar(c.R), g: C.uchar(c.G), b: C.uchar(c.B), a: C.uchar(c.A)}
}

func (f Font) toC() C.Font   { return f.cf }
func (i Image) toC() C.Image { return i.ci }

func fontFromC(cf C.Font) Font {
	return Font{
		Texture:    Texture2D{ID: uint32(cf.texture.id), Width: int32(cf.texture.width), Height: int32(cf.texture.height), Mipmaps: int32(cf.texture.mipmaps), Format: int32(cf.texture.format)},
		BaseSize:   int32(cf.baseSize),
		GlyphCount: int32(cf.glyphCount),
		cf:         cf,
	}
}

// ---------------------------------------------------------------------------
// Constants (values from kryon_compat.generated.h)
// ---------------------------------------------------------------------------

const (
	FlagWindowResizable uint = 0x00000004

	MouseButtonLeft   int32 = 0
	MouseButtonRight  int32 = 1
	MouseButtonMiddle int32 = 2

	FilterPoint     int32 = 0
	FilterBilinear  int32 = 1
	FilterTrilinear int32 = 2

	KeyNull         int32 = 0
	KeyZero         int32 = 48
	KeyOne          int32 = 49
	KeyTwo          int32 = 50
	KeyC            int32 = 67
	KeyH            int32 = 72
	KeyL            int32 = 76
	KeyN            int32 = 78
	KeyT            int32 = 84
	KeyV            int32 = 86
	KeyX            int32 = 88
	KeyEscape       int32 = 256
	KeyEnter        int32 = 257
	KeyBackspace    int32 = 259
	KeyRight        int32 = 262
	KeyLeft         int32 = 263
	KeyDown         int32 = 264
	KeyUp           int32 = 265
	KeyLeftControl  int32 = 341
	KeyRightControl int32 = 345
)

// ---------------------------------------------------------------------------
// Window / lifecycle
// ---------------------------------------------------------------------------

func InitWindow(w, h int32, title string) {
	ct := C.CString(title)
	defer C.free(unsafe.Pointer(ct))
	C.InitWindow(C.int(w), C.int(h), ct)
}

func CloseWindow()            { C.CloseWindow() }
func IsWindowReady() bool     { return bool(C.IsWindowReady()) }
func WindowShouldClose() bool { return bool(C.WindowShouldClose()) }

func SetConfigFlags(flags uint)   { C.SetConfigFlags(C.uint(flags)) }
func SetTargetFPS(fps int32)      { C.SetTargetFPS(C.int(fps)) }
func SetExitKey(key int32)        { C.SetExitKey(C.int(key)) }
func SetWindowMinSize(w, h int32) { C.SetWindowMinSize(C.int(w), C.int(h)) }
func GetScreenWidth() int32       { return int32(C.GetScreenWidth()) }
func GetScreenHeight() int32      { return int32(C.GetScreenHeight()) }
func GetWindowScaleDPI() Vector2  { return vecFromC(C.GetWindowScaleDPI()) }
func TakeScreenshot(fileName string) {
	cf := C.CString(fileName)
	defer C.free(unsafe.Pointer(cf))
	C.TakeScreenshot(cf)
}

func SetWindowIcon(img Image) { C.SetWindowIcon(img.toC()) }

// ---------------------------------------------------------------------------
// Clipboard
// ---------------------------------------------------------------------------

func SetClipboardText(text string) {
	ct := C.CString(text)
	defer C.free(unsafe.Pointer(ct))
	C.SetClipboardText(ct)
}

func GetClipboardText() string {
	return C.GoString(C.GetClipboardText())
}

// ---------------------------------------------------------------------------
// Frame / drawing
// ---------------------------------------------------------------------------

func ClearBackground(c Color) { C.ClearBackground(c.toC()) }
func BeginDrawing()           { C.BeginDrawing() }
func EndDrawing()             { C.EndDrawing() }

// BeginUIFrame initializes Kryon's per-frame viewport, camera, clipping and
// input state.  Raw raylib-style drawing does not need it, but higher-level
// Kryon widgets (notably scroll containers) do.
func BeginUIFrame(w, h int32, dpi float32) {
	C.BeginUIFrame(C.int(w), C.int(h), C.float(dpi))
}
func BeginScissorMode(x, y, w, h int32) {
	C.BeginScissorMode(C.int(x), C.int(y), C.int(w), C.int(h))
}
func EndScissorMode() { C.EndScissorMode() }

func DrawRectangle(x, y, w, h int32, c Color) {
	C.DrawRectangle(C.int(x), C.int(y), C.int(w), C.int(h), c.toC())
}

func DrawRectangleRec(r Rectangle, c Color) { C.DrawRectangleRec(r.toC(), c.toC()) }

func DrawRectangleGradientH(x, y, w, h int32, left, right Color) {
	C.DrawRectangleGradientH(C.int(x), C.int(y), C.int(w), C.int(h), left.toC(), right.toC())
}

func DrawCircleV(center Vector2, radius float32, color Color) {
	C.DrawCircleV(center.toC(), C.float(radius), color.toC())
}

func DrawRing(center Vector2, innerRadius, outerRadius, startAngle, endAngle float32, segments int32, color Color) {
	C.DrawRing(center.toC(), C.float(innerRadius), C.float(outerRadius),
		C.float(startAngle), C.float(endAngle), C.int(segments), color.toC())
}

func GetTime() float64 { return float64(C.GetTime()) }

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

func IsKeyPressed(key int32) bool { return bool(C.IsKeyPressed(C.int(key))) }
func IsKeyDown(key int32) bool    { return bool(C.IsKeyDown(C.int(key))) }
func GetCharPressed() int32       { return int32(C.GetCharPressed()) }

func IsMouseButtonDown(b int32) bool     { return bool(C.IsMouseButtonDown(C.int(b))) }
func IsMouseButtonReleased(b int32) bool { return bool(C.IsMouseButtonReleased(C.int(b))) }
func GetMousePosition() Vector2          { return vecFromC(C.GetMousePosition()) }
func GetMouseWheelMove() float32         { return float32(C.GetMouseWheelMove()) }

// ---------------------------------------------------------------------------
// Collision / math
// ---------------------------------------------------------------------------

func CheckCollisionPointRec(p Vector2, r Rectangle) bool {
	return bool(C.CheckCollisionPointRec(p.toC(), r.toC()))
}

func CheckCollisionRecs(a, b Rectangle) bool {
	return bool(C.CheckCollisionRecs(a.toC(), b.toC()))
}

func Vector2Distance(a, b Vector2) float32 {
	dx, dy := a.X-b.X, a.Y-b.Y
	return float32(math.Sqrt(float64(dx*dx + dy*dy)))
}

// ---------------------------------------------------------------------------
// Text / fonts
// ---------------------------------------------------------------------------

func MeasureTextEx(f Font, text string, size, spacing float32) Vector2 {
	ct := C.CString(text)
	defer C.free(unsafe.Pointer(ct))
	return vecFromC(C.MeasureTextEx(f.toC(), ct, C.float(size), C.float(spacing)))
}

func DrawTextEx(f Font, text string, pos Vector2, size, spacing float32, c Color) {
	ct := C.CString(text)
	defer C.free(unsafe.Pointer(ct))
	C.DrawTextEx(f.toC(), ct, pos.toC(), C.float(size), C.float(spacing), c.toC())
}

func LoadFontFromMemory(fileType string, data []byte, fontSize int32, cps []rune) Font {
	ft := C.CString(fileType)
	defer C.free(unsafe.Pointer(ft))
	var cpArr *C.int
	if len(cps) > 0 {
		cpArr = (*C.int)(unsafe.Pointer(&cps[0]))
	}
	return fontFromC(C.LoadFontFromMemory(ft, (*C.uchar)(unsafe.Pointer(&data[0])),
		C.int(len(data)), C.int(fontSize), cpArr, C.int(len(cps))))
}

func UnloadFont(f Font) { C.UnloadFont(f.toC()) }

func GetGlyphInfo(f Font, codepoint int32) GlyphInfo {
	g := C.GetGlyphInfo(f.toC(), C.int(codepoint))
	return GlyphInfo{Value: int32(g.value), OffsetX: int32(g.offsetX),
		OffsetY: int32(g.offsetY), AdvanceX: int32(g.advanceX)}
}

func SetTextureFilter(t Texture2D, filter int32) {
	ct := C.Texture{id: C.uint(t.ID), width: C.int(t.Width), height: C.int(t.Height),
		mipmaps: C.int(t.Mipmaps), format: C.int(t.Format)}
	C.SetTextureFilter(ct, C.int(filter))
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

func LoadImageFromMemory(fileType string, data []byte) Image {
	ft := C.CString(fileType)
	defer C.free(unsafe.Pointer(ft))
	return Image{ci: C.LoadImageFromMemory(ft, (*C.uchar)(unsafe.Pointer(&data[0])), C.int(len(data)))}
}

func UnloadImage(img Image) { C.UnloadImage(img.toC()) }
