import * as NativeLayout from "kryon/layout";
import { Session } from "kryon/session";
import { End } from "kryon/tree";
import { Scoped } from "std/callable";

export interface LayoutProps {
    session: Session;
    props: NativeLayout.Props;
    children: Scoped<() => void>;
}

export interface ResolvedGroupProps {
    session: Session;
    props: NativeLayout.Props;
    children: Scoped<(container: NativeLayout.LayoutContainer) => void>;
}

export function Group(layout: LayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Group(layout.session, layout.props);
    if (result.opened) {
        layout.children();
        End(layout.session);
    }
    return result;
}

export function ResolvedGroup(layout: ResolvedGroupProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Group(layout.session, layout.props);
    if (result.opened) {
        layout.children(result);
        End(layout.session);
    }
    return result;
}
