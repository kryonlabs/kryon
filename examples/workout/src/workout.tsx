import * as App from "kryon/app";
import * as Model from "workout_model";
import { Session } from "kryon/session";
import { Image } from "kryon/tsx/image";

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const rendered = Model.PrepareFrame(frame);
    if (rendered.image_ready) {
        <Image session={session} props={{bounds: frame.viewport, texture: rendered.texture, smooth: true}}/>;
    }
    Model.FinishFrame(rendered);
}

export function main(): s32 {
    Model.ConfigurePlatform();
    return App.Run({title: "Workout", width: s32(540), height: s32(900)}, Frame);
}
