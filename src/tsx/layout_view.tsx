import * as NativeLayout from "kryon/layout";
import { Session } from "kryon/session";
import { StyleData } from "kryon/style";
import { End } from "kryon/tree";
import { Scoped } from "std/callable";
import { LayoutProps } from "kryon/tsx/group";

export interface MeasuredLayoutProps {
    session: Session;
    props: NativeLayout.Props;
    items: NativeLayout.FlexItem[];
    placements: NativeLayout.FlexPlacement[];
    children: Scoped<(container: NativeLayout.LayoutContainer) => void>;
}

export interface ResolvedLayoutProps {
    session: Session;
    props: NativeLayout.Props;
    children: Scoped<(container: NativeLayout.LayoutContainer) => void>;
}

export interface StyledBoxProps {
    session: Session;
    props: NativeLayout.Props;
    face: StyleData;
    children: Scoped<() => void>;
}

export function Column(layout: LayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Draw(layout.session, layout.props);
    if (result.opened) {
        layout.children();
        End(layout.session);
    }
    return result;
}

export function ResolvedColumn(layout: ResolvedLayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Draw(layout.session, layout.props);
    if (result.opened) {
        layout.children(result);
        End(layout.session);
    }
    return result;
}

export function Row(layout: LayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Row(layout.session, layout.props);
    if (result.opened) {
        layout.children();
        End(layout.session);
    }
    return result;
}

export function ResolvedRow(layout: ResolvedLayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Row(layout.session, layout.props);
    if (result.opened) {
        layout.children(result);
        End(layout.session);
    }
    return result;
}

export function Stack(layout: LayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Stack(layout.session, layout.props);
    if (result.opened) {
        layout.children();
        End(layout.session);
    }
    return result;
}

export function ResolvedStack(layout: ResolvedLayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Stack(layout.session, layout.props);
    if (result.opened) {
        layout.children(result);
        End(layout.session);
    }
    return result;
}

export function StyledBox(layout: StyledBoxProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Box(layout.session, layout.props, layout.face);
    if (result.opened) {
        layout.children();
        End(layout.session);
    }
    return result;
}

export function Screen(layout: LayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Screen(layout.session, layout.props);
    if (result.opened) {
        layout.children();
        End(layout.session);
    }
    return result;
}

export function ResolvedScreen(layout: ResolvedLayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Screen(layout.session, layout.props);
    if (result.opened) {
        layout.children(result);
        End(layout.session);
    }
    return result;
}

export function MeasuredColumn(layout: MeasuredLayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Draw(layout.session, layout.props, layout.items, layout.placements);
    if (result.opened) {
        layout.children(result);
        End(layout.session);
    }
    return result;
}

export function MeasuredRow(layout: MeasuredLayoutProps): NativeLayout.LayoutContainer {
    const result = NativeLayout.Row(layout.session, layout.props, layout.items, layout.placements);
    if (result.opened) {
        layout.children(result);
        End(layout.session);
    }
    return result;
}
