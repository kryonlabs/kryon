package kryon

// Popup input ownership is independent of the kind of widgets in the panel.
// Entries persist until dismissal or frame-end owner removal, so background
// widgets declared before the popup cannot consume last frame's popup input.
type popupInputPanel struct {
	bounds      Rectangle
	parent      int32
	hasParent   bool
	seen, order uint64
	alive       bool
	focus       PopupFocusState
}

type popupInputToken struct {
	runtime *runtime
	frame   uint64
	owner   int32
	depth   int
}

type popupInputOwner struct {
	owner    int32
	hasOwner bool
}

func (r *runtime) currentPopupInputOwner() popupInputOwner {
	if n := len(r.popupInputScopes); n != 0 {
		return popupInputOwner{owner: r.popupInputScopes[n-1].owner, hasOwner: true}
	}
	return popupInputOwner{}
}

func (r *runtime) popupInputOwnerCaptures(owner popupInputOwner) bool {
	if r.currentPopupInputOwner() != owner {
		return true
	}
	return r.popupKeyboardCapturesOwner(owner.owner, owner.hasOwner)
}

func (r *runtime) popupDescendsFrom(owner, ancestor int32) bool {
	// Callers ask for proper descendants; the shared ancestry walk includes self.
	if owner == ancestor {
		return false
	}
	state := PopupAncestry{}
	for !state.Done {
		panel, found := r.popupPanels[owner]
		state = PopupOwnership_PopupAncestryAdvance(found, owner == ancestor, panel.hasParent)
		owner = panel.parent
	}
	return state.Contains
}

func (r *runtime) beginPopupInput(owner int32, bounds Rectangle) popupInputToken {
	for _, scope := range r.popupInputScopes {
		if scope.owner == owner {
			panic("popup owner is already active")
		}
	}
	if r.popupPanels == nil {
		r.popupPanels = make(map[int32]popupInputPanel)
	}
	token := popupInputToken{r, r.paintLayerFrame, owner, len(r.popupInputScopes)}
	r.popupInputOrder++
	panel, existed := r.popupPanels[owner]
	panel.focus = PopupOwnership_PopupFocusInitialize(panel.focus, existed, r.focusID)
	panel.bounds = bounds
	panel.seen = r.paintLayerFrame
	panel.order = r.popupInputOrder
	panel.hasParent = false
	if token.depth != 0 {
		panel.parent = r.popupInputScopes[token.depth-1].owner
		panel.hasParent = true
		if r.popupDescendsFrom(panel.parent, owner) {
			panic("popup ownership cycle")
		}
	}
	parent, foundParent := r.popupPanels[panel.parent]
	panel.alive = PopupOwnership_PopupOwnerAlive(panel.hasParent, foundParent && parent.alive)
	r.popupPanels[owner] = panel
	r.popupInputScopes = append(r.popupInputScopes, token)
	return token
}

func (r *runtime) endPopupInput(token popupInputToken) {
	n := len(r.popupInputScopes)
	if n == 0 || r.popupInputScopes[n-1] != token || token.frame != r.paintLayerFrame {
		panic("popup input scopes must close in their opening frame and stack order")
	}
	r.popupInputScopes = r.popupInputScopes[:n-1]
}

func (r *runtime) closePopupInput(owner int32) {
	panel, found := r.popupPanels[owner]
	restore := false
	removed := []int32{owner}
	for id, child := range r.popupPanels {
		if r.popupDescendsFrom(id, owner) {
			removed = append(removed, id)
		}
		if PopupOwnership_PopupFocusRestore(child.focus, r.focusID,
			id == owner || r.popupDescendsFrom(id, owner), false) {
			restore = true
		}
	}
	if focusOwner, ok := r.popupFocus[r.focusID]; ok && focusOwner.hasOwner &&
		(focusOwner.owner == owner || r.popupDescendsFrom(focusOwner.owner, owner)) {
		restore = true
	}
	for _, id := range removed {
		delete(r.popupPanels, id)
	}
	if found && restore {
		r.setFocus(panel.focus.RestoreFocus)
	}
}

func (r *runtime) prunePopupInput() {
	if len(r.popupInputScopes) != 0 {
		panic("unclosed popup input scope")
	}
	var missing []int32
	for id, panel := range r.popupPanels {
		if PopupOwnership_PopupOwnerRetired(panel.alive, panel.seen, r.paintLayerFrame) {
			missing = append(missing, id)
		}
	}
	for _, id := range missing {
		r.closePopupInput(id)
	}
}

