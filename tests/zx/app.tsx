import * as App from "kryon/app";
import { Session } from "kryon/session";
import { KeyboardTake, TypedCodepointTake } from "kryon/tree_input";
import { Button } from "kryon/tsx/button";
import { Image } from "kryon/tsx/image";
import { Text } from "kryon/tsx/text";
import { WindowQuit } from "kryon/window";

let clicks: s32 = s32(0);
let frames: s32 = s32(0);

function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    frames = frames + s32(1);
    const clicked = Button({session: session, props: {key: u64(1n),
        bounds: {x: 40, y: 40, width: 240, height: 64}, label: "Click the portable app"}});
    if (clicked) { clicks = clicks + s32(1); }
    <Text session={session} props={{key: u64(2n), bounds: {x: 40, y: 128, width: 400, height: 40},
        text: clicks > s32(0) ? "Clicked!" : "Zx state persists", size: s32(24)}}/>;
    <Image session={session} props={{key: u64(3n), bounds: {x: 500, y: 80, width: 200, height: 200},
        asset_path: "assets/red.png", alt_text: "Red square"}}/>;
    const key = KeyboardTake(session);
    const typed = TypedCodepointTake(session);
    if (key === s32(65) || typed === s32(233)) {
        if (clicks === s32(1) && frames > s32(2)) {
            print("zx interaction passed\n");
        } else {
            print("zx interaction failed: clicks=% frames=%\n", clicks, frames);
        }
        WindowQuit();
    }
}

export function main(): s32 { return App.Run({title: "app"}, Frame); }
