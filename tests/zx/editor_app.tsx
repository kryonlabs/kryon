import * as App from "kryon/app";
import * as State from "editor_state";
import { Session } from "kryon/session";
import { ControlledTextField } from "kryon/tsx/text_field";
import { TextFromBytes } from "std/byte_text_linux";

function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    State.Init();
    let snapshot = State.Snapshot();
    const input = State.ReadInput(session, nativeSlice(snapshot.typed));
    const field = ControlledTextField({session: session, props: {
        key: u64(1n), bounds: {x: 20, y: 20, width: 280, height: 44},
        value: TextFromBytes(nativeSlice(snapshot.current, s32(0), State.length)),
        cursor: State.cursor, anchor: State.anchor,
        max_bytes: s32(64), input: input,
    }});
    State.Apply(field);
    State.VerifyIsolation(session, frame.viewport);
}

export function main(): s32 {
    const code = App.Run({title: "editor_app"}, Frame);
    return State.Result(code);
}
