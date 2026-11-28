import * as App from "kryon/app";
import * as State from "mobile_state";
import { Session } from "kryon/session";
import { WindowQuit } from "kryon/window";
import { Button } from "kryon/tsx/button";
import { Image } from "kryon/tsx/image";
import { Progress } from "kryon/tsx/progress";
import { Text } from "kryon/tsx/text";
import { ControlledTextField } from "kryon/tsx/text_field";
import { TextFromBytes } from "std/byte_text_linux";

function Frame(frame: App.Frame): void {
    if (!State.AssetsValid()) { WindowQuit(); return; }
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    let snapshot = State.Snapshot();
    const current = TextFromBytes(nativeSlice(snapshot.current, s32(0), State.length));
    const accepted = current === "portable";
    if (Button({session: session, props: {key: u64(1n),
        bounds: {x: 40, y: 40, width: 240, height: 64}, label: "Click the portable app"}})) {
        State.Click();
    }
    <Text session={session} props={{key: u64(2n), bounds: {x: 40, y: 120, width: 250, height: 40},
        text: State.clicks > s32(0) ? "Clicked!" : "ZX state persists", size: s32(24)}}/>;
    <Image session={session} props={{key: u64(3n), bounds: {x: 200, y: 180, width: 100, height: 100},
        asset_path: "assets/red.png", alt_text: "Embedded red square"}}/>;
    const input = State.ReadInput(session, nativeSlice(snapshot.typed));
    const field = ControlledTextField({session: session, props: {key: u64(4n),
        bounds: {x: 40, y: 300, width: 240, height: 44}, value: current,
        placeholder: "Type portable", cursor: State.cursor, anchor: State.anchor,
        scroll_x: State.scroll, max_bytes: s32(256), input: input}});
    State.Apply(field);
    <Progress session={session} props={{key: u64(5n), bounds: {x: 40, y: 240, width: 120, height: 24},
        min: s32(0), max: s32(100), value: accepted && State.clicks === s32(1) ? s32(100) : s32(0)}}/>;
}

export function main(): s32 {
    return State.Result(App.Run({title: "zx mobile probe"}, Frame));
}
