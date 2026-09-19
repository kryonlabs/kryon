package kryon

import "testing"

func accessibilityNode(t *testing.T, r *runtime, id int32) AccessibilityNode {
	t.Helper()
	for _, node := range r.GetAccessibilitySnapshot() {
		if node.FocusID == id {
			return node
		}
	}
	t.Fatalf("missing accessibility target %d", id)
	return AccessibilityNode{}
}

func TestAccessibilityActionsDeliverOnce(t *testing.T) {
	r := New(AppConfig{Width: 400, Height: 300}).(*runtime)
	flags, toggle, clicks := int32(6), int32(0), 0
	text := make([]byte, 32)
	copy(text, "private")
	cursor := int32(7)
	draw := func() {
		r.BeginFrame()
		if r.Button(ButtonProps{ID: 11, Label: "Run", Bounds: NewRectangle(0, 0, 100, 30)}) {
			clicks++
		}
		r.Checkbox(CheckboxProps{ID: 12, Flags: &flags, FlagsValue: 2, Bounds: NewRectangle(0, 40, 100, 30)})
		r.Toggle(ToggleProps{ID: 13, Value: &toggle, Bounds: NewRectangle(0, 80, 100, 30)})
		r.TextField(TextFieldProps{FocusID: 14, Text: text, CursorPosition: &cursor, Secure: true, ReadOnly: true, Bounds: NewRectangle(0, 120, 100, 30)})
		r.EndFrame()
	}
	draw()
	for _, id := range []int32{11, 12, 13} {
		node := accessibilityNode(t, r, id)
		if node.Actions != uint32(AccessibilityActionFocus|AccessibilityActionActivate) {
			t.Fatalf("missing actions: %+v", node)
		}
		for repeat := 0; repeat < 2; repeat++ {
			if !r.QueueAccessibilityAction(id, node.Generation, AccessibilityActionActivate) {
				t.Fatal("activation rejected")
			}
		}
	}
	field := accessibilityNode(t, r, 14)
	if field.Actions != uint32(AccessibilityActionFocus) || field.Value != "" {
		t.Fatalf("secure editor capabilities: %+v", field)
	}
	if r.QueueAccessibilityAction(14, field.Generation, AccessibilityActionActivate) {
		t.Fatal("editor advertised an activation")
	}
	if !r.QueueAccessibilityAction(14, field.Generation, AccessibilityActionFocus) {
		t.Fatal("read-only editor focus rejected")
	}
	draw()
	if clicks != 1 || flags != 4 || toggle != 1 || r.Focus() != 14 {
		t.Fatalf("actions not delivered: clicks=%d flags=%d toggle=%d focus=%d", clicks, flags, toggle, r.Focus())
	}
	draw()
	if clicks != 1 || flags != 4 || toggle != 1 {
		t.Fatal("action replayed")
	}
	if r.QueueAccessibilityAction(11, field.Generation, AccessibilityActionActivate) {
		t.Fatal("stale snapshot accepted")
	}
}

func TestAccessibilityActionRevalidation(t *testing.T) {
	for _, change := range []string{"disabled", "loading", "state_disabled", "state_loading", "scope", "removed", "kind", "popup"} {
		t.Run(change, func(t *testing.T) {
			r := New(AppConfig{Width: 200, Height: 100}).(*runtime)
			changed, clicks := false, 0
			value := int32(0)
			draw := func() {
				r.BeginFrame()
				var token popupInputToken
				if changed && change == "popup" {
					token = r.beginPopupInput(99, NewRectangle(0, 0, 200, 100))
					r.endPopupInput(token)
				}
				r.DisabledScope(changed && change == "scope")
				if changed && change == "kind" {
					r.Checkbox(CheckboxProps{ID: 11, Value: &value, Bounds: NewRectangle(0, 0, 100, 30)})
				} else if !changed || change != "removed" {
					state := ButtonStateAuto
					if changed && change == "state_disabled" {
						state = ButtonStateDisabled
					} else if changed && change == "state_loading" {
						state = ButtonStateLoading
					}
					if r.Button(ButtonProps{ID: 11, State: state, Disabled: changed && change == "disabled", Loading: changed && change == "loading", Bounds: NewRectangle(0, 0, 100, 30)}) {
						clicks++
					}
				}
				r.DisabledEndScope()
				r.EndFrame()
			}
			draw()
			node := accessibilityNode(t, r, 11)
			if !r.QueueAccessibilityAction(11, node.Generation, AccessibilityActionActivate) {
				t.Fatal("initial request rejected")
			}
			changed = true
			draw()
			if clicks != 0 || value != 0 {
				t.Fatal("ineligible replacement received the action")
			}
			if change != "removed" && change != "kind" {
				node = accessibilityNode(t, r, 11)
				if node.Actions != 0 || r.QueueAccessibilityAction(11, node.Generation, AccessibilityActionFocus) {
					t.Fatal("ineligible control advertised actions")
				}
			}
			r.closePopupInput(99)
			changed = false
			draw()
			if clicks != 0 {
				t.Fatal("discarded action replayed on reappearance")
			}
		})
	}
}

