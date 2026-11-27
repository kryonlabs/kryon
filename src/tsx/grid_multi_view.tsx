import * as NativeGridMulti from "kryon/grid_multi";
import { Session } from "kryon/session";

export interface GridMultiProps {
    session: Session;
    props: NativeGridMulti.Props;
    items: NativeGridMulti.GridMultiItem[];
    selected: boolean[];
}

export function GridMulti(widget: GridMultiProps): NativeGridMulti.GridMultiResult {
    return NativeGridMulti.Draw(widget.session, widget.props, widget.items, widget.selected);
}
