import * as NativeSvgIcon from "kryon/svg_icon";
import * as Widgets from "kryon/tsx/widget";

export function SvgIcon(widget: Widgets.WidgetProps<NativeSvgIcon.Props>): void {
    NativeSvgIcon.Draw(widget.session, widget.props);
}
