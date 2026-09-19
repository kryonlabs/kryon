package kryon

import (
	"time"
)

func (r *runtime) Toast(props ToastProps) {
	metrics := Toast_ToastMetricsFor(1, StyleFrame{})
	if props.Message == "" {
		r.toastMessage = ""
		r.toastClassName = 0
		r.toastUntil = time.Time{}
		return
	}
	r.toastMessage = props.Message
	r.toastClassName = props.ClassName
	duration := Toast_ToastDuration(float32(props.Seconds), metrics)
	r.toastUntil = time.Now().Add(time.Duration(float64(duration) * float64(time.Second)))
}

func (r *runtime) recordToast() {
	if r.toastMessage == "" || time.Now().After(r.toastUntil) {
		r.toastMessage = ""
		return
	}
	surfaceFrame := StyleFrame{
		Value: ResolveActiveStyle(StyleData{}, Toast_ToastSurfaceFactsFor(r.toastClassName),
			int32(ButtonStateNormal)),
	}
	labelFrame := StyleFrame{
		Value: ResolveActiveStyle(StyleData{}, Toast_ToastLabelFactsFor(r.toastClassName),
			int32(ButtonStateNormal)),
	}
	surface := unpackStyle(surfaceFrame.Value)
	label := unpackStyle(labelFrame.Value)
	metrics := Toast_ToastMetricsFor(1, surfaceFrame)
	labelFont, labelFontID := styleTextFace(label, Text14)
	textWidth := int32(runtimeTextWidthWithFont(r.toastMessage, labelFont, labelFontID))
	layout := Toast_ToastLayoutFor(r.GetScreenWidth(), r.GetScreenHeight(), textWidth, labelFont, metrics)
	r.record(FrameOp{Kind: FrameOpRect, Bounds: layout.Bounds, Color: surface.Background, BorderColor: surface.Border, Radius: surface.Radius, BorderWidth: surface.BorderWidth, Material: MaterialKind(surface.Material), Opacity: surface.Opacity})
	r.record(FrameOp{Kind: FrameOpText, Bounds: layout.TextBounds, Text: r.toastMessage, Color: label.Foreground, Opacity: label.Opacity, FontSize: labelFont, FontID: labelFontID})
}
