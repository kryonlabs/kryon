import * as App from "kryon/app";
import * as NativeContextMenu from "kryon/context_menu";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { Button } from "kryon/tsx/button";
import { ContextMenu } from "kryon/tsx/context_menu";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneDismissed: boolean = false;
let galleryDismissed: boolean = false;

function DrawContextMenu(frame: App.Frame, anchor: Rectangle, reopenBounds: Rectangle, dismissed: boolean): boolean {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (dismissed) {
        const reopen = Button({session: session, props: {bounds: reopenBounds, label: "Open context menu"}});
        return !reopen;
    }
    const items: [NativeContextMenu.ContextMenuItem, NativeContextMenu.ContextMenuItem,
        NativeContextMenu.ContextMenuItem] = [
        nativeInit<NativeContextMenu.ContextMenuItem>({label: "Open", id: s32(1)}), nativeInit<NativeContextMenu.ContextMenuItem>({label: "Rename", id: s32(2)}),
        nativeInit<NativeContextMenu.ContextMenuItem>({label: "Remove", id: s32(3), disabled: exampleDisabled})];
    const result = ContextMenu({session: session, props: {open: true, x: anchor.x, y: anchor.y}, items: nativeSlice(items)});
    return result.closed;
}

function DrawStandalone(frame: App.Frame): void {
    const viewport = frame.viewport;
    standaloneDismissed = DrawContextMenu(frame, nativeInit<Rectangle>({x: viewport.x + 24, y: viewport.y + 64}),
        nativeInit<Rectangle>({height: 44}), standaloneDismissed);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="ContextMenu">{DrawStandalone(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    const bounds = SelectionBounds(frame.viewport, 64, 44);
    galleryDismissed = DrawContextMenu(frame, bounds, bounds, galleryDismissed);
}

export function main(): s32 {
    return App.Run({title: "contextmenu"}, Frame);
}
