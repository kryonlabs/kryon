import * as NativeImage from "kryon/image";
import * as Widgets from "kryon/tsx/widget";

export function Image(widget: Widgets.WidgetProps<NativeImage.Props>): void {
    NativeImage.Draw(widget.session, widget.props);
}
