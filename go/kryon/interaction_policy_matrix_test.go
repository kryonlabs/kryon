package kryon

import "testing"

func TestInteractionPolicyMatrixDisabledControls(t *testing.T) {
	if Button_ButtonActionEnabled(true, false) || Button_ButtonActionEnabled(false, true) {
		t.Fatal("disabled/loading button activated")
	}
	if !Button_ButtonActionEnabled(false, false) {
		t.Fatal("enabled button did not activate")
	}

	link := Link_LinkInteractionFor(true, false, true, true, true, false, true)
	if link.Active || !link.DisabledMarker || link.Activated || link.ConsumeRelease || link.State != ButtonStateDisabled {
		t.Fatalf("disabled link interaction changed: %+v", link)
	}

	popup := PopupPolicy_PopupDecisionFor(0, true)
	if !popup.Valid || popup.CapturesInput || PopupPolicy_PopupOpenAfterDisabled(popup, true, true) {
		t.Fatalf("disabled popup capture/open policy changed: %+v", popup)
	}
}

func TestInteractionPolicyMatrixEmptyData(t *testing.T) {
	frame := StyleFrame{}
	nav := ListBox_ListBoxNavigate(3, 0, ListBox_ListBoxKeyDown(), 24, 20, 80, 100, 1, frame)
	if nav.Selected != 3 || nav.Scroll != 24 || nav.Changed {
		t.Fatalf("empty list navigation changed: %+v", nav)
	}

	layout := ListBox_ListBoxLayoutFor(Rectangle{X: 0, Y: 0, Width: 120, Height: 80}, 0, 20, 0, 99, 1, frame)
	if layout.ContentHeight != 0 || layout.MaxScroll != 0 || layout.Scroll != 0 {
		t.Fatalf("empty list layout changed: %+v", layout)
	}

	context := TextInput_TextContextMenuStateFor(false, true, false, false, true)
	if context.CutEnabled || context.CopyEnabled || !context.PasteEnabled || context.SelectAllEnabled {
		t.Fatalf("empty text context state changed: %+v", context)
	}
}

func TestInteractionPolicyMatrixSimultaneousKeys(t *testing.T) {
	if !Focus_FocusActivationFor(true, true, false, false, true, true, true) {
		t.Fatal("enter should activate even when space is ignored by text input")
	}
	if Focus_FocusActivationFor(true, true, false, false, false, true, true) {
		t.Fatal("space should not activate while text input is active")
	}
	if ListBox_ListBoxKeyFor(true, true, true, true) != ListBox_ListBoxKeyHome() {
		t.Fatal("list-box simultaneous key priority changed")
	}
	if Drag_DragKeyboardDirectionFor(true, true) != 1 {
		t.Fatal("drag simultaneous left/right priority changed")
	}

	menuInput := Menu_MenuKeyboardInputFor(true, true, true, true, true, true, true, true, true)
	menu := Menu_MenuKeyboardDecisionFor(menuInput, 1, 0)
	if !menu.KeyHandled || menu.MoveDelta != -1 || menu.OpenOrActivate {
		t.Fatalf("menu simultaneous key priority changed: %+v", menu)
	}

	dragInput := Drag_DragKeyboardInputFor(1, true, true, true, true)
	drag := Drag_DragKeyboardValue(5, 1, 0, 10, dragInput)
	if !drag.Changed || drag.Value != 0 {
		t.Fatalf("drag home/end priority changed: %+v", drag)
	}
}

func TestInteractionPolicyMatrixReleaseWithoutPress(t *testing.T) {
	link := Link_LinkInteractionFor(false, false, true, true, true, false, false)
	if !link.Active || !link.Hovered || link.Activated || link.ConsumeRelease {
		t.Fatalf("release without press activated link: %+v", link)
	}

	row := ListBox_ListBoxRowDecisionFor(true, true, false, 2, 4)
	if row.Select || row.ConsumeRelease || row.Selected != 2 {
		t.Fatalf("release without selectable owner changed row: %+v", row)
	}
}

