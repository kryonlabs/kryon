import * as App from "kryon/app";
import { ButtonTone } from "kryon/control_props";
import { Rectangle } from "kryon/geometry";
import * as NativeModal from "kryon/modal";
import { Session } from "kryon/session";
import { ActionModal } from "kryon/tsx/action_modal";
import { Button } from "kryon/tsx/button";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneDismissed: boolean = false;
let galleryDismissed: boolean = false;

function DrawActionModal(frame: App.Frame, reopenBounds: Rectangle, dismissed: boolean): boolean {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (dismissed) {
        const reopen = Button({session: session, props: {bounds: reopenBounds, label: "Open dialog again"}});
        return !reopen;
    }
    const actions: [NativeModal.ActionModalAction, NativeModal.ActionModalAction] = [
        nativeInit<NativeModal.ActionModalAction>({label: "Cancel"}), nativeInit<NativeModal.ActionModalAction>({label: "Save", tone: ButtonTone.ButtonToneAccent, disabled: exampleDisabled})];
    const result = ActionModal({session: session, props: {title: "Save changes?",
        message: "Keep your latest ideas for next time.", max_width: s32(420)}, actions: nativeSlice(actions)});
    return result.action !== s32(0);
}

function DrawStandalone(frame: App.Frame): void {
    standaloneDismissed = DrawActionModal(frame, nativeInit<Rectangle>({height: 44}), standaloneDismissed);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="ActionModal">{DrawStandalone(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    galleryDismissed = DrawActionModal(frame, SelectionBounds(frame.viewport, 64, 44), galleryDismissed);
}

export function main(): s32 {
    return App.Run({title: "actionmodal"}, Frame);
}