// Compare whole branches, not just the two leaf timestamps. All descendants
// of a later sibling layer paint above an earlier sibling's descendants.
func (r *runtime) popupAbove(a, b int32) bool {
	originA, originB := a, b
	state := PopupOrder{}
	hasA, hasB := true, true
	for !state.Done {
		ap, foundA := r.popupPanels[a]
		bp, foundB := r.popupPanels[b]
		hasA, hasB = hasA && foundA, hasB && foundB
		sameParent := ap.hasParent == bp.hasParent && (!ap.hasParent || ap.parent == bp.parent)
		state = PopupOwnership_PopupOrderAdvance(state, hasA, hasB, a == b && hasA && hasB,
			sameParent, ap.order, bp.order)
		if state.ResetA {
			a, hasA = originA, true
		} else if state.MoveA {
			a, hasA = ap.parent, ap.hasParent
		}
		if state.ResetB {
			b, hasB = originB, true
		} else if state.MoveB {
			b, hasB = bp.parent, bp.hasParent
		}
	}
	return state.Above
}

func (r *runtime) popupCaptures(x, y float32) bool {
	var top int32
	found := false
	for id, panel := range r.popupPanels {
		if !panel.alive || !pointInRect(x, y, panel.bounds) {
			continue
		}
		if !found || r.popupAbove(id, top) {
			top, found = id, true
		}
	}
	if n := len(r.popupInputScopes); n != 0 {
		owner := r.popupInputScopes[n-1].owner
		panel, exists := r.popupPanels[owner]
		return PopupOwnership_PopupInputCaptured(true, exists && panel.alive, found, owner == top)
	}
	return PopupOwnership_PopupInputCaptured(false, false, found, false)
}

func (r *runtime) closeDropdown(owner int32) {
	delete(r.dropdownGestures, owner)
	if offset := r.dropdownOffsets[owner]; offset != nil && r.scrollDragOffset == offset {
		r.scrollDragOffset = nil
	}
	delete(r.dropdownOffsets, owner)
	delete(r.openDropdowns, owner)
	delete(r.dropdownHighlight, owner)
	r.closePopupInput(owner)
}

// Keyboard ownership follows the top branch regardless of pointer position.
func (r *runtime) popupKeyboardCaptures() bool {
	if n := len(r.popupInputScopes); n != 0 {
		return r.popupKeyboardCapturesOwner(r.popupInputScopes[n-1].owner, true)
	}
	return r.popupKeyboardCapturesOwner(0, false)
}

func (r *runtime) popupKeyboardCapturesOwner(owner int32, hasOwner bool) bool {
	var top int32
	found := false
	for id, panel := range r.popupPanels {
		if !panel.alive {
			continue
		}
		if !found || r.popupAbove(id, top) {
			top, found = id, true
		}
	}
	if hasOwner {
		panel, exists := r.popupPanels[owner]
		return PopupOwnership_PopupInputCaptured(true, exists && panel.alive, found, owner == top)
	}
	return PopupOwnership_PopupInputCaptured(false, false, found, false)
}

type popupFocusOwner struct {
	owner    int32
	hasOwner bool
	seen     uint64
}

func (r *runtime) registerPopupFocus(id int32) {
	if r.popupFocus == nil {
		r.popupFocus = make(map[int32]popupFocusOwner)
	}
	owner := popupFocusOwner{seen: r.paintLayerFrame}
	if n := len(r.popupInputScopes); n != 0 {
		owner.owner, owner.hasOwner = r.popupInputScopes[n-1].owner, true
		panel := r.popupPanels[owner.owner]
		decision := PopupOwnership_PopupFocusRegister(panel.focus, id, r.focusID,
			panel.alive, true, r.popupKeyboardCapturesOwner(owner.owner, true))
		panel.focus = decision.State
		if decision.Acquire {
			r.setFocus(id)
		}
		r.popupPanels[owner.owner] = panel
	}
	r.popupFocus[id] = owner
}

func (r *runtime) popupFocusCaptures(id int32) bool {
	owner := r.popupFocus[id]
	return r.popupKeyboardCapturesOwner(owner.owner, owner.hasOwner)
}
