package kryon

import "testing"

func useMaterialStyleForTest(t *testing.T) {
	t.Helper()
	ClearStylePacks()
	if !RegisterBuiltInStylePacks() {
		t.Fatal("built-in styles did not register")
	}
	if !SetActiveStylePack("material") {
		t.Fatal("material style did not activate")
	}
	t.Cleanup(ClearStylePacks)
}
