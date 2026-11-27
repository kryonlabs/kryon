import { Color } from "kryon/drawing_props";
import * as NativeSortable from "kryon/sortable";
import { Session } from "kryon/session";
import * as Widgets from "kryon/tsx/widget";

export interface SortableItemProps {
    widget: Widgets.WidgetProps<NativeSortable.ItemProps>;
    state: NativeRef<NativeSortable.State>;
}

export function SortableItem(item: SortableItemProps): NativeSortable.ItemResult {
    return NativeSortable.Item(item.widget.session, item.state, item.widget.props);
}

export interface SortableIndicatorProps {
    session: Session;
    key: u64;
    drop: NativeSortable.Drop;
    color: Color;
    thickness: float32;
    radius: float32;
}

export function SortableIndicator(indicator: SortableIndicatorProps): void {
    NativeSortable.Indicator(indicator.session, indicator.key, indicator.drop,
        indicator.color, indicator.thickness, indicator.radius);
}
