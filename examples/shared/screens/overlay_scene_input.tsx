import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Rectangle } from "kryon/geometry";
import { KeyEnter, KeyEscape, KeyLeft, KeyRight, PointInBounds } from "kryon/input_protocol";
import { Session } from "kryon/session";
import { OverlayKeyInput, OverlayPointer } from "kryon/tooltip_common";
import { KeyboardTake, PointerSample, TreeEscapeCaptured } from "kryon/tree_input";

export function OverlayFrameBounds(viewport: Rectangle, live: boolean): Rectangle {
    if (live) {
        return viewport;
    }
    return nativeInit<Rectangle>({width: 480, height: 320});
}

export function OverlaySceneBounds(viewport: Rectangle, bounds: Rectangle): Rectangle {
    return {x: viewport.x + bounds.x * viewport.width / 480, y: viewport.y + bounds.y,
        width: bounds.width * viewport.width / 480, height: bounds.height};
}

export function OverlaySceneAnchor(viewport: Rectangle): Rectangle {
    return OverlaySceneBounds(viewport, {x: 38, y: 128, width: 126, height: 34});
}

export function OverlaySceneViewport(viewport: Rectangle): Rectangle {
    return OverlaySceneBounds(viewport, nativeInit<Rectangle>({width: 480, height: 320}));
}

export function OverlayScenePointer(session: Session, anchor: Rectangle): OverlayPointer {
    const pointer = PointerSample(session);
    return nativeInit<OverlayPointer>({x: pointer.x, y: pointer.y, down: pointer.down, pressed: pointer.pressed, released: pointer.released,
        over_anchor: PointInBounds(pointer.x, pointer.y, anchor)});
}

export function OverlaySceneKeys(session: Session): OverlayKeyInput {
    const key = KeyboardTake(session);
    return nativeInit<OverlayKeyInput>({escape: key === KeyEscape || TreeEscapeCaptured(session), enter: key === KeyEnter,
        left: key === KeyLeft, right: key === KeyRight});
}

export function OverlaySceneTime(session: Session): s64 {
    const seconds = float32(CaptureFrameInput(session).elapsed_ms) / float32(1000);
    return s64(seconds * float32(1000));
}
