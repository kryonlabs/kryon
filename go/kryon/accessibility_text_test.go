package kryon

import (
	"bytes"
	"strings"
	"testing"
)

func TestAccessibilityTextValueValidation(t *testing.T) {
	for _, tc := range []struct {
		name, value string
		capacity    int
		limit       int32
		multiline   bool
		queued      bool
		applied     bool
	}{
		{name: "replace", value: "hello", capacity: 32, queued: true, applied: true},
		{name: "empty", capacity: 32, queued: true, applied: true},
		{name: "same", value: "old", capacity: 32, queued: true, applied: true},
		{name: "scalar limit", value: "\u00e9\u754c", capacity: 32, limit: 2, queued: true, applied: true},
		{name: "scalar overflow", value: "\u00e9\u754cx", capacity: 32, limit: 2, queued: true},
		{name: "byte overflow", value: "\u00e9\u754c", capacity: 5, queued: true},
		{name: "exact fit", value: "\u00e9\u754c", capacity: 6, queued: true, applied: true},
		{name: "multiline", value: "a\r\nb\tc", capacity: 32, multiline: true, queued: true, applied: true},
		{name: "single newline", value: "a\nb", capacity: 32},
		{name: "single tab", value: "a\tb", capacity: 32},
		{name: "control", value: "a\x7fb", capacity: 32, multiline: true},
		{name: "nul", value: "a\x00b", capacity: 32},
		{name: "invalid utf8", value: "\xc0\xaf", capacity: 32},
		{name: "oversized", value: strings.Repeat("x", 65537), capacity: 32},
	} {
		t.Run(tc.name, func(t *testing.T) {
			r := New(AppConfig{Width: 200, Height: 100}).(*runtime)
			defer r.Close()
			buf := make([]byte, tc.capacity)
			copy(buf, "old")
			cursor := int32(3)
			draw := func() bool {
				r.BeginFrame()
				changed := false
				if tc.multiline {
					changed = r.TextArea(TextAreaProps{FocusID: 11, Text: buf, CursorPosition: &cursor, MaxCodepoints: tc.limit})
				} else {
					r.TextField(TextFieldProps{FocusID: 11, Text: buf, CursorPosition: &cursor, MaxCodepoints: tc.limit})
				}
				r.EndFrame()
				return changed
			}
			draw()
			node := accessibilityNode(t, r, 11)
			if node.Actions != uint32(AccessibilityActionFocus|AccessibilityActionSetValue|AccessibilityActionSetSelection) {
				t.Fatalf("editor capabilities: %+v", node)
			}
			if r.QueueAccessibilityValue(11, node.Generation, tc.value) != tc.queued {
				t.Fatal("unexpected queue result")
			}
			changed := draw()
			want := "old"
			if tc.applied {
				want = tc.value
			}
			if got := string(buf[:zeroIndex(buf)]); got != want {
				t.Fatalf("value = %q, want %q", got, want)
			}
			if tc.multiline && changed != (tc.applied && tc.value != "old") {
				t.Fatal("wrong TextArea changed result")
			}
			if tc.applied && (cursor != int32(len(want)) || r.Focus() != 11) {
				t.Fatal("replacement did not collapse selection and focus editor")
			}
			if draw() {
				t.Fatal("replacement replayed")
			}
		})
	}
}

