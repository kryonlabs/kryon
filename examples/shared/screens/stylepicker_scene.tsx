import * as App from "kryon/app";
import { Session } from "kryon/session";
import { Card } from "kryon/tsx/card";
import { Text } from "kryon/tsx/text";
import { EditorExample } from "editor_example";

function ModernStyle(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Card session={session} props={{}}>
        <Text session={session} props={{bounds: {height: 20}, text: "One modern style"}}/>
    </Card>;
    <Text session={session} props={{bounds: {height: 38}, text: "Colors and light/dark remain customizable."}}/>;
}

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="Modern style">{ModernStyle(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void { ModernStyle(frame); }

export function main(): s32 { return App.Run({title: "stylepicker"}, Frame); }
