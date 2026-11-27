import * as App from "kryon/app";
import { Session } from "kryon/session";
import { Toast } from "kryon/tsx/toast";
import { EditorExample } from "editor_example";

function Message(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Toast session={session} props={{viewport: {x: frame.viewport.x, y: frame.viewport.y,
        width: frame.viewport.width, height: 280}, message: "Your changes have been saved",
        seconds: 4, now_seconds: 1}}/>;
}

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="Toast">{Message(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void { Message(frame); }

export function main(): s32 { return App.Run({title: "toast"}, Frame); }