func TestAccessibilityTextSelectionAndComposition(t *testing.T) {
	for _, readOnly := range []bool{false, true} {
		r := New(AppConfig{Width: 200, Height: 100}).(*runtime)
		buf := make([]byte, 128)
		copy(buf, "Ae\u0301\r\nB\U0001f469\u200d\U0001f4bbZ")
		cursor := int32(0)
		draw := func() bool {
			r.BeginFrame()
			changed := r.TextArea(TextAreaProps{FocusID: 11, Text: buf, CursorPosition: &cursor, ReadOnly: readOnly})
			r.EndFrame()
			return changed
		}
		draw()
		for _, tc := range []struct{ anchor, cursor, wantAnchor, wantCursor int32 }{
			{3, 12, 1, 7}, {17, 5, 7, 4}, {-9, 999, 0, 19}, {999, -9, 19, 0},
		} {
			node := accessibilityNode(t, r, 11)
			if !r.QueueAccessibilitySelection(11, node.Generation, tc.anchor, tc.cursor) {
				t.Fatal("selection rejected")
			}
			if draw() {
				t.Fatal("selection reported a text change")
			}
			node = accessibilityNode(t, r, 11)
			if node.SelectionAnchor != tc.wantAnchor || node.SelectionCursor != tc.wantCursor || cursor != tc.wantCursor {
				t.Fatalf("selection = %d:%d, want %d:%d", node.SelectionAnchor, node.SelectionCursor, tc.wantAnchor, tc.wantCursor)
			}
		}
		if readOnly {
			if r.QueueAccessibilityValue(11, uint64(r.frames), "blocked") {
				t.Fatal("read-only field accepted replacement")
			}
		} else {
			r.SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "pending", 7, 0)
			draw()
			node := accessibilityNode(t, r, 11)
			if strings.Contains(node.Value, "pending") || node.SelectionAnchor != 19 || node.SelectionCursor != 0 {
				t.Fatalf("preedit leaked into committed snapshot: %+v", node)
			}
			r.SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT, "stale", 5, 0)
			if !r.QueueAccessibilityValue(11, node.Generation, "new") {
				t.Fatal("replacement rejected")
			}
			if !draw() || string(buf[:zeroIndex(buf)]) != "new" || len(r.preedit) != 0 {
				t.Fatal("replacement did not cancel composition")
			}
			r.QueueText("!")
			if !draw() || string(buf[:zeroIndex(buf)]) != "new!" {
				t.Fatal("normal editing after replacement failed")
			}
		}
		r.Close()
	}
}

func TestAccessibilityTextRevalidation(t *testing.T) {
	for _, change := range []string{"read_only", "disabled", "popup", "removed", "kind", "capacity", "limit"} {
		t.Run(change, func(t *testing.T) {
			r := New(AppConfig{Width: 200, Height: 100}).(*runtime)
			defer r.Close()
			buf := make([]byte, 32)
			copy(buf, "old")
			cursor := int32(3)
			changed := false
			draw := func() {
				r.BeginFrame()
				if changed && change == "popup" {
					token := r.beginPopupInput(99, NewRectangle(0, 0, 200, 100))
					r.endPopupInput(token)
				}
				r.DisabledScope(changed && change == "disabled")
				text, limit := buf, int32(0)
				if changed && change == "capacity" {
					text = buf[:4]
				}
				if changed && change == "limit" {
					limit = 3
				}
				if changed && change == "kind" {
					r.TextArea(TextAreaProps{FocusID: 11, Text: text, CursorPosition: &cursor})
				} else if !changed || change != "removed" {
					r.TextField(TextFieldProps{FocusID: 11, Text: text, CursorPosition: &cursor, ReadOnly: changed && change == "read_only", MaxCodepoints: limit})
				}
				r.DisabledEndScope()
				r.EndFrame()
			}
			draw()
			if !r.QueueAccessibilityValue(11, uint64(r.frames), "replacement") {
				t.Fatal("initial request rejected")
			}
			payload := r.accessibility.pending[0].value
			changed = true
			draw()
			if string(buf[:zeroIndex(buf)]) != "old" || r.Focus() == 11 || !bytes.Equal(payload, make([]byte, len(payload))) {
				t.Fatal("invalid replacement applied or payload retained")
			}
			r.closePopupInput(99)
			changed = false
			draw()
			if string(buf[:zeroIndex(buf)]) != "old" {
				t.Fatal("discarded replacement replayed")
			}
		})
	}
}

