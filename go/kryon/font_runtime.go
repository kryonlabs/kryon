package kryon

import (
	"image"
	"image/color"
	"image/draw"
	"os"
	"path/filepath"
	"strings"
	"sync"

	xfont "golang.org/x/image/font"
	"golang.org/x/image/font/opentype"
	"golang.org/x/image/math/fixed"
)

type uiFontSource struct {
	id     uint32
	name   string
	typ    string
	data   []byte
	parsed *opentype.Font
	faces  map[int32]xfont.Face
}

// Keep advance truncation in font units. Rounding to 26.6 pixels first can
// promote a 7.99-pixel advance to 8, unlike the C rasterizer's integer atlas.
type pixelFontFace struct {
	xfont.Face
	parsed   *opentype.Font
	units    fixed.Int26_6
	scale    float32
	advances map[rune]fixed.Int26_6
}

func (face *pixelFontFace) GlyphAdvance(glyph rune) (fixed.Int26_6, bool) {
	if advance, ok := face.advances[glyph]; ok {
		return advance, true
	}
	index, err := face.parsed.GlyphIndex(nil, glyph)
	if err != nil {
		return 0, false
	}
	advance, err := face.parsed.GlyphAdvance(nil, index, face.units, xfont.HintingNone)
	if err != nil {
		return 0, false
	}
	pixels := fontUnitAdvance(advance, face.scale)
	face.advances[glyph] = pixels
	return pixels, true
}

func fontUnitAdvance(advance fixed.Int26_6, scale float32) fixed.Int26_6 {
	return fixed.I(int(float32(advance) / 64 * scale))
}

var (
	fontMu           sync.Mutex
	nextFontID       uint32 = 1
	fontsByID               = map[uint32]*uiFontSource{}
	fontsByName             = map[string]*uiFontSource{}
	activeUIFontName string
)

const defaultUIFontName = "kryon-default"

// ensureDefaultUIFont gives every native Go host the same default UI face as
// the C runtime. Applications can still replace it with RegisterUIFontData and
// UseUIFont; failure to resolve a packaged/system font keeps the small built-in
// renderer fallback available for minimal environments.
func ensureDefaultUIFont() {
	fontMu.Lock()
	if activeUIFontName != "" {
		fontMu.Unlock()
		return
	}
	fontMu.Unlock()

	paths := []string{
		"fonts/noto/NotoSans-Regular.ttf",
		"../fonts/noto/NotoSans-Regular.ttf",
		"../../fonts/noto/NotoSans-Regular.ttf",
		"vendor/kryon/fonts/noto/NotoSans-Regular.ttf",
		"/usr/local/share/fonts/noto/NotoSans-Regular.ttf",
		"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
	}
	if executable, err := os.Executable(); err == nil {
		dir := filepath.Dir(executable)
		paths = append([]string{
			filepath.Join(dir, "fonts", "noto", "NotoSans-Regular.ttf"),
			filepath.Join(dir, "..", "share", "kryon", "fonts", "noto", "NotoSans-Regular.ttf"),
		}, paths...)
	}
	for _, path := range paths {
		data, err := os.ReadFile(path)
		if err != nil {
			continue
		}
		if _, ok := registerFontData(defaultUIFontName, ".ttf", data); ok {
			semiboldPath := filepath.Join(filepath.Dir(path), "NotoSans-SemiBold.ttf")
			if semibold, err := os.ReadFile(semiboldPath); err == nil && registeredTypeface("semibold") == 0 {
				registerFontData("semibold", ".ttf", semibold)
			}
			useUIFont(defaultUIFontName)
			return
		}
	}
}

func registeredTypeface(name string) uint32 {
	fontMu.Lock()
	defer fontMu.Unlock()
	if source := fontsByName[name]; source != nil {
		return source.id
	}
	return 0
}

func registerFontData(name, typ string, data []byte) (uint32, bool) {
	if len(data) == 0 {
		return 0, false
	}
	parsed, err := opentype.Parse(data)
	if err != nil {
		return 0, false
	}
	fontMu.Lock()
	defer fontMu.Unlock()
	if existing := fontsByName[name]; existing != nil && name != "" {
		existing.typ = typ
		existing.data = append(existing.data[:0], data...)
		existing.parsed = parsed
		for _, face := range existing.faces {
			face.Close()
		}
		existing.faces = map[int32]xfont.Face{}
		return existing.id, true
	}
	id := nextFontID
	nextFontID++
	source := &uiFontSource{
		id:     id,
		name:   name,
		typ:    typ,
		data:   append([]byte(nil), data...),
		parsed: parsed,
		faces:  map[int32]xfont.Face{},
	}
	fontsByID[id] = source
	if name != "" {
		fontsByName[name] = source
	}
	return id, true
}

func useUIFont(name string) bool {
	fontMu.Lock()
	defer fontMu.Unlock()
	if fontsByName[name] == nil {
		return false
	}
	activeUIFontName = name
	return true
}

