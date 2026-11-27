// Width, height, dark and measured progress are numeric command-line arguments.
import * as Preview from "loading_preview";
import { LoadingScreen } from "kryon/tsx/loading_screen";

function Draw(frame: Preview.PreviewFrame): void {
    <LoadingScreen session={frame.session} props={{bounds: frame.bounds,
        title: "Inner Breeze", detail: "Getting things ready for you",
        icon_shape: "S1.7rr:M12 21C6 17 5 9 12 3C19 9 18 17 12 21ZM12 21V10M12 16L7 12M12 13L16 9",
        now: frame.now, scale: 1, has_progress: frame.measured, completed: s32(3), total: s32(5)}}/>;
}

export function main(argc: s32, argv: NativeRef<NativeRef<u8>>): s32 {
    return Preview.Run(argc, argv, Draw);
}
