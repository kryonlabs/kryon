package kryon

import (
	"image/png"
	"os"
	"time"
)

var compatStart = time.Now()

type compatInputRuntime interface {
	MousePosition() Vector2
	MouseButtonPressed(int32) bool
	MouseWheelMove() float32
	KeyPressed(int32) bool
	CharPressed() int32
}

func SetSingleInstance(bool) {}
func SetConfigFlags(uint)    {}
func SetExitKey(int32)       {}
func SetTargetFPS(int32)     {}

func InitWindow(width, height int32, title string) {
	Open(AppConfig{Title: title, Width: int(width), Height: int(height)})
}

func CloseWindow() { Close() }

func IsWindowReady() bool { return activeRuntime != nil }

func BeginDrawing() { BeginFrame() }
func EndDrawing()   { EndFrame() }

func DrawRectangleRec(r Rectangle, c Color) {
	Rect(int32(r.X), int32(r.Y), int32(r.Width), int32(r.Height), c)
}

func DrawRectangleLinesEx(r Rectangle, thick int32, c Color) {
	for i := int32(0); i < thick; i++ {
		x := int32(r.X) + i
		y := int32(r.Y) + i
		w := int32(r.Width) - i*2
		h := int32(r.Height) - i*2
		Line(x, y, x+w, y, c)
		Line(x, y+h, x+w, y+h, c)
		Line(x, y, x, y+h, c)
		Line(x+w, y, x+w, y+h, c)
	}
}

func DrawLine(x1, y1, x2, y2 int32, c Color) {
	Line(x1, y1, x2, y2, c)
}

func CheckCollisionPointRec(p Vector2, r Rectangle) bool {
	return pointInRect(p.X, p.Y, r)
}

func DrawTextEx(_ Font, text string, pos Vector2, size float32, _ float32, c Color) {
	Text(text, int32(pos.X), int32(pos.Y), int32(size), c)
}

func MeasureTextEx(_ Font, text string, size float32, _ float32) Vector2 {
	return Vector2{X: float32(len([]rune(text))) * size * 0.55, Y: size}
}

func LoadFontFromMemory(string, []byte, int32, []rune) Font {
	return Font{Texture: Texture2D{ID: 1}}
}

func SetTextureFilter(Texture2D, int32) {}
func RegisterUIFontData(string, string, []byte, []rune) bool {
	return true
}
func UseUIFont(string) {}

func GetMousePosition() Vector2 {
	if rt, ok := active().(compatInputRuntime); ok {
		return rt.MousePosition()
	}
	return Vector2{}
}

func IsMouseButtonPressed(button int32) bool {
	if rt, ok := active().(compatInputRuntime); ok {
		return rt.MouseButtonPressed(button)
	}
	return false
}

func GetMouseWheelMove() float32 {
	if rt, ok := active().(compatInputRuntime); ok {
		return rt.MouseWheelMove()
	}
	return 0
}

func IsKeyPressed(key int32) bool {
	if rt, ok := active().(compatInputRuntime); ok {
		return rt.KeyPressed(key)
	}
	return false
}

func GetCharPressed() int32 {
	if rt, ok := active().(compatInputRuntime); ok {
		return rt.CharPressed()
	}
	return 0
}

func GetTime() float64 {
	return time.Since(compatStart).Seconds()
}

func TakeScreenshot(path string) {
	if path == "" {
		return
	}
	file, err := os.Create(path)
	if err != nil {
		return
	}
	defer file.Close()
	_ = png.Encode(file, RenderCurrentFrame())
}
