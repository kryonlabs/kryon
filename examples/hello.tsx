import * as App from "kryon/app";
import { Session } from "kryon/session";
import { Text } from "kryon/tsx/text";

function Draw(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Text session={session} props={{text: "Hello, world!", size: s32(32)}}/>;
}

export function main(): s32 {
    return App.Run({title: "Hello", width: s32(320), height: s32(160)}, Draw);
}
