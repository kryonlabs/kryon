import * as NativeBreadcrumb from "kryon/breadcrumb";
import { Session } from "kryon/session";

export interface BreadcrumbProps {
    session: Session;
    props: NativeBreadcrumb.Props;
    items: NativeBreadcrumb.BreadcrumbItem[];
}

export function Breadcrumb(widget: BreadcrumbProps): NativeBreadcrumb.BreadcrumbResult {
    return NativeBreadcrumb.Draw(widget.session, widget.props, widget.items);
}
