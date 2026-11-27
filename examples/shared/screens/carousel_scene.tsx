import * as App from "kryon/app";
import { CarouselItem, CarouselState } from "kryon/carousel";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Carousel } from "kryon/tsx/carousel";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;
let state: CarouselState = nativeInit<CarouselState>({});
let opened: s32 = s32(-1);

function DrawSlides(session: Session, disabled: boolean, reducedMotion: boolean): void {
    const now = float64(CaptureFrameInput(session).elapsed_ms) / float64(1000);
    const items: [CarouselItem, CarouselItem] = [
        nativeInit<CarouselItem>({label: "Alpine lake", portrait: "assets/alpine-lake.webp", landscape: "assets/alpine-lake.webp",
            caption: "A quiet morning in the mountains", action: "Open", disabled: disabled}),
        nativeInit<CarouselItem>({label: "Coastal cliffs", portrait: "assets/coastal-cliffs.webp", landscape: "assets/coastal-cliffs.webp",
            caption: "Follow the trail along the ocean", action: "Create", disabled: disabled}),
    ];
    if (reducedMotion) {
        state.position = float32(state.selected);
    }
    const result = Carousel({session: session, state: nativeRef(state), items: nativeSlice(items), props: {disabled: disabled,
        bounds: {height: 320}, now: now, label: "Places to explore", previous_label: "Previous", next_label: "Next", scale: 1}});
    if (result >= s32(0)) {
        opened = result;
    }
    if (opened >= s32(0)) {
        <Text session={session} props={{disabled: disabled, bounds: {height: 36}, text: items[opened].label}}/>;
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    const reducedMotion = exampleReducedMotion;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Carousel", size: s32(28)}}/>
        {DrawSlides(session, disabled, reducedMotion)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "carousel"}, Frame);
}
