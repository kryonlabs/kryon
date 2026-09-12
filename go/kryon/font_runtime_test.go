package kryon

import "testing"

func TestNewRuntimeResolvesKryonDefaultTextFont(t *testing.T) {
	fontMu.Lock()
	previous := activeTextFontName
	activeTextFontName = ""
	fontMu.Unlock()
	defer func() {
		fontMu.Lock()
		activeTextFontName = previous
		fontMu.Unlock()
	}()

	_ = New(AppConfig{Width: 64, Height: 64})
	fontMu.Lock()
	active := activeTextFontName
	fontMu.Unlock()
	if active != defaultTextFontName {
		t.Fatalf("default UI font = %q, want %q", active, defaultTextFontName)
	}
	if faceForFont(0, Text16) == nil {
		t.Fatal("default UI font did not produce a native Go face")
	}
}
