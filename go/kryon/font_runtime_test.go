package kryon

import "testing"

func TestNewRuntimeResolvesKryonDefaultUIFont(t *testing.T) {
	fontMu.Lock()
	previous := activeUIFontName
	activeUIFontName = ""
	fontMu.Unlock()
	defer func() {
		fontMu.Lock()
		activeUIFontName = previous
		fontMu.Unlock()
	}()

	_ = New(AppConfig{Width: 64, Height: 64})
	fontMu.Lock()
	active := activeUIFontName
	fontMu.Unlock()
	if active != defaultUIFontName {
		t.Fatalf("default UI font = %q, want %q", active, defaultUIFontName)
	}
	if faceForFont(0, Text16) == nil {
		t.Fatal("default UI font did not produce a native Go face")
	}
}
