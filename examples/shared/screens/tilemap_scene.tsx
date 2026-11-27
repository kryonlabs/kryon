import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { TileMap } from "kryon/tsx/tile_map";

let exampleDisabled: boolean = false;

function DrawTiles(session: Session): void {
    const tiles: [string] = ["assets/pfp.webp"];
    const names: [string] = ["Portrait"];
    const tile = s32(0);
    const empty = s32(-1);
    const pattern: [s32, s32, s32, s32, s32, s32, s32, s32, s32] = [
        tile, empty, tile,
        empty, tile, empty,
        tile, empty, tile,
    ];
    <TileMap session={session} props={{bounds: {height: 224}, columns: s32(3), rows: s32(3), tiles: nativeSlice(tiles),
        names: nativeSlice(names), layers: nativeSlice(pattern), layer_count: s32(1), label: "Picture tiles", pixel_snap: true}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Tile map", size: s32(28)}}/>
        {DrawTiles(session)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "tilemap"}, Frame);
}