func TestAccessibilityTextCoalescingAndSecureCleanup(t *testing.T) {
	r := New(AppConfig{Width: 200, Height: 100}).(*runtime)
	buf := make([]byte, 64)
	cursor := int32(0)
	draw := func() {
		r.BeginFrame()
		r.TextField(TextFieldProps{FocusID: 11, Text: buf, CursorPosition: &cursor, Secure: true})
		r.EndFrame()
	}
	draw()
	generation := uint64(r.frames)
	if !r.QueueAccessibilityValue(11, generation, "secret") || !r.QueueAccessibilitySelection(11, generation, 0, 2) {
		t.Fatal("initial requests rejected")
	}
	old := r.accessibility.pending[0].value
	if !r.QueueAccessibilityValue(11, generation, "last") || len(r.accessibility.pending) != 2 {
		t.Fatal("replacement did not coalesce")
	}
	if !bytes.Equal(old, make([]byte, len(old))) {
		t.Fatal("superseded secret payload retained")
	}
	last := r.accessibility.pending[1].value
	draw()
	node := accessibilityNode(t, r, 11)
	if string(buf[:zeroIndex(buf)]) != "last" || cursor != 4 || node.Value != "" || node.SelectionAnchor != 0 || node.SelectionCursor != 0 {
		t.Fatalf("ordered secure replacement: %+v cursor=%d", node, cursor)
	}
	if !bytes.Equal(last, make([]byte, len(last))) {
		t.Fatal("delivered secret payload retained")
	}
	if r.QueueAccessibilityAction(11, node.Generation, AccessibilityActionSetValue) || r.QueueAccessibilityAction(11, node.Generation, AccessibilityActionSetSelection) {
		t.Fatal("payload-less action accepted")
	}
	if !r.QueueAccessibilityValue(11, node.Generation, "close") {
		t.Fatal("final request rejected")
	}
	last = r.accessibility.pending[0].value
	r.Close()
	if !bytes.Equal(last, make([]byte, len(last))) || r.QueueAccessibilityValue(11, node.Generation, "closed") {
		t.Fatal("closed runtime retained or accepted payload")
	}
}

func TestAccessibilityTextHostAndPackage(t *testing.T) {
	host := NewHost(AppConfig{Width: 200, Height: 100})
	defer host.Close()
	buf := make([]byte, 32)
	cursor := int32(0)
	draw := func() {
		host.Draw(func() {
			BeginFrame()
			TextField(TextFieldProps{FocusID: 11, Text: buf, CursorPosition: &cursor})
			EndFrame()
		})
	}
	draw()
	node := host.GetAccessibilitySnapshot()[0]
	if !host.QueueAccessibilityValue(11, node.Generation, "first") || !host.QueueAccessibilitySelection(11, node.Generation, 1, 3) {
		t.Fatal("host queue rejected request")
	}
	draw()
	node = host.GetAccessibilitySnapshot()[0]
	if node.Value != "first" || node.SelectionAnchor != 1 || node.SelectionCursor != 3 {
		t.Fatalf("host request not delivered: %+v", node)
	}
	host.Draw(func() {
		if !QueueAccessibilityValue(11, node.Generation, "second") || !QueueAccessibilitySelection(11, node.Generation, 2, 5) {
			t.Fatal("package queue rejected request")
		}
	})
	draw()
	node = host.GetAccessibilitySnapshot()[0]
	if node.Value != "second" || node.SelectionAnchor != 2 || node.SelectionCursor != 5 {
		t.Fatalf("package request not delivered: %+v", node)
	}
	if (*Host)(nil).QueueAccessibilityValue(11, node.Generation, "nil") || (*Host)(nil).QueueAccessibilitySelection(11, node.Generation, 0, 1) {
		t.Fatal("nil host accepted request")
	}
}

func TestAccessibilityTextQueueCapacity(t *testing.T) {
	r := New(AppConfig{Width: 100, Height: 100}).(*runtime)
	defer r.Close()
	r.BeginFrame()
	for id := int32(1); id <= 33; id++ {
		r.TextField(TextFieldProps{FocusID: id})
	}
	r.EndFrame()
	generation := uint64(r.frames)
	for id := int32(1); id <= 32; id++ {
		if !r.QueueAccessibilityValue(id, generation, "value") {
			t.Fatalf("value queue filled early at %d", id)
		}
	}
	if r.QueueAccessibilityValue(33, generation, "overflow") || r.QueueAccessibilitySelection(1, generation, 0, 1) {
		t.Fatal("unbounded mixed request queue")
	}
	if !r.QueueAccessibilityValue(1, generation, strings.Repeat("x", 65536)) {
		t.Fatal("maximum-sized replacement rejected when coalescing full queue")
	}
	payload := r.accessibility.pending[31].value
	r.BeginFrame()
	if r.QueueAccessibilityValue(1, generation, "mid-frame") || r.QueueAccessibilitySelection(1, generation, 0, 1) {
		t.Fatal("mid-frame text request accepted")
	}
	r.EndFrame()
	if !bytes.Equal(payload, make([]byte, len(payload))) {
		t.Fatal("removed target retained payload")
	}
}
