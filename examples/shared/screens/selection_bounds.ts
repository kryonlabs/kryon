import { Rectangle } from "kryon/geometry";

export function SelectionBounds(viewport: Rectangle, y: float32, height: float32): Rectangle {
    return {x: viewport.x + float32(24) * viewport.width / float32(480),
        y: viewport.y + y, width: float32(432) * (viewport.width / float32(480)), height: height};
}
