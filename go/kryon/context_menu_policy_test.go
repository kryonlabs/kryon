package kryon

import "testing"

func TestContextMenuRespectsPopupCaptureAndDisabledScope(t *testing.T) {
	for _, scenario := range []string{"modal", "disabled", "clipped", "available"} {
		t.Run(scenario, func(t *testing.T) {
			r := New(AppConfig{Width: 400, Height: 300}).(*runtime)
			open, x, y := int32(0), int32(0), int32(0)
			modalOpen := true
			props := MenuProps{ID: 410, Mode: MenuModeContext,
				Trigger: NewRectangle(10, 10, 100, 50), Open: &open, X: &x, Y: &y,
				Items: []MenuItem{{Kind: MenuCommand, Label: "Open", ID: 11}}, ItemCount: 1}
			for frame := 0; frame < 2; frame++ {
				if frame == 1 {
					r.QueueMouseButtonUp(MouseButtonRight, 30, 25)
				}
				r.BeginFrame()
				if scenario == "modal" {
					if !r.PopupScope(PopupProps{ID: 420, Bounds: NewRectangle(150, 100, 200, 150),
						Open: &modalOpen, Flags: PopupModal}) {
						t.Fatal("modal did not open")
					}
					r.PopupEndScope()
				}
				if scenario == "disabled" {
					r.DisabledScope(true)
				}
				if scenario == "clipped" {
					r.scrollClips = append(r.scrollClips, NewRectangle(60, 10, 50, 50))
				}
				r.Menu(props)
				if scenario == "clipped" {
					r.scrollClips = r.scrollClips[:len(r.scrollClips)-1]
				}
				if scenario == "disabled" {
					r.DisabledEndScope()
				}
				r.EndFrame()
			}
			if got, want := open != 0, scenario == "available"; got != want {
				t.Fatalf("context menu open=%v, want %v", got, want)
			}
		})
	}
}

func TestContextMenuSharedDismissalAndCallerClose(t *testing.T) {
	r := New(AppConfig{Width: 400, Height: 300}).(*runtime)
	open, x, y := int32(1), int32(200), int32(100)
	props := MenuProps{ID: 430, Mode: MenuModeContext,
		Trigger: NewRectangle(10, 10, 100, 50), Open: &open, X: &x, Y: &y,
		Items: []MenuItem{{Kind: MenuCommand, Label: "Open", ID: 11}}, ItemCount: 1}
	r.BeginFrame()
	r.Menu(props)
	r.EndFrame()
	r.QueueTap(30, 25)
	r.BeginFrame()
	r.Menu(props)
	r.EndFrame()
	if open != 1 {
		t.Fatal("trigger release dismissed the context menu")
	}
	r.QueueTap(350, 250)
	r.BeginFrame()
	r.Menu(props)
	activated := r.Button(ButtonProps{ID: 431, Label: "Behind", Bounds: NewRectangle(330, 230, 60, 50)})
	r.EndFrame()
	if open != 0 || activated {
		t.Fatalf("outside release open=%d, background activation=%v", open, activated)
	}
	open = 1
	r.BeginFrame()
	r.Menu(props)
	r.EndFrame()
	open = 0
	r.BeginFrame()
	r.Menu(props)
	r.EndFrame()
	if _, exists := r.contextMenus[props.ID]; exists {
		t.Fatal("caller closed the menu but its retained popup remained open")
	}
}
