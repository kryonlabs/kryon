import * as App from "kryon/app";
import { Gap, LayoutMetricsFor } from "kryon/layout";
import { Session } from "kryon/session";
import { SkeletonShape } from "kryon/skeleton";
import { ThemeMetricRole } from "kryon/theme";
import { TreeCurrentParent, TreeSubmittedNodeAt } from "kryon/tree";
import { Column } from "kryon/tsx/layout";
import { Skeleton } from "kryon/tsx/skeleton";
import { Text } from "kryon/tsx/text";

let exampleReducedMotion: boolean = false;
let seconds: float64 = 0.0;

function ShortLineWidth(session: Session): float32 {
    const column = TreeSubmittedNodeAt(session, TreeCurrentParent(session));
    const content = LayoutMetricsFor(column.bounds, column.layout_gap, column.layout_padding).content;
    return content.width * float32(0.6);
}

export function SetExampleOptions(_disabled: boolean, reducedMotion: boolean): void {
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (!exampleReducedMotion) {
        seconds += 1.0 / 60.0;
    }
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        padding_override: true, gap: s32(10), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 40}, text: "Skeleton", size: s32(28)}}/>
        <Skeleton session={session} props={{shape: SkeletonShape.SkeletonCircle, now: seconds}}/>
        <Skeleton session={session} props={{shape: SkeletonShape.SkeletonLine, now: seconds}}/>
        <Skeleton session={session} props={{shape: SkeletonShape.SkeletonLine, now: seconds,
            bounds: {width: ShortLineWidth(session)}}}/>
        <Skeleton session={session} props={{shape: SkeletonShape.SkeletonBlock, now: seconds}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "skeleton"}, Frame);
}
