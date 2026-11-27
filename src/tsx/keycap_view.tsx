import * as NativeKeycap from "kryon/keycap";
import * as Widgets from "kryon/tsx/widget";

export function Keycap(widget: Widgets.WidgetProps<NativeKeycap.Props>): NativeKeycap.KeycapResult {
    return NativeKeycap.Draw(widget.session, widget.props);
}
