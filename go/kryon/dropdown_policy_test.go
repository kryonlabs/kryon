package kryon

import "testing"

func TestDropdownNavigationAvailability(t *testing.T) {
	for mask := 0; mask < 16; mask++ {
		for current := int32(-1); current < 5; current++ {
			for key := 0; key < 5; key++ {
				nav := Dropdown_StartNavigation(current, 4, key == 1, key == 2, key == 3, key == 4)
				for steps := 0; nav.Searching; steps++ {
					if steps > 9 || nav.Index < 0 || nav.Index >= 4 {
						t.Fatalf("unbounded navigation: %+v", nav)
					}
					nav = Dropdown_ScanNavigation(nav, mask&(1<<nav.Index) != 0)
				}
				if mask == 0 && nav.Result != -1 || mask != 0 && (nav.Result < 0 || mask&(1<<nav.Result) == 0) {
					t.Fatalf("mask %d selected unavailable row: %+v", mask, nav)
				}
			}
		}
	}
}

func TestDropdownPolicyLimitsAndActivation(t *testing.T) {
	if Dropdown_ContentHeight(2147483647, 44, 8) != 2147483647 {
		t.Fatal("content height overflowed")
	}
	panel := Dropdown_PopupBounds(Rectangle{X: 50, Y: 90, Width: 400, Height: 44}, Rectangle{Width: 20, Height: 10}, 100, 1)
	if panel.X != 0 || panel.Y != 0 || panel.Width != 20 || panel.Height != 10 {
		t.Fatalf("tiny viewport: %+v", panel)
	}
	if Dropdown_WheelOffset(20, -1e30, 44, 100) != 100 || Dropdown_WheelOffset(20, 1e30, 44, 100) != 0 {
		t.Fatal("wheel did not saturate")
	}
	if Dropdown_CanCommit(true, true, true, true, true, false, false, true) || Dropdown_CanCommit(false, false, true, true, true, false, false, true) {
		t.Fatal("opening or disabled row committed")
	}
	if Dropdown_CanCommit(true, false, false, false, true, false, true, false) || !Dropdown_CanCommit(true, false, false, false, true, false, true, true) {
		t.Fatal("drag suppression lost returned-click behavior")
	}
}
