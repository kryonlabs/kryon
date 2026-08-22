package kryon

import (
	"image"
	"image/color"
	"image/draw"
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

var (
	fontMu           sync.Mutex
	nextFontID       uint32 = 1
	fontsByID               = map[uint32]*uiFontSource{}
	fontsByName             = map[string]*uiFontSource{}
	activeUIFontName string
)

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
	face, err := opentype.NewFace(source.parsed, &opentype.FaceOptions{
		Size:    float64(size),
		DPI:     72,
		Hinting: xfont.HintingFull,
	})
	if err != nil {
		return nil
	}
	source.faces[size] = face
	return face
}

func drawFontText(img draw.Image, text string, x, y int, fontSize int32, c Color, fontID uint32) bool {
	face := faceForFont(fontID, fontSize)
	if face == nil {
		return false
	}
	d := &xfont.Drawer{
		Dst:  img,
		Src:  image.NewUniform(color.RGBA{R: c.R, G: c.G, B: c.B, A: c.A}),
		Face: face,
		Dot:  fixed.P(x, y+fontAscent(face)),
	}
	for _, line := range splitLines(text) {
		d.DrawString(line)
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
	maxW := fixed.Int26_6(0)
	lines := splitLines(text)
	for _, line := range lines {
		w := xfont.MeasureString(face, line)
		if w > maxW {
			maxW = w
		}
	}
	return Vector2{
		X: float32(maxW.Ceil()),
		Y: float32(maxInt(1, len(lines)) * fontLineHeight(face)),
	}, true
}

func fontTextAdvance(text string, cursor int32, fontSize int32, fontID uint32) (int, bool) {
	face := faceForFont(fontID, fontSize)
	if face == nil {
		return 0, false
	}
	pos := clampByteCursor(text, int(cursor))
	return xfont.MeasureString(face, text[:pos]).Round(), true
}

func fontTextHeight(fontSize int32, fontID uint32) (int32, bool) {
	face := faceForFont(fontID, fontSize)
	if face == nil {
		return 0, false
	}
	return int32(fontLineHeight(face)), true
}

func fontAscent(face xfont.Face) int {
	return face.Metrics().Ascent.Ceil()
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
