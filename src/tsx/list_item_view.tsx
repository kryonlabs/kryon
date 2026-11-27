import * as NativeListItem from "kryon/list_item";
import * as Widgets from "kryon/tsx/widget";

export function ListItem(widget: Widgets.WidgetProps<NativeListItem.Props>): NativeListItem.ListItemResult {
    return NativeListItem.Draw(widget.session, widget.props);
}
