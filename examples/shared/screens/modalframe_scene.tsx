import * as App from "kryon/app";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { Button } from "kryon/tsx/button";
import { Modal } from "kryon/tsx/modal";
import { Text } from "kryon/tsx/text";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { OverlayCaptionBounds } from "overlay_caption";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneDismissed: boolean = false;
let galleryDismissed: boolean = false;

function DrawModal(frame: App.Frame, reopenBounds: Rectangle, dismissed: boolean): boolean {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (dismissed) {
        const reopen = Button({session: session, props: {bounds: reopenBounds, label: "Open modal again",
            disabled: exampleDisabled}});
        return !reopen;
    }
    const result = Modal({session: session, props: {bounds: SelectionBounds(frame.viewport, 64, 188),
        title: "A focused moment", right_label: "Done", has_right_action: true,
        dismiss_disabled: exampleDisabled, right_action_disabled: exampleDisabled}, children: (): void => {
        <Text session={session} props={{bounds: OverlayCaptionBounds(frame.viewport, 140),
            text: "Keep the next step clear"}}/>;
    }});
    return result.dismissed || result.right_clicked;
}

function DrawStandalone(frame: App.Frame): void {
    standaloneDismissed = DrawModal(frame, nativeInit<Rectangle>({height: 44}), standaloneDismissed);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="ModalFrame">{DrawStandalone(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    galleryDismissed = DrawModal(frame, SelectionBounds(frame.viewport, 64, 188), galleryDismissed);
}

export function main(): s32 {
    return App.Run({title: "modalframe"}, Frame);
}