func faceForFont(id uint32, size int32) xfont.Face {
	if size <= 0 {
		size = Text16
	}
	fontMu.Lock()
	defer fontMu.Unlock()
	source := fontsByID[id]
	if source == nil && activeUIFontName != "" {
		source = fontsByName[activeUIFontName]
	}
	if source == nil || source.parsed == nil {
		return nil
	}
	if face := source.faces[size]; face != nil {
		return face
	}
	// C's font rasterizer interprets size as ascent-to-descent pixel height,
	// not pixels per em. Convert that contract before creating the Go face.
	em := float64(source.parsed.UnitsPerEm())
	metrics, err := source.parsed.Metrics(nil, fixed.Int26_6(em*64), xfont.HintingNone)
	if err != nil {
		return nil
	}
	span := float64(metrics.Ascent+metrics.Descent) / 64
	if span <= 0 {
		return nil
	}
	face, err := opentype.NewFace(source.parsed, &opentype.FaceOptions{
		Size:    float64(size) * em / span,
		DPI:     72,
		Hinting: xfont.HintingNone,
	})
	if err != nil {
		return nil
	}
	pixelFace := &pixelFontFace{Face: face, parsed: source.parsed,
		units: fixed.Int26_6(em * 64), scale: float32(size) / float32(span),
		advances: make(map[rune]fixed.Int26_6)}
	source.faces[size] = pixelFace
	return pixelFace
}

func fontTextBaseline(text string, boxY, boxHeight int, fontSize int32, fontID uint32) int {
	face := faceForFont(fontID, fontSize)
	if face == nil {
		return boxY + (boxHeight-7*glyphScale(fontSize))/2
	}
	bounds, _ := xfont.BoundString(face, text)
	inkTop := bounds.Min.Y.Floor()
	inkHeight := bounds.Max.Y.Ceil() - inkTop
	// Match TextBaselineY: round the complete offset, including the glyph's
	// atlas-relative top. Integer division loses the half-pixel tie.
	return boxY + int(float64(boxHeight-inkHeight)*0.5-float64(inkTop+fontAscent(face))+0.5)
}

func drawFontText(img draw.Image, text string, x, y int, fontSize int32, c Color, fontID uint32, letterSpacing ...int32) bool {
	spacing := int32(0)
	if len(letterSpacing) > 0 {
		spacing = max(letterSpacing[0], 0)
	}
	face := faceForFont(fontID, fontSize)
	if face == nil {
		return false
	}
	d := &xfont.Drawer{
		Dst:  img,
		Src:  image.NewUniform(color.NRGBA{R: c.R, G: c.G, B: c.B, A: c.A}),
		Face: face,
		Dot:  fixed.P(x, y+fontAscent(face)),
	}
	for _, line := range splitLines(text) {
		for _, glyph := range line {
			start := d.Dot.X
			d.DrawString(string(glyph))
			advance, _ := face.GlyphAdvance(glyph)
			d.Dot.X = start + fixed.I(advance.Floor()+int(spacing))
		}
		d.Dot.X = fixed.I(x)
		d.Dot.Y += fixed.I(fontLineHeight(face))
	}
	return true
}

func measureFontText(text string, fontSize int32, fontID uint32) (Vector2, bool) {
	face := faceForFont(fontID, fontSize)
	if face == nil {
		return Vector2{}, false
	}
	maxW := 0
	lines := splitLines(text)
	for _, line := range lines {
		w := fontPixelAdvance(face, line)
		if w > maxW {
			maxW = w
		}
	}
	return Vector2{
		X: float32(maxW),
		Y: float32(maxInt(1, len(lines)) * fontLineHeight(face)),
	}, true
}

func fontTextAdvance(text string, cursor int32, fontSize int32, fontID uint32) (int, bool) {
	face := faceForFont(fontID, fontSize)
	if face == nil {
		return 0, false
	}
	pos := clampByteCursor(text, int(cursor))
	return fontPixelAdvance(face, text[:pos]), true
}

// The C atlas stores truncated advances and does not apply pair kerning.
// Use that same pixel grid for drawing, measurement, and caret positioning.
func fontPixelAdvance(face xfont.Face, text string) int {
	width := 0
	for _, glyph := range text {
		advance, _ := face.GlyphAdvance(glyph)
		width += advance.Floor()
	}
	return width
}

func fontTextHeight(fontSize int32, fontID uint32) (int32, bool) {
	face := faceForFont(fontID, fontSize)
	if face == nil {
		return 0, false
	}
	return int32(fontLineHeight(face)), true
}

func fontAscent(face xfont.Face) int {
	return face.Metrics().Ascent.Floor()
}

func fontLineHeight(face xfont.Face) int {
	metrics := face.Metrics()
	return maxInt(1, (metrics.Ascent + metrics.Descent).Ceil())
}

func splitLines(text string) []string {
	if text == "" {
		return []string{""}
	}
	return strings.Split(text, "\n")
}