func TestInteractionPolicyMatrixDragCancellation(t *testing.T) {
	pointer := Drag_DragPointerDecisionFor(true, false, false, false, true, false, true, false)
	if !pointer.ClearActive || pointer.UpdateDelta || pointer.FinishActive {
		t.Fatalf("captured drag cancellation changed: %+v", pointer)
	}

	source := DragDrop_DragDropSourceDecisionFor(true, 7, 7, false, false, true, 5, 64, true, false, false, false, false)
	if !source.ClearSource || !source.Valid || source.ReturnsActive {
		t.Fatalf("drag-drop source cancellation changed: %+v", source)
	}

	reorder := Reorder_ReorderActiveItemLifecycleFor(1, 3, true, true)
	if !reorder.CancelActive || !reorder.IgnoreList {
		t.Fatalf("reorder cancellation changed: %+v", reorder)
	}
}

func TestInteractionPolicyMatrixPopupCapture(t *testing.T) {
	plain := PopupPolicy_PopupDecisionFor(0, false)
	modal := PopupPolicy_PopupDecisionFor(2, false)
	tooltip := PopupPolicy_PopupDecisionFor(1, false)
	if !plain.CapturesInput || !modal.CapturesInput || tooltip.CapturesInput {
		t.Fatalf("popup capture roles changed: plain=%+v modal=%+v tooltip=%+v", plain, modal, tooltip)
	}

	dismiss := PopupPolicy_PopupDismissDecisionFor(plain, true, true, false)
	if dismiss.Close || dismiss.ConsumeRelease {
		t.Fatalf("popup ignored consumed release changed: %+v", dismiss)
	}

	state := PopupPolicy_PopupLifecycleBegin(0, PopupFrameInput{
		ID: 1, Bounds: NewRectangle(0, 0, 100, 80), Open: true, HasOpen: true,
	})
	escape := PopupPolicy_PopupLifecycleKeyboard(state, true, true)
	if !escape.Visible || escape.CloseInput {
		t.Fatalf("popup keyboard capture suppression changed: %+v", escape)
	}
}

func TestInteractionPolicyMatrixFocusLoss(t *testing.T) {
	if Focus_FocusActivationFor(true, true, false, true, true, false, false) {
		t.Fatal("captured focus activation should be suppressed")
	}
	if !TextInput_TextFocusOwnerIsStale(true, 8, 10) {
		t.Fatal("stale focus owner was not detected")
	}

	release := TextInput_TextFocusReleaseDecisionFor(true, true, true, true, true, true)
	if !release.Release || !release.CancelSelf || !release.ClearOwner || !release.ClearFrameOwner || !release.ClearActiveFocus || !release.ClearFieldDrag || !release.ClearAreaDrag {
		t.Fatalf("focus-loss release cleanup changed: %+v", release)
	}
}

func TestInteractionPolicyMatrixNestedOwnershipRestoration(t *testing.T) {
	sameOwner := TextInput_TextFocusClaimDecisionFor(true, true, true)
	if !sameOwner.Claim || sameOwner.DisplacePrevious || sameOwner.CancelPrevious || sameOwner.ClearPeerSelection {
		t.Fatalf("same-owner focus restoration changed: %+v", sameOwner)
	}

	displacedOwner := TextInput_TextFocusClaimDecisionFor(true, true, false)
	if !displacedOwner.Claim || !displacedOwner.DisplacePrevious || !displacedOwner.CancelPrevious || !displacedOwner.ClearPeerSelection {
		t.Fatalf("displaced-owner focus claim changed: %+v", displacedOwner)
	}

	owner := TextInput_TextFocusOwnerDecisionFor(true, false, false, true, false)
	if !owner.Focused || !owner.AdoptOwner || !owner.MarkFrameOwner || owner.ClearTarget {
		t.Fatalf("focus owner adoption changed: %+v", owner)
	}
	owner = TextInput_TextFocusOwnerDecisionFor(true, false, false, true, true)
	if owner.Focused || owner.AdoptOwner || !owner.ClearTarget {
		t.Fatalf("nested owner suppression changed: %+v", owner)
	}

	foreign := Reorder_ReorderForeignActiveListFor(true, 1, 2, false)
	if !foreign.IgnoreList || !foreign.CancelActive {
		t.Fatalf("foreign owner release restoration changed: %+v", foreign)
	}
	foreign = Reorder_ReorderForeignActiveListFor(true, 1, 2, true)
	if !foreign.IgnoreList || foreign.CancelActive {
		t.Fatalf("foreign owner in-progress restoration changed: %+v", foreign)
	}
}
