import * as NativeCarousel from "kryon/carousel";
import { Session } from "kryon/session";

export interface CarouselProps {
    session: Session;
    props: NativeCarousel.Props;
    items: NativeCarousel.CarouselItem[];
    state: NativeRef<NativeCarousel.CarouselState>;
}

export function Carousel(widget: CarouselProps): s32 {
    return NativeCarousel.Draw(widget.session, widget.props, widget.items, widget.state);
}
