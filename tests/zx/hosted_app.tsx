import * as App from "kryon/app_context";
import * as Protocol from "kryon/app_protocol";
import * as State from "hosted_state";
import { ScrollResult } from "kryon/scroll";
import { Button } from "kryon/tsx/button";
import { DirectionPad } from "kryon/tsx/direction_pad";
import { Image } from "kryon/tsx/image";
import { Joystick, SwipeArea } from "kryon/tsx/joystick";
import { Scroll } from "kryon/tsx/scroll";
import { Text } from "kryon/tsx/text";

export function main(): s32 {
    const mode = App.AppMode();
    if (mode === Protocol.APP_IDENTIFY) {
        State.Identify();
        return Protocol.APP_PROTOCOL;
    }
    if (mode === Protocol.APP_SAVE) { return State.Save(); }
    if (mode !== Protocol.APP_FRAME) { return s32(0); }
    const status = State.Load();
    if (status !== s32(0)) { return status; }
    const session = App.AppSession();

    if (Button({session: session, props: {key: u64(1n),
        bounds: {x: 40, y: 40, width: 240, height: 64},
        label: "Save a portable document"}})) {
        if (!State.Click()) { return s32(4); }
    }
    <Text session={session} props={{bounds: {x: 40, y: 128, width: 400, height: 40},
        text: State.clicked ? "Saved document" : "New document", size: s32(24)}}/>;
    <Image session={session} props={{bounds: {x: 500, y: 80, width: 200, height: 200},
        asset_path: App.AppAssetPath("assets/red.png")}}/>;
    State.ObserveKey();
    State.ObserveSwipe(SwipeArea({session: session, props: {key: u64(31000n), id: s32(31000),
        bounds: {x: 40, y: 320, width: 300, height: 120}, now: App.AppNow(), repeat_seconds: 0.1}}));
    State.ObserveJoystick(Joystick({session: session, props: {key: u64(32000n), id: s32(32000),
        bounds: {x: 400, y: 320, width: 200, height: 120}, now: App.AppNow(), repeat_seconds: 0.1}}));
    State.ObservePad(DirectionPad({session: session, props: {key: u64(34000n), id: s32(34000),
        bounds: {x: 640, y: 300, width: 160, height: 160}, now: App.AppNow(),
        repeat_seconds: 0.1, floating: true}}));
    const scroll = Scroll({session: session, props: {key: u64(33000n),
        bounds: {x: 40, y: 460, width: 300, height: 120},
        content_height: s32(480), scroll_offset: State.scroll_offset},
        children: (view: ScrollResult): void => {
            <Text session={session} props={{bounds: {x: view.content.x, y: view.content.y,
                width: 240, height: 40}, text: "First row", size: s32(24)}}/>;
            <Text session={session} props={{bounds: {x: view.content.x, y: view.content.y + 440,
                width: 240, height: 40}, text: "Last row", size: s32(24)}}/>;
        }});
    State.ObserveScroll(scroll);
    return s32(0);
}
