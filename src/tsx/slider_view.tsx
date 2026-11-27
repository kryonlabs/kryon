import * as NativeSlider from "kryon/slider";
import * as Widgets from "kryon/tsx/widget";

export function Slider(widget: Widgets.WidgetProps<NativeSlider.Props>): NativeSlider.SliderStep {
    return NativeSlider.Draw(widget.session, widget.props);
}

export function DiscreteSlider(widget: Widgets.WidgetProps<NativeSlider.DiscreteSliderProps>): NativeSlider.SliderDiscreteStep {
    return NativeSlider.DiscreteSlider(widget.session, widget.props);
}
