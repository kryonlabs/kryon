import * as NativeMoneyField from "kryon/money_field";
import * as Widgets from "kryon/tsx/widget";

export interface MoneyFieldProps {
    widget: Widgets.WidgetProps<NativeMoneyField.Props>;
    state: NativeRef<NativeMoneyField.MoneyFieldState>;
}

export function MoneyField(view: MoneyFieldProps): NativeMoneyField.MoneyFieldResult {
    return NativeMoneyField.Draw(view.widget.session, view.widget.props, view.state);
}
