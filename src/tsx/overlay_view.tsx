import * as NativeOverlay from "kryon/overlay";
import * as Widgets from "kryon/tsx/widget";

export function DismissibleOverlay(widget: Widgets.WidgetProps<NativeOverlay.Props>): NativeOverlay.DismissibleOverlayResult {
    return NativeOverlay.Draw(widget.session, widget.props);
}

export function Popup(widget: Widgets.WidgetProps<NativeOverlay.PopupProps>): NativeOverlay.PopupResult {
    return NativeOverlay.Popup(widget.session, widget.props);
}
