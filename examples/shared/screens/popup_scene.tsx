import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { Popup } from "kryon/tsx/overlay";
import { Text } from "kryon/tsx/text";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { OverlayCaptionBounds } from "overlay_caption";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneDismissed: boolean = false;
let galleryDismissed: boolean = false;

function DrawPopup(frame: App.Frame, reopenBounds: Rectangle, dismissed: boolean): boolean {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (dismissed) {
        const reopen = Button({session: session, props: {bounds: reopenBounds, label: "Show popup again",
            disabled: exampleDisabled}});
        return !reopen;
    }
    const bounds = SelectionBounds(frame.viewport, 64, 188);
    const result = Popup({session: session, props: {key: u64(1), bounds: bounds}});
    if (result.panel_node >= s32(0)) {
        <Box session={session} props={{bounds: bounds, fill: StyleTokenColor("surface"),
            border: StyleTokenColor("border"), radius: 8}}/>;
        <Text session={session} props={{bounds: OverlayCaptionBounds(frame.viewport, 120), text: "A floating panel"}}/>;
    }
    return result.closed;
}

function DrawStandalone(frame: App.Frame): void {
    standaloneDismissed = DrawPopup(frame, nativeInit<Rectangle>({height: 44}), standaloneDismissed);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="Popup">{DrawStandalone(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    galleryDismissed = DrawPopup(frame, SelectionBounds(frame.viewport, 64, 44), galleryDismissed);
}

export function main(): s32 {
    return App.Run({title: "popup"}, Frame);
}
