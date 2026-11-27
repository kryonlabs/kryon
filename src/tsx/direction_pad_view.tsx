import * as NativeDirectionPad from "kryon/direction_pad";
import * as Widgets from "kryon/tsx/widget";

export function DirectionPad(widget: Widgets.WidgetProps<NativeDirectionPad.Props>): NativeDirectionPad.DirectionPadResult {
    return NativeDirectionPad.Draw(widget.session, widget.props);
}
