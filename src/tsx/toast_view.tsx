import * as NativeToast from "kryon/toast";
import * as Widgets from "kryon/tsx/widget";

export function Toast(widget: Widgets.WidgetProps<NativeToast.Props>): NativeToast.ToastResult {
    return NativeToast.Draw(widget.session, widget.props);
}
