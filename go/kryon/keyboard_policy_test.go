package kryon

import "testing"

func TestCanonicalKeyboardPolicy(t *testing.T) {
	for mask := 0; mask < 8; mask++ {
		focused, captured, escape := mask&1 != 0, mask&2 != 0, mask&4 != 0
		if Menu_MenuEscapeShouldClose(focused, captured, escape) != (focused && !captured && escape) {
			t.Fatalf("menu focus/capture/Escape mask %d", mask)
		}
		if Menu_MenuContextShouldSuppressClose(focused, captured, escape) != (focused || captured && escape) {
			t.Fatalf("menu opened/trigger/release mask %d", mask)
		}
	}
	for mask := 0; mask < 16; mask++ {
		down, up, right, left := mask&1 != 0, mask&2 != 0, mask&4 != 0, mask&8 != 0
		want := Collapsible_CollapsibleKeyNone()
		switch {
		case down:
			want = Collapsible_CollapsibleKeyDown()
		case up:
			want = Collapsible_CollapsibleKeyUp()
		case right:
			want = Collapsible_CollapsibleKeyRight()
		case left:
			want = Collapsible_CollapsibleKeyLeft()
		}
		if Collapsible_CollapsibleKeyFor(down, up, right, left) != want {
			t.Fatalf("collapsible arrow priority mask %d", mask)
		}
		for _, modifier := range []bool{false, true} {
			got := TextInput_TextShortcutInputFor(modifier, down, up, right, left)
			if got.SelectAll != (modifier && down) || got.Copy != (modifier && up) ||
				got.Cut != (modifier && right) || got.Paste != (modifier && left) {
				t.Fatalf("text shortcut mask %d, modifier %t: %+v", mask, modifier, got)
			}
		}
	}
}
