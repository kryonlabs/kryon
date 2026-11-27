import * as NativeLayout from "kryon/layout";
import * as NativeGrid from "kryon/grid_props";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { End } from "kryon/tree";
import { Scoped } from "std/callable";

export interface GridProps {
    session: Session;
    props: NativeGrid.GridProps;
    children: Scoped<() => void>;
}

export interface MeasuredGridProps {
    session: Session;
    props: NativeGrid.GridProps;
    items: NativeGrid.GridItem[];
    itemBounds: Rectangle[];
    children: Scoped<(grid: NativeLayout.GridResult) => void>;
}

export interface ResolvedGridProps {
    session: Session;
    props: NativeGrid.GridProps;
    children: Scoped<(grid: NativeLayout.GridResult) => void>;
}

export function Grid(widget: GridProps): NativeLayout.GridResult {
    const result = NativeLayout.Grid(widget.session, widget.props);
    if (result.container.opened) {
        widget.children();
        End(widget.session);
    }
    return result;
}

export function MeasuredGrid(widget: MeasuredGridProps): NativeLayout.GridResult {
    const result = NativeLayout.Grid(widget.session, widget.props, widget.items, widget.itemBounds);
    if (result.container.opened) {
        widget.children(result);
        End(widget.session);
    }
    return result;
}

export function ResolvedGrid(widget: ResolvedGridProps): NativeLayout.GridResult {
    const result = NativeLayout.Grid(widget.session, widget.props);
    if (result.container.opened) {
        widget.children(result);
        End(widget.session);
    }
    return result;
}
