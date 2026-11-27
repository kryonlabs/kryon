import * as App from "kryon/app";
import { Session } from "kryon/session";
import { Button } from "kryon/tsx/button";
import { Text } from "kryon/tsx/text";
import { FormatInteger } from "std/number_text";
import { Vec } from "std/vec";

let digits: Vec<u8> = {};
let count: s32 = s32(0);

function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const width = viewport.width - 40;
    const label = FormatInteger(s64(count), VecSlice(digits, s64(0), digits.count));
    <Text session={session} props={{bounds: {x: viewport.x + 20, y: viewport.y + 20, width: width, height: 64},
        text: label, size: s32(40)}}/>;
    if (<Button session={session} props={{bounds: {x: viewport.x + 20, y: viewport.y + 104, width: width, height: 44},
        label: "Increment"}}/>) {
        count += s32(1);
    }
}

export function main(): s32 {
    if (!VecResize(digits, s64(24))) {
        return s32(1);
    }
    const result = App.Run({title: "counter"}, Frame);
    VecFree(digits);
    return result;
}
