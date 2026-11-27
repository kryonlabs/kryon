import * as NativePage from "kryon/page";
import * as Widgets from "kryon/tsx/widget";
import { Session } from "kryon/session";
import { End } from "kryon/tree";
import { Scoped } from "std/callable";

export interface PageProps {
    session: Session;
    props: NativePage.Props;
    children: Scoped<() => void>;
}

export interface SectionProps {
    session: Session;
    props: NativePage.SectionProps;
    children: Scoped<() => void>;
}

export function Page(widget: PageProps): NativePage.PageResult {
    const result = NativePage.Draw(widget.session, widget.props);
    if (result.opened) {
        widget.children();
        End(widget.session);
    }
    return result;
}

export function Section(widget: SectionProps): NativePage.SectionResult {
    const result = NativePage.Section(widget.session, widget.props);
    if (result.opened) {
        widget.children();
        End(widget.session);
    }
    return result;
}

export function Heading(widget: Widgets.WidgetProps<NativePage.HeadingProps>): void {
    NativePage.Heading(widget.session, widget.props);
}

export function ParagraphText(widget: Widgets.WidgetProps<NativePage.ParagraphTextProps>): void {
    NativePage.ParagraphText(widget.session, widget.props);
}
