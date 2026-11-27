import * as NativeScrubField from "kryon/scrub_field";
import * as Widgets from "kryon/tsx/widget";

export interface ScrubFieldProps {
    widget: Widgets.WidgetProps<NativeScrubField.Props>;
    state: NativeRef<NativeScrubField.ScrubFieldState>;
}

export function ScrubField(view: ScrubFieldProps): NativeScrubField.ScrubFieldResult {
    return NativeScrubField.Draw(view.widget.session, view.widget.props, view.state);
}
