import * as NativeProgress from "kryon/progress";
import * as Widgets from "kryon/tsx/widget";

export function Progress(widget: Widgets.WidgetProps<NativeProgress.Props>): void {
    NativeProgress.Draw(widget.session, widget.props);
}
