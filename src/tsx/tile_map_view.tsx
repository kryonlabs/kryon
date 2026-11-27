import * as NativeTileMap from "kryon/tile_map";
import * as Widgets from "kryon/tsx/widget";

export function TileMap(widget: Widgets.WidgetProps<NativeTileMap.Props>): NativeTileMap.TileMapResult {
    return NativeTileMap.Draw(widget.session, widget.props);
}
