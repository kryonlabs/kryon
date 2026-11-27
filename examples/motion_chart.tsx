import * as App from "kryon/app";
import * as State from "motion_chart_state";
import * as Theme from "kryon/theme";
import { ColorFromPacked } from "kryon/drawing_props";
import { Session } from "kryon/session";
import { TreeRootBounds } from "kryon/tree";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { Text } from "kryon/tsx/text";

function Draw(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const bounds = TreeRootBounds(session);
    if (bounds.width < float32(280) || bounds.height < float32(240)) { return; }
    const theme = Theme.CurrentTheme();
    const x = bounds.x + float32(48);
    const y = bounds.y + float32(46);
    <Box session={session} props={{bounds: bounds, fill: ColorFromPacked(theme.colors.background)}}/>;
    <Box session={session} props={{radius: 18,
        bounds: {x: x - float32(24), y: y - float32(22), width: bounds.width - float32(48), height: bounds.height - float32(48)},
        fill: ColorFromPacked(theme.colors.surface_raised), border: ColorFromPacked(theme.colors.border)}}/>;
    <Text session={session} props={{bounds: {x: x, y: y, width: 104, height: 36}, text: "Revenue", size: s32(25)}}/>;
    if (Button({session: session, props: {label: "Switch week",
        bounds: {x: bounds.x + bounds.width - float32(158), y: y, width: 110, height: 36}}})) {
        State.SwitchWeek();
    }
    <Text session={session} props={{bounds: {x: x, y: y + float32(42), width: 165, height: 24},
        text: State.current_week ? "Current week" : "Previous week", size: s32(14)}}/>;
    State.DrawSeries(session, {x: x, y: y + float32(82), width: bounds.width - float32(96),
        height: bounds.height - float32(181)}, State.Phase(session, theme), theme.colors);
}

export function main(): s32 {
    State.Init();
    return App.Run({title: "Animated revenue chart", width: s32(600), height: s32(360)}, Draw);
}