func TestAccessibilityActionQueueBoundsAndValidation(t *testing.T) {
	r := New(AppConfig{Width: 100, Height: 100}).(*runtime)
	r.BeginFrame()
	for id := int32(1); id <= 33; id++ {
		r.Button(ButtonProps{ID: id, Bounds: NewRectangle(0, 0, 10, 10)})
	}
	r.EndFrame()
	generation := accessibilityNode(t, r, 1).Generation
	for _, id := range []int32{-1, 0, 100} {
		if r.QueueAccessibilityAction(id, generation, AccessibilityActionActivate) {
			t.Fatalf("invalid target %d accepted", id)
		}
	}
	for _, action := range []AccessibilityAction{0, 3, 99} {
		if r.QueueAccessibilityAction(1, generation, action) {
			t.Fatalf("invalid action %d accepted", action)
		}
	}
	for id := int32(1); id <= 32; id++ {
		if !r.QueueAccessibilityAction(id, generation, AccessibilityActionFocus) {
			t.Fatalf("queue filled early at %d", id)
		}
	}
	if r.QueueAccessibilityAction(33, generation, AccessibilityActionFocus) {
		t.Fatal("unbounded action queue")
	}
	r.BeginFrame()
	if r.QueueAccessibilityAction(1, generation, AccessibilityActionFocus) {
		t.Fatal("mid-frame request accepted")
	}
	r.Button(ButtonProps{ID: 1})
	r.Button(ButtonProps{ID: 1})
	r.EndFrame()
	if r.QueueAccessibilityAction(1, uint64(r.frames), AccessibilityActionFocus) {
		t.Fatal("ambiguous target accepted")
	}
}

func TestAccessibilitySinkCanQueueNextFrame(t *testing.T) {
	host := NewHost(AppConfig{Width: 100, Height: 100})
	defer host.Close()
	queued, clicks := false, 0
	host.SetAccessibilitySink(func(nodes []AccessibilityNode) {
		if queued {
			return
		}
		queued = host.QueueAccessibilityAction(nodes[0].FocusID, nodes[0].Generation, AccessibilityActionActivate)
	})
	for i := 0; i < 3; i++ {
		host.Draw(func() {
			BeginFrame()
			if Button(ButtonProps{ID: 11}) {
				clicks++
			}
			EndFrame()
		})
	}
	if !queued || clicks != 1 {
		t.Fatalf("sink queue delivery: queued=%v clicks=%d", queued, clicks)
	}
}

func TestAccessibilityButtonVariants(t *testing.T) {
	for _, variant := range []string{"info", "card", "composed"} {
		t.Run(variant, func(t *testing.T) {
			r := New(AppConfig{Width: 100, Height: 100}).(*runtime)
			clicks := 0
			draw := func() {
				r.BeginFrame()
				props := ButtonProps{ID: 41, Bounds: NewRectangle(0, 0, 90, 30)}
				switch variant {
				case "info":
					props.Info = true
					if r.Button(props) {
						clicks++
					}
				case "card":
					if r.Card(CardProps{ID: 41, Clickable: true, Bounds: props.Bounds}) {
						clicks++
					}
				case "composed":
					r.ButtonScope(props)
					r.Text(TextProps{Text: "Composed"})
					r.End()
				}
				r.EndFrame()
			}
			draw()
			node := accessibilityNode(t, r, 41)
			if !r.QueueAccessibilityAction(41, node.Generation, AccessibilityActionActivate) {
				t.Fatal("variant activation rejected")
			}
			draw()
			if !accessibilityNode(t, r, 41).Focused {
				t.Fatal("variant did not receive focus")
			}
			if variant == "composed" {
				if !r.ops[0].Pressed {
					t.Fatal("composed variant did not activate")
				}
			} else if clicks != 1 {
				t.Fatalf("variant activations = %d", clicks)
			}
		})
	}
}

func TestAccessibilityPassiveCardRejectsActions(t *testing.T) {
	for _, composed := range []bool{false, true} {
		r := New(AppConfig{Width: 100, Height: 100}).(*runtime)
		draw := func(clickable bool) {
			r.BeginFrame()
			props := CardProps{ID: 41, Clickable: clickable, Bounds: NewRectangle(0, 0, 90, 30)}
			if composed {
				r.CardScope(props)
				r.End()
			} else {
				r.Card(props)
			}
			r.EndFrame()
		}
		draw(true)
		node := accessibilityNode(t, r, 41)
		if !r.QueueAccessibilityAction(41, node.Generation, AccessibilityActionActivate) {
			t.Fatal("clickable card rejected activation")
		}
		draw(false)
		if r.Focus() == 41 || r.ops[0].Pressed {
			t.Fatal("passive card received queued activation")
		}
		if r.QueueAccessibilityAction(41, uint64(r.frames), AccessibilityActionActivate) {
			t.Fatal("passive card accepted activation")
		}
		draw(true)
		if r.ops[0].Pressed {
			t.Fatal("passive card request replayed")
		}
	}
}
