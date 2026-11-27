import * as NativeLoadingScreen from "kryon/loading_screen";
import * as Widgets from "kryon/tsx/widget";

export function LoadingScreen(widget: Widgets.WidgetProps<NativeLoadingScreen.Props>): void {
    NativeLoadingScreen.Draw(widget.session, widget.props);
}
