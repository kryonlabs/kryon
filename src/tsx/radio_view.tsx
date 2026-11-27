import * as NativeRadio from "kryon/radio";
import * as Widgets from "kryon/tsx/widget";

export function Radio(widget: Widgets.WidgetProps<NativeRadio.Props>): s32 {
    return NativeRadio.Draw(widget.session, widget.props);
}
