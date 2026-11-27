import * as NativeHoldButton from "kryon/hold_button";
import * as Widgets from "kryon/tsx/widget";

export interface HoldButtonProps {
    widget: Widgets.WidgetProps<NativeHoldButton.Props>;
    state: NativeRef<NativeHoldButton.HoldButtonState>;
}

export function HoldButton(view: HoldButtonProps): NativeHoldButton.HoldButtonResult {
    return NativeHoldButton.Draw(view.widget.session, view.widget.props, view.state);
}
