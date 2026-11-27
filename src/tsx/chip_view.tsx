import * as NativeChip from "kryon/chip";
import * as Widgets from "kryon/tsx/widget";

export function Chip(widget: Widgets.WidgetProps<NativeChip.Props>): NativeChip.ChipResult {
    return NativeChip.Draw(widget.session, widget.props);
}
