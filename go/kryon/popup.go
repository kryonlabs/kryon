package kryon

type popupScope struct {
	id    int32
	open  *bool
	paint paintLayerToken
	input popupInputToken
	state PopupLifecycle
}

// Apply policy output to host-owned state. Paint/input tokens stay native;
// opening, dismissal and visibility are generated from popup_policy.kry.
func (r *runtime) applyPopupState(id int32, open *bool, state PopupLifecycle) {
	if state.Eligible && !state.Decision.Tooltip && open != nil {
		*open = state.Open
	}
	if state.CloseInput {
		r.closePopupInput(id)
		delete(r.openPopups, id)
	}
	if !state.Visible {
		delete(r.tooltipPopupsSeen, id)
	}
}

func (r *runtime) PopupScope(p PopupProps) bool {
	open := p.Open != nil && *p.Open
	state := PopupPolicy_PopupLifecycleBegin(uint32(p.Flags), PopupFrameInput{
		ID: p.ID, Bounds: p.Bounds, Trigger: p.Trigger,
		HasOpen: p.Open != nil, Open: open, Disabled: p.Disabled,
		TriggerBlocked: !r.pointerCanReach(p.Trigger), Mouse: r.mousePos,
		RightReleased: r.mouseReleased[MouseButtonRight],
		ViewWidth:     float32(r.GetScreenWidth()), ViewHeight: float32(r.GetScreenHeight()),
	})
	if !state.Decision.Valid {
		panic("unsupported popup flags")
	}
	if !state.Eligible {
		return false
	}
	if state.ContextOpened {
		r.mouseReleased[MouseButtonRight] = false
	}
	for i := range r.taps {
		state = PopupPolicy_PopupLifecycleRelease(state, true, r.taps[i].consumed,
			pointInRect(r.taps[i].x, r.taps[i].y, p.Bounds))
		if state.ConsumeRelease {
			r.taps[i].consumed = true
		}
	}
	r.applyPopupState(p.ID, p.Open, state)
	if !state.Visible {
		return false
	}
	var input popupInputToken
	if state.Decision.CapturesInput {
		input = r.beginPopupInput(p.ID, state.InputBounds)
	}
	state = PopupPolicy_PopupLifecycleKeyboard(state, r.keyDown[KeyEscape], r.popupKeyboardCaptures())
	r.applyPopupState(p.ID, p.Open, state)
	if !state.Visible {
		if state.Decision.CapturesInput {
			r.endPopupInput(input)
		}
		return false
	}
	if state.Decision.Tooltip {
		r.tooltipPopupsSeen[p.ID] = true
		open = state.Open
		p.Open = &open
	} else {
		if r.popupsSeen == nil {
			r.popupsSeen = make(map[int32]bool)
		}
		if r.openPopups == nil {
			r.openPopups = make(map[int32]*bool)
		}
		r.popupsSeen[p.ID] = true
		r.openPopups[p.ID] = p.Open
	}
	paint := r.beginPaintLayer(p.ID)
	if alpha := state.BackdropAlpha; alpha > 0 {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: NewRectangle(0, 0, float32(r.GetScreenWidth()), float32(r.GetScreenHeight())), Color: Color{A: uint8(alpha)}})
	}
	r.scrollClips = append(r.scrollClips, p.Bounds)
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, p.ClassName, StyleSheet_StyleKindPopup(), PopupPolicy_PopupPanelRole())
	panelOp := styleFrameRectOp(p.Bounds, Rectangle{}, panelFrame)
	panelOp.ID = p.ID
	r.record(panelOp)
	r.popupScopes = append(r.popupScopes, popupScope{id: p.ID, open: p.Open, paint: paint, input: input, state: state})
	return true
}

func (r *runtime) popupCloseScope() {
	if len(r.popupScopes) == 0 {
		panic("popupCloseScope without PopupScope")
	}
	s := &r.popupScopes[len(r.popupScopes)-1]
	s.state = PopupPolicy_PopupLifecycleFinish(s.state, *s.open, true)
	*s.open = s.state.Open
	r.applyPopupState(s.id, s.open, s.state)
}

func (r *runtime) PopupEndScope() {
	n := len(r.popupScopes)
	if n == 0 {
		panic("PopupEndScope without PopupScope")
	}
	s := r.popupScopes[n-1]
	s.state = PopupPolicy_PopupLifecycleFinish(s.state, *s.open, false)
	r.applyPopupState(s.id, s.open, s.state)
	if len(r.scrollClips) == 0 {
		panic("popup clip was unbalanced")
	}
	r.scrollClips = r.scrollClips[:len(r.scrollClips)-1]
	if s.state.Decision.CapturesInput {
		r.endPopupInput(s.input)
	}
	r.endPaintLayer(s.paint)
	r.popupScopes[n-1] = popupScope{}
	r.popupScopes = r.popupScopes[:n-1]
}
