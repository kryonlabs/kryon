import * as NativeLoadingIndicator from "kryon/loading_indicator";
import * as Widgets from "kryon/tsx/widget";

export function LoadingIndicator(widget: Widgets.WidgetProps<NativeLoadingIndicator.Props>): void {
    NativeLoadingIndicator.Draw(widget.session, widget.props);
}
