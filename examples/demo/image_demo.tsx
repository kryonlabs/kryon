import * as State from "image_demo_state";
import * as NativeImage from "kryon/image";
import { SessionValid } from "kryon/session";
import { Image } from "kryon/tsx/image";

export function Frame(): s32 {
    const session = State.Begin();
    if (!SessionValid(session)) { return s32(-1); }
    <Image session={session} props={{class_name: s32(7), asset_path: "../docs/site/cursors/arrow.png",
        alt_text: "Arrow", bounds: {x: 112, y: 32, width: 96, height: 96},
        fit: NativeImage.ImageFit.ImageFitStretch}}/>;
    return State.End();
}
