package kryon

func (r *runtime) SetPageTitle(title string) {
	r.pageTitle = title
}

func (r *runtime) SetPageDescription(description string) {
	r.pageDescription = description
}

func (r *runtime) SetPageCanonicalURL(url string) {
	r.pageCanonicalURL = url
}

func (r *runtime) SetPageThemeColor(color Color) {
	r.pageThemeColor = color
}

func (r *runtime) GetRoutePath() string {
	if r.routePath == "" {
		return "/"
	}
	return r.routePath
}

func (r *runtime) GetRouteHash() string {
	return r.routeHash
}

func (r *runtime) GetRouteVersion() int32 {
	return r.routeVersion
}

func (r *runtime) PushRoute(path string) {
	r.setRoute(path)
}

func (r *runtime) ReplaceRoute(path string) {
	r.setRoute(path)
}

func (r *runtime) Page(props PageProps) {
	bounds := Layout_LayoutScopeBounds(props.Bounds, r.GetScreenWidth(), r.GetScreenHeight())
	key := props.Key
	if key == 0 {
		key = Key(props.Title)
	}
	style := styleForClassKind(props.ClassName, StyleSheet_StyleKindPage())
	if props.Title != "" {
		r.SetPageTitle(props.Title)
	}
	if props.Description != "" {
		r.SetPageDescription(props.Description)
	}
	if props.CanonicalURL != "" {
		r.SetPageCanonicalURL(props.CanonicalURL)
	}
	if style.Background.A != 0 {
		r.SetPageThemeColor(style.Background)
	}
	r.record(FrameOp{Kind: FrameOpPage, Bounds: bounds, Text: props.Title, Semantic: SemanticPage})
	r.Column(ColumnProps{Bounds: bounds, Gap: styleLength(style.Gap), Padding: styleLength(style.PaddingX), Key: key})
}

func (r *runtime) Section(props SectionProps) {
	bounds := Layout_LayoutScopeBounds(props.Bounds, r.GetScreenWidth(), r.GetScreenHeight())
	key := props.Key
	if key == 0 {
		key = Key(props.Label)
	}
	r.record(FrameOp{Kind: FrameOpSection, Bounds: bounds, Text: props.Label, Semantic: SemanticSection})
	style := styleForClassKind(props.ClassName, StyleSheet_StyleKindSection())
	r.Column(ColumnProps{Bounds: bounds, Gap: styleLength(style.Gap), Padding: styleLength(style.PaddingX), Key: key})
}

func (r *runtime) Heading(props HeadingProps) {
	level := props.Level
	if level < 1 {
		level = 1
	} else if level > 6 {
		level = 6
	}
	style := defaultTextStyleForClassKind(Text24, props.ClassName, StyleSheet_StyleKindHeading())
	font, fontID := styleTextFace(style, Text24)
	color := style.Foreground
	bounds := props.Bounds
	if bounds.Width <= 0 {
		bounds.Width = float32(runtimeTextWidthWithFont(props.Text, font, fontID))
	}
	if bounds.Height <= 0 {
		bounds.Height = float32(font)
	}
	bounds = r.layoutRect(bounds)
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: props.Text, Color: color, Opacity: style.Opacity, FontSize: font, FontID: fontID, ID: int32(props.Key), Semantic: SemanticHeading, Level: level})
}

func (r *runtime) ParagraphText(props ParagraphTextProps) {
	style := defaultTextStyleForClassKind(Text16, props.ClassName, StyleSheet_StyleKindParagraphText())
	font, fontID := styleTextFace(style, Text16)
	color := style.Foreground
	lineGap := int32(4)
	if style.Gap > 0 {
		lineGap = int32(style.Gap + 0.5)
	}
	width := int32(props.Bounds.Width)
	if width <= 0 {
		width = r.GetScreenWidth() - int32(props.Bounds.X)
	}
	measure := func(text string) int { return runtimeTextWidthWithFont(text, font, fontID) }
	lines := layoutTextLines(props.Text, float32(width), measure)
	lineHeight := textHeight(font, fontID)
	height := Paragraph_ParagraphLayoutTotalHeight(int32(len(lines)), lineHeight, lineGap)
	bounds := r.layoutRect(Rectangle{X: props.Bounds.X, Y: props.Bounds.Y, Width: float32(width), Height: float32(height)})
	y := int32(bounds.Y)
	for index, line := range lines {
		r.record(FrameOp{Kind: FrameOpText,
			Bounds: Rectangle{X: bounds.X, Y: float32(y), Width: float32(measure(line)), Height: float32(lineHeight)},
			Text:   line, Color: color, Opacity: style.Opacity, FontSize: font, FontID: fontID,
			ID: int32(props.Key), Semantic: SemanticParagraph})
		y = Paragraph_ParagraphNextLineY(y, lineHeight, lineGap, index+1 < len(lines))
	}
}

func (r *runtime) Link(props LinkProps) bool {
	state := ButtonStateNormal
	if props.Disabled {
		state = ButtonStateDisabled
	}
	frame := simpleStyleFrameWithClassRole(ButtonToneNeutral, state, props.Disabled, false,
		props.ClassName, StyleSheet_StyleKindLink(), StyleSheet_StyleAny())
	linkStyle := unpackStyle(frame.Value)
	font, fontID := styleTextFace(linkStyle, Text16)
	bounds := r.layoutRect(props.Bounds)
	bounds = Link_LinkBoundsFor(bounds,
		int32(runtimeTextWidthWithFont(props.Text, font, fontID)),
		textHeight(font, fontID), font)
	pressed := false
	if !props.Disabled {
		pressed = r.consumeTap(bounds)
	}
	appearance := Link_ResolveLinkAppearance(frame, false, props.Disabled)
	color := unpackRGBA(appearance.Color)
	r.record(FrameOp{Kind: FrameOpText, Bounds: bounds, Text: props.Text, Color: color, Opacity: linkStyle.Opacity, FontSize: font, FontID: fontID, FocusID: props.FocusID, Disabled: props.Disabled, Pressed: pressed, Semantic: SemanticLink, Link: props.Link, Role: "link"})
	return pressed
}
