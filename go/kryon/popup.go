package kryon

type popupScope struct {
	id            int32
	open          *bool
	paint         paintLayerToken
	input         popupInputToken
	capturesInput bool
}

func (r *runtime) PopupScope(p PopupProps) bool {
	decision := PopupPolicy_PopupDecisionFor(uint32(p.Flags), p.Disabled)
	if !decision.Valid {
		panic("unsupported popup flags")
	}
	if !PopupPolicy_PopupCanBegin(decision, p.ID, p.Bounds, p.Trigger, p.Open != nil) {
		return false
	}
	if decision.Context && !p.Disabled && r.mouseReleased[MouseButtonRight] {
		if r.pointerCanReach(p.Trigger) {
			openResult := PopupPolicy_PopupOpenFor(*p.Open, true, false, p.Open != nil)
			*p.Open = openResult.Open
			r.mouseReleased[MouseButtonRight] = false
		}
	}
	if decision.Tooltip {
		if p.Disabled || !pointInRect(r.mousePos.X, r.mousePos.Y, p.Trigger) {
			return false
		}
		r.tooltipPopupsSeen[p.ID] = true
		open := true
		p.Open = &open
	}
	if !decision.Tooltip {
		if r.popupsSeen == nil {
			r.popupsSeen = make(map[int32]bool)
		}
		if r.openPopups == nil {
			r.openPopups = make(map[int32]*bool)
		}
		r.popupsSeen[p.ID] = true
		r.openPopups[p.ID] = p.Open
		*p.Open = PopupPolicy_PopupOpenAfterDisabled(decision, *p.Open, p.Disabled)
		if !*p.Open {
			delete(r.openPopups, p.ID)
			r.closePopupInput(p.ID)
			return false
		}
		for i := range r.taps {
			if decision.Modal {
				break
			}
			if !r.taps[i].consumed && !pointInRect(r.taps[i].x, r.taps[i].y, p.Bounds) {
				r.taps[i].consumed = true
				openResult := PopupPolicy_PopupOpenFor(*p.Open, false, true, true)
				*p.Open = openResult.Open
				r.closePopupInput(p.ID)
				delete(r.openPopups, p.ID)
				return false
			}
		}
	}
	var input popupInputToken
	if decision.CapturesInput {
		inputBounds := PopupPolicy_PopupInputBounds(decision, p.Bounds, float32(r.GetScreenWidth()), float32(r.GetScreenHeight()))
		input = r.beginPopupInput(p.ID, inputBounds)
		if r.keyDown[KeyEscape] && !r.popupKeyboardCaptures() {
			openResult := PopupPolicy_PopupOpenFor(*p.Open, false, true, true)
			*p.Open = openResult.Open
			r.closePopupInput(p.ID)
			r.endPopupInput(input)
			delete(r.openPopups, p.ID)
			return false
		}
	}
	paint := r.beginPaintLayer(p.ID)
	if alpha := PopupPolicy_PopupBackdropAlpha(decision); alpha > 0 {
		r.record(FrameOp{Kind: FrameOpRect, Bounds: NewRectangle(0, 0, float32(r.GetScreenWidth()), float32(r.GetScreenHeight())), Color: Color{A: uint8(alpha)}})
	}
	r.scrollClips = append(r.scrollClips, p.Bounds)
	panelFrame := simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal,
		false, false, p.ClassName, StyleSheet_StyleKindPopup(), PopupPolicy_PopupPanelRole())
	panelOp := styleFrameRectOp(p.Bounds, Rectangle{}, panelFrame)
	panelOp.ID = p.ID
	r.record(panelOp)
	r.popupScopes = append(r.popupScopes, popupScope{id: p.ID, open: p.Open, paint: paint, input: input, capturesInput: decision.CapturesInput})
	return true
}

func (r *runtime) popupCloseScope() {
	if len(r.popupScopes) == 0 {
		panic("popupCloseScope without PopupScope")
	}
	s := &r.popupScopes[len(r.popupScopes)-1]
	openResult := PopupPolicy_PopupOpenFor(*s.open, false, true, true)
	*s.open = openResult.Open
	if s.capturesInput {
		r.closePopupInput(s.id)
		delete(r.openPopups, s.id)
	}
}

func (r *runtime) PopupEndScope() {
	n := len(r.popupScopes)
	if n == 0 {
		panic("PopupEndScope without PopupScope")
	}
	s := r.popupScopes[n-1]
	if s.capturesInput && !*s.open {
		r.closePopupInput(s.id)
		delete(r.openPopups, s.id)
	}
	if len(r.scrollClips) == 0 {
		panic("popup clip was unbalanced")
	}
	r.scrollClips = r.scrollClips[:len(r.scrollClips)-1]
	if s.capturesInput {
		r.endPopupInput(s.input)
	}
	r.endPaintLayer(s.paint)
	r.popupScopes[n-1] = popupScope{}
	r.popupScopes = r.popupScopes[:n-1]
}
