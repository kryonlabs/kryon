import * as App from "kryon/app";
import * as State from "services_state";
import { Session } from "kryon/session";
import { CursorSet, CursorShape } from "kryon/cursor";
import { WindowQuit } from "kryon/window";
import { Text } from "kryon/tsx/text";
import { Circle } from "kryon/tsx/shape";
import { ServiceKeepOpen } from "service_urls";

function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (!CursorSet(session, {shape: s32(CursorShape.CursorCrosshair)})) {
        if (State.Failure(s32(18))) { WindowQuit(); return; }
    }
    <Text session={session} props={{bounds: {x: 20, y: 20, width: 800, height: 50},
        text: "Portable audio and asynchronous HTTP"}}/>;
    if (ServiceKeepOpen() && (State.stage < s32(0) || State.stage === s32(4))) {
        <Circle session={session} props={{center: {x: 160, y: 160}, radius: 60,
            fill: State.stage === s32(4) ? {r: u8(20), g: u8(180), b: u8(80), a: u8(255)} :
                {r: u8(220), g: u8(30), b: u8(40), a: u8(255)}}}/>;
        return;
    }
    State.Tick();
    if (State.Finished()) { WindowQuit(); }
}

export function main(): s32 {
    const code = App.Run({title: "services"}, Frame);
    return State.Result(code);
}
