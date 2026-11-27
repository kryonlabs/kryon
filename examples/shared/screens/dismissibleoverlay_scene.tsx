import * as App from "kryon/app";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { Button } from "kryon/tsx/button";
import { DismissibleOverlay } from "kryon/tsx/overlay";
import { Text } from "kryon/tsx/text";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { OverlayCaptionBounds } from "overlay_caption";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneDismissed: boolean = false;
let galleryDismissed: boolean = false;

function DrawDismissibleOverlay(frame: App.Frame, reopenBounds: Rectangle, dismissed: boolean): boolean {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (dismissed) {
        const reopen = Button({session: session, props: {bounds: reopenBounds, label: "Show overlay again",
            disabled: exampleDisabled}});
        return !reopen;
    }
    const result = DismissibleOverlay({session: session, props: {bounds: SelectionBounds(frame.viewport, 64, 188),
        scrim: {r: u8(16), g: u8(34), b: u8(45), a: u8(120)}, dismiss_disabled: exampleDisabled}});
    if (result.panel_node >= s32(0)) {
        <Text session={session} props={{bounds: OverlayCaptionBounds(frame.viewport, 128),
            text: "Click outside the panel to dismiss it."}}/>;
    }
    return result.closed;
}

function DrawStandalone(frame: App.Frame): void {
    standaloneDismissed = DrawDismissibleOverlay(frame, nativeInit<Rectangle>({height: 44}), standaloneDismissed);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="DismissibleOverlay">{DrawStandalone(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    galleryDismissed = DrawDismissibleOverlay(frame, SelectionBounds(frame.viewport, 64, 44), galleryDismissed);
}

export function main(): s32 {
    return App.Run({title: "dismissibleoverlay"}, Frame);
}
