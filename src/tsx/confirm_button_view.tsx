import * as NativeConfirmButton from "kryon/confirm_button";
import * as Widgets from "kryon/tsx/widget";

export interface ConfirmButtonProps {
    widget: Widgets.WidgetProps<NativeConfirmButton.Props>;
    state: NativeRef<NativeConfirmButton.ConfirmButtonState>;
}

export function ConfirmButton(view: ConfirmButtonProps): NativeConfirmButton.ConfirmButtonResult {
    return NativeConfirmButton.Draw(view.widget.session, view.widget.props, view.state);
}
