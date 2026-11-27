import * as Surface from "kryon/host/surface";
import * as State from "hosted_surface_state";
import { Session } from "kryon/session";
import { TreeRootBounds } from "kryon/tree";
import { Button } from "kryon/tsx/button";
import { Text } from "kryon/tsx/text";
import { TextField } from "kryon/tsx/text_field";

function Draw(frame: Surface.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = TreeRootBounds(session);
    const width = viewport.width - float32(24);
    State.ObserveFrame(session, viewport);
    if (Button({session: session, props: {bounds: {x: 12, y: 36, width: width, height: 38},
        label: "Count click"}})) {
        State.CountClick();
    }
    <Text session={session} props={{bounds: {x: 12, y: 8, width: width, height: 24},
        text: State.CountText(), size: s32(18)}}/>;
    const changed = TextField({session: session, state: nativeRef(State.value),
        props: {bounds: {x: 12, y: 82, width: width, height: 38}, placeholder: "Type here"}});
    State.FieldChanged(changed.changed);
}

export function main(): s32 {
    return State.Run(Draw);
}
