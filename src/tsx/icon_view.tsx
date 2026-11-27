import { Rectangle } from "kryon/geometry";
import { ImageForIcon } from "kryon/icon";
import { Session } from "kryon/session";
import { Image } from "kryon/tsx/image";

export interface IconProps {
    session: Session;
    icon: s32;
    bounds: Rectangle;
    altText: string;
}

export function Icon(widget: IconProps): void {
    <Image session={widget.session} props={ImageForIcon(widget.icon, widget.bounds, widget.altText)}/>;
}
