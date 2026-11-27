import * as NativeFieldset from "kryon/fieldset";
import * as Widgets from "kryon/tsx/widget";

export function Fieldset(widget: Widgets.WidgetProps<NativeFieldset.Props>): void {
    NativeFieldset.Draw(widget.session, widget.props);
}
