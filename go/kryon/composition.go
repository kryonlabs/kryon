package kryon

import "strings"

// These phases and queue limits match the native C input front-end.
type KryTextCompositionPhase int32

const (
	KRY_TEXT_COMPOSITION_START KryTextCompositionPhase = iota + 1
	KRY_TEXT_COMPOSITION_UPDATE
	KRY_TEXT_COMPOSITION_COMMIT
	KRY_TEXT_COMPOSITION_CANCEL
	KRY_TEXT_COMPOSITION_MAX = 256
)

type KryTextCompositionEvent struct {
	Phase           KryTextCompositionPhase
	Text            string
	Cursor          int32
	SelectionLength int32
}

func (r *runtime) SubmitTextComposition(phase KryTextCompositionPhase, text string, cursor, selectionLength int32) int32 {
	if phase < KRY_TEXT_COMPOSITION_START || phase > KRY_TEXT_COMPOSITION_CANCEL || len(r.compositionEvents) >= 16 {
		return 0
	}
	if end := strings.IndexByte(text, 0); end >= 0 {
		text = text[:end]
	}
	if len(text) >= KRY_TEXT_COMPOSITION_MAX {
		text = text[:KRY_TEXT_COMPOSITION_MAX-1]
	}
	r.compositionEvents = append(r.compositionEvents, KryTextCompositionEvent{phase, text, max(cursor, 0), max(selectionLength, 0)})
	return 1
}

func (r *runtime) PollTextComposition(event *KryTextCompositionEvent) int32 {
	if event == nil || len(r.compositionEvents) == 0 {
		return 0
	}
	*event = r.compositionEvents[0]
	copy(r.compositionEvents, r.compositionEvents[1:])
	r.compositionEvents[len(r.compositionEvents)-1] = KryTextCompositionEvent{}
	r.compositionEvents = r.compositionEvents[:len(r.compositionEvents)-1]
	return 1
}

func (r *runtime) ClearTextComposition() {
	clear(r.compositionEvents)
	r.compositionEvents = r.compositionEvents[:0]
}

func SubmitTextComposition(phase KryTextCompositionPhase, text string, cursor, selectionLength int32) int32 {
	return active().SubmitTextComposition(phase, text, cursor, selectionLength)
}

func PollTextComposition(event *KryTextCompositionEvent) int32 {
	return active().PollTextComposition(event)
}

func ClearTextComposition() { active().ClearTextComposition() }

func (r *runtime) editComposition(id int32, text string, pos int, sel selection, limit int) (string, int, selection, bool) {
	changed := false
	var event KryTextCompositionEvent
	for r.PollTextComposition(&event) != 0 {
		switch event.Phase {
		case KRY_TEXT_COMPOSITION_START, KRY_TEXT_COMPOSITION_UPDATE:
			if r.preedit == nil {
				r.preedit = make(map[int32]KryTextCompositionEvent)
			}
			r.preedit[id] = event
		case KRY_TEXT_COMPOSITION_COMMIT:
			var inserted bool
			text, pos, inserted = insertText(text, pos, sel, event.Text, limit)
			if inserted {
				changed = true
				sel = selection{Anchor: pos, Cursor: pos}
			}
			delete(r.preedit, id)
		case KRY_TEXT_COMPOSITION_CANCEL:
			delete(r.preedit, id)
		}
	}
	return text, pos, sel, changed
}
