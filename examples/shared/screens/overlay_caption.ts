import { Rectangle } from "kryon/geometry";

export function OverlayCaptionBounds(viewport: Rectangle, y: float32): Rectangle {
    const scale = viewport.width / float32(480);
    return {x: viewport.x + float32(28) * scale, y: viewport.y + y, width: float32(420) * scale, height: 38};
}
