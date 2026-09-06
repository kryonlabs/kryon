package kryon

type comboScope struct {
	id    int32
	open  *bool
	paint paintLayerToken
	input popupInputToken
}

func (r *runtime) comboPopup(p ComboProps) Rectangle {
	row := p.Bounds.Height
	if row <= 0 {
		row = 28
	}
	heights := p.Flags & (ComboHeightSmall | ComboHeightRegular | ComboHeightLarge | ComboHeightLargest)
	if heights != 0 && heights&(heights-1) != 0 {
		panic("combo height flags are mutually exclusive")
	}
	popup := Rectangle{X: p.Bounds.X, Y: p.Bounds.Y + p.Bounds.Height, Width: p.PopupSize.X, Height: p.PopupSize.Y}
	if popup.Width <= 0 {
		popup.Width = p.Bounds.Width
	}
	if popup.Height <= 0 {
		rows := float32(8)
		if p.Flags&ComboHeightSmall != 0 {
			rows = 4
		}
		if p.Flags&ComboHeightLarge != 0 {
			rows = 20
		}
		if p.Flags&ComboHeightLargest != 0 {
			rows = 32
		}
		popup.Height = row * rows
	}
	w, h := float32(r.GetScreenWidth()), float32(r.GetScreenHeight())
	if w > 0 {
		if popup.Width > w {
			popup.Width = w
		}
		if p.Flags&ComboPopupAlignLeft == 0 {
			popup.X = p.Bounds.X + p.Bounds.Width - popup.Width
		}
		popup.X = max(float32(0), min(popup.X, w-popup.Width))
	}
	if h > 0 {
		if popup.Height > h {
			popup.Height = h
		}
		if popup.Y+popup.Height > h {
			popup.Y = p.Bounds.Y - popup.Height
		}
		popup.Y = max(float32(0), popup.Y)
		if popup.Y+popup.Height > h {
			popup.Height = h - popup.Y
		}
	}
	return popup
}

func (r *runtime) BeginCombo(p ComboProps) bool {
	if p.ID <= 0 || p.Open == nil {
		return false
	}
	if r.combosSeen == nil {
		r.combosSeen = make(map[int32]bool)
	}
	if r.openCombos == nil {
		r.openCombos = make(map[int32]*bool)
	}
	r.combosSeen[p.ID] = true
	r.openCombos[p.ID] = p.Open
	popup := r.comboPopup(p)
	if p.Disabled {
		*p.Open = false
	}
	if *p.Open {
		for i := range r.taps {
			if !r.taps[i].consumed && !pointInRect(r.taps[i].x, r.taps[i].y, popup) {
				r.taps[i].consumed = true
				*p.Open = false
				r.closePopupInput(p.ID)
			}
		}
	}
	var input popupInputToken
	hasInput := false
	if *p.Open {
		input, hasInput = r.beginPopupInput(p.ID, popup), true
	}
	trigger := p.Bounds
	preview := p.Preview
	if p.Flags&ComboNoPreview != 0 {
		preview = ""
	}
	if p.Flags&ComboWidthFitPreview != 0 {
		need := float32(runtimeTextWidth(preview, Text16) + 20)
		if p.Flags&ComboNoArrowButton == 0 {
			need += 20
		}
		if need > trigger.Width {
			trigger.Width = need
		}
	}
	label := preview
	if p.Flags&ComboNoArrowButton == 0 {
		label += " v"
	}
	pressed := r.Button(ButtonProps{Bounds: trigger, Label: label, ID: p.ID, Disabled: p.Disabled})
	if pressed {
		*p.Open = !*p.Open
	}
	if !*p.Open {
		if hasInput {
			r.closePopupInput(p.ID)
			r.endPopupInput(input)
		}
		delete(r.openCombos, p.ID)
		return false
	}
	if !hasInput {
		input, hasInput = r.beginPopupInput(p.ID, popup), true
	}
	if r.keyDown[KeyEscape] && !r.popupKeyboardCaptures() {
		*p.Open = false
		r.closePopupInput(p.ID)
		r.endPopupInput(input)
		delete(r.openCombos, p.ID)
		return false
	}
	paint := r.beginPaintLayer(p.ID)
	r.scrollClips = append(r.scrollClips, popup)
	t := r.theme()
	r.record(FrameOp{Kind: FrameOpRect, Bounds: popup, Color: t.surface, BorderColor: t.border, ID: p.ID})
	r.comboScopes = append(r.comboScopes, comboScope{id: p.ID, open: p.Open, paint: paint, input: input})
	return true
}

func (r *runtime) CloseCombo() {
	if len(r.comboScopes) == 0 {
		return
	}
	s := &r.comboScopes[len(r.comboScopes)-1]
	*s.open = false
	r.closePopupInput(s.id)
	delete(r.openCombos, s.id)
}

func (r *runtime) EndCombo() {
	n := len(r.comboScopes)
	if n == 0 {
		panic("EndCombo without BeginCombo")
	}
	s := r.comboScopes[n-1]
	if !*s.open {
		r.closePopupInput(s.id)
		delete(r.openCombos, s.id)
	}
	if len(r.scrollClips) == 0 {
		panic("combo popup clip was unbalanced")
	}
	r.scrollClips = r.scrollClips[:len(r.scrollClips)-1]
	r.endPopupInput(s.input)
	r.endPaintLayer(s.paint)
	r.comboScopes[n-1] = comboScope{}
	r.comboScopes = r.comboScopes[:n-1]
}
