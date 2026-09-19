package kryon

import (
	"time"
)

func (r *runtime) Button(props ButtonProps) bool {
	if props.Arrow {
		props.Label = string(rune(Button_ButtonArrowGlyph(props.Direction)))
		if props.Size == ControlSizeMedium {
			props.Size = ControlSizeSmall
		}
	}
	if props.Info {
		props.Label = "i"
		props.Circle = true
		props.IconOnly = false
		if props.Size == ControlSizeMedium {
			props.Size = ControlSizeSmall
		}
		if props.Bounds.Width <= 0 {
			props.Bounds.Width = 18
		}
		if props.Bounds.Height <= 0 {
			props.Bounds.Height = props.Bounds.Width
		}
	}
	if props.Split {
		activated := int32(0)
		if props.ActivatedID == nil {
			props.ActivatedID = &activated
		}
		*props.ActivatedID = 0
		open := int32(0)
		if props.Open == nil {
			props.Open = &open
		}
		action := r.resolveButtonProps(props)
		menuID := action.ID + 1
		if action.ID == 0 {
			action.ID = r.resolveFocusID(0)
			menuID = r.resolveFocusID(0)
		}
		layout := Button_ButtonResolveSplitLayout(action.Bounds.Width, action.Bounds.Height)
		fullBounds := action.Bounds
		fullBounds.Width = layout.Width
		action.Bounds.Width = layout.ActionWidth
		menu := action
		menu.Bounds = Rectangle{X: fullBounds.X + layout.MenuOffset,
			Y: action.Bounds.Y, Width: layout.MenuWidth, Height: action.Bounds.Height}
		menu.Label = "Open menu"
		menu.ID = menuID
		menu.IconType = IconNone
		menu.IconOnly = true
		menu.Square = true
		menu.Menu = false
		menu.Split = false
		clicked := r.surfaceButtonAt(action, fullBounds, false)
		*props.Open = boolInt(Button_ButtonToggleMenuOpen(*props.Open != 0, r.surfaceButtonAt(menu, fullBounds, true)))
		divider := unpackStyle(simpleStyleFrameWithClassRole(ButtonToneNeutral, ButtonStateNormal, false, false,
			props.ClassName, StyleSheet_StyleKindButton(), StyleSheet_StyleAny()).Value)
		r.record(FrameOp{Kind: FrameOpLine,
			Bounds: Rectangle{X: menu.Bounds.X, Y: menu.Bounds.Y + layout.DividerInset,
				Height: menu.Bounds.Height - 2*layout.DividerInset}, Color: divider.Border})
		if *props.Open != 0 {
			*props.ActivatedID = r.Menu(MenuProps{
				ID: props.MenuID, ClassName: props.ClassName, Mode: MenuModePopup,
				Bounds:    NewRectangle(fullBounds.X, fullBounds.Y+fullBounds.Height, 0, 0),
				Items:     props.Items,
				ItemCount: props.ItemCount,
			}).ActivatedID
			*props.Open = boolInt(Button_ButtonCloseMenuAfterActivation(*props.Open != 0,
				*props.ActivatedID))
		}
		return clicked
	}
	if props.Menu {
		activated := int32(0)
		if props.ActivatedID == nil {
			props.ActivatedID = &activated
		}
		*props.ActivatedID = 0
		open := int32(0)
		if props.Open == nil {
			props.Open = &open
		}
		props.IconType = IconNone
		props.IconPlacement = IconPlacementTrailing
		props = r.resolveSurfaceButtonProps(props, true)
		props.Bounds = r.layoutRect(props.Bounds)
		*props.Open = boolInt(Button_ButtonToggleMenuOpen(*props.Open != 0,
			r.surfaceButtonAt(props, Rectangle{}, true)))
		if *props.Open == 0 {
			return false
		}
		*props.ActivatedID = r.Menu(MenuProps{
			ID: props.MenuID, ClassName: props.ClassName, Mode: MenuModePopup,
			Bounds:    NewRectangle(props.Bounds.X, props.Bounds.Y+props.Bounds.Height, 0, 0),
			Items:     props.Items,
			ItemCount: props.ItemCount,
		}).ActivatedID
		*props.Open = boolInt(Button_ButtonCloseMenuAfterActivation(*props.Open != 0, *props.ActivatedID))
		return *props.ActivatedID != 0
	}
	props = r.resolveButtonProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	return r.buttonAt(props)
}

func cardButtonProps(props CardProps) ButtonProps {
	return ButtonProps{
		Bounds:    props.Bounds,
		ID:        props.ID,
		ClassName: props.ClassName,
		Disabled:  props.Disabled,
		Selected:  props.Selected,
		State:     props.State,
	}
}

func (r *runtime) Card(props CardProps) bool {
	button := r.resolveSurfaceButtonPropsForKind(cardButtonProps(props), false, StyleSheet_StyleKindCard())
	button.Bounds = r.layoutRect(button.Bounds)
	button.Label = ""
	if !props.Clickable {
		r.prepareAccessibility(props.ID, int32(WidgetKindCard), false)
		frame, _ := r.surfaceButtonFrameForKind(button, Rectangle{}, false, StyleSheet_StyleKindCard())
		frame.Role = "group"
		r.record(frame)
		return false
	}
	frame, pressed := r.surfaceButtonFrameForKind(button, Rectangle{}, false, StyleSheet_StyleKindCard())
	r.record(frame)
	return pressed
}

func (r *runtime) CardScope(props CardProps) {
	if !props.Clickable {
		r.prepareAccessibility(props.ID, int32(WidgetKindCard), false)
	}
	button := r.resolveSurfaceButtonPropsForKind(cardButtonProps(props), false, StyleSheet_StyleKindCard())
	button.Bounds = r.layoutRect(button.Bounds)
	button.Label = ""
	frame, _ := r.surfaceButtonFrameForKind(button, Rectangle{}, false, StyleSheet_StyleKindCard())
	if !props.Clickable {
		frame.Role = "group"
	}
	r.record(frame)
	owner := 0
	if props.Clickable {
		owner = len(r.ops)
	}
	r.layout = append(r.layout, layoutFrame{
		accessibilityOwner:     owner,
		accessibilityContainer: len(r.ops),
		bounds:                 frame.Button.ContentBounds,
		center:                 true, textFont: frame.Button.Font,
		textColor: unpackRGBA(frame.Button.Foreground), textColorSet: true,
		textDisabled: frame.Disabled,
	})
}

func (r *runtime) ButtonScope(props ButtonProps) {
	label := props.Label
	props = r.resolveButtonProps(props)
	props.Bounds = r.layoutRect(props.Bounds)
	props.Label = ""
	frame, _ := r.surfaceButtonFrame(props, Rectangle{}, false)
	r.record(frame)
	r.layout = append(r.layout, layoutFrame{
		accessibilityOwner:     len(r.ops),
		accessibilityContainer: len(r.ops),
		bounds:                 frame.Button.ContentBounds,
		center:                 true, textFont: frame.Button.Font,
		textColor: unpackRGBA(frame.Button.Foreground), textColorSet: true,
		textDisabled: frame.Disabled,
	})
	if label != "" {
		r.Text(TextProps{Text: label, Wrap: TextWrapNone})
	}
}

func boolInt(value bool) int32 {
	if value {
		return 1
	}
	return 0
}

// buttonAt applies the canonical Button interaction and paint contract to an
// already-laid-out rectangle. Composite widgets use it for embedded buttons
// without advancing their parent's layout a second time.
func (r *runtime) buttonAt(props ButtonProps) bool {
	return r.surfaceButtonAt(props, Rectangle{}, false)
}

func (r *runtime) surfaceButtonAt(props ButtonProps, surfaceBounds Rectangle, disclosure bool) bool {
	frame, pressed := r.surfaceButtonFrame(props, surfaceBounds, disclosure)
	if props.Invisible {
		return pressed
	}
	if props.Swatch {
		frame.Color = props.SwatchColor
	}
	r.record(frame)
	if image, ok := buttonImageProps(frame.Button.Props); ok {
		tint := unpackRGBA(frame.Button.Foreground)
		if tint.A == 0 {
			tint = White
		}
		op := imageOperation(image, tint)
		op.Disabled = frame.Disabled
		r.record(op)
	}
	return pressed
}

func buttonImageProps(props ButtonProps) (ImageProps, bool) {
	if props.ImageAssetPath == "" && props.ImageBounds.Width <= 0 && props.ImageBounds.Height <= 0 {
		return ImageProps{}, false
	}
	bounds := props.ImageBounds
	if bounds.Width <= 0 && bounds.Height <= 0 {
		bounds = props.Bounds
	}
	return ImageProps{
		AssetPath: props.ImageAssetPath,
		Bounds:    bounds,
		Source:    props.ImageSource,
		Origin:    props.ImageOrigin,
		Rotation:  props.ImageRotation,
		Fit:       ImageFit(props.ImageFit),
	}, true
}

// Resolve input and animation once. A composed button uses this same frame
// for its surface and inherited content instead of resolving a static style.
func (r *runtime) surfaceButtonFrame(props ButtonProps, surfaceBounds Rectangle, disclosure bool) (FrameOp, bool) {
	return r.surfaceButtonFrameForKind(props, surfaceBounds, disclosure, StyleSheet_StyleKindButton())
}

func (r *runtime) surfaceButtonFrameForKind(props ButtonProps, surfaceBounds Rectangle, disclosure bool, styleKind int32) (FrameOp, bool) {
	return r.surfaceButtonFrameForRoleKind(props, surfaceBounds, disclosure, styleKind, StyleSheet_StyleAny())
}

func (r *runtime) surfaceButtonFrameForRoleKind(props ButtonProps, surfaceBounds Rectangle, disclosure bool, styleKind int32, role int32) (FrameOp, bool) {
	props = r.resolveSurfaceButtonPropsForRoleKind(props, disclosure, styleKind, role)
	props.ID = r.resolveFocusID(props.ID)
	kind := int32(WidgetKindButton)
	if styleKind == StyleSheet_StyleKindCard() {
		kind = int32(WidgetKindCard)
	}
	flags := Style_ResolveFlags(int32(props.State), props.Disabled, props.Loading, props.Selected)
	r.prepareAccessibility(props.ID, kind, !flags.Disabled && !flags.Loading)
	input := r.Button_ReadButtonInput(props.Bounds, props.ID, int32(props.State),
		props.Disabled, props.Loading, props.Selected)
	metrics := r.themeMetrics()
	props.Disabled = input.Flags.Disabled
	props.Loading = input.Flags.Loading
	props.Selected = input.Flags.Selected
	motion := r.Button_AdvanceButtonMotion(uint64(uint32(props.ID)),
		int32(props.State), input, Surface_DefaultMotionEnabled(),
		r.frameDeltaMS, metrics.TransitionNormalMS, metrics.TransitionFastMS)
	appearance := resolveMinimalControlRoleFrame(props, ButtonState(input.Interaction.State),
		props.State == ButtonStateAuto, motion.Hover.Value, motion.Press.Value,
		motion.Focus.Value, styleKind, role)
	if styleKind == StyleSheet_StyleKindModal() && role == Modal_ModalActionRole() {
		appearance = Modal_ModalActionButtonFrame(appearance)
	}
	resolved := Button_BuildFrame(props, input, appearance, motion,
		surfaceBounds, packRGBA(r.appAmbientColor()), 1,
		int32(appearance.Value.FontSize), Text16)
	frame := FrameOp{Kind: FrameOpButton, Button: resolved,
		Bounds: resolved.Props.Bounds, SurfaceBounds: surfaceBounds, Text: resolved.Props.Label, ID: resolved.Props.ID,
		FontID:     registeredTypeface(resolved.Appearance.Value.Typeface),
		Disclosure: disclosure, ElapsedMS: float64(r.elapsedTime) / float64(time.Millisecond),
		Disabled: resolved.Props.Disabled, Loading: resolved.Props.Loading, Pressed: input.Interaction.Pressed,
		accessibilityKind: kind,
		Focused:           input.Interaction.Focused, Hovered: input.Interaction.Hovered}
	return frame, input.Activated
}

func (r *runtime) resolveFocusID(id int32) int32 {
	if id != 0 {
		return id
	}
	id = r.autoFocusID
	r.autoFocusID++
	return id
}

func (r *runtime) resolveButtonProps(props ButtonProps) ButtonProps {
	return r.resolveSurfaceButtonProps(props, false)
}

func (r *runtime) resolveSurfaceButtonProps(props ButtonProps, disclosure bool) ButtonProps {
	return r.resolveSurfaceButtonPropsForKind(props, disclosure, StyleSheet_StyleKindButton())
}

func (r *runtime) resolveSurfaceButtonPropsForKind(props ButtonProps, disclosure bool, styleKind int32) ButtonProps {
	return r.resolveSurfaceButtonPropsForRoleKind(props, disclosure, styleKind, StyleSheet_StyleAny())
}

func (r *runtime) resolveSurfaceButtonPropsForRoleKind(props ButtonProps, disclosure bool, styleKind int32, role int32) ButtonProps {
	props.Disabled = props.Disabled || r.contentDisabled()
	style := unpackStyle(resolveMinimalControlRoleState(props, props.State, styleKind, role))
	font := Style_ResolveFont(0, int32(style.FontSize), Text16)
	metrics := defaultThemeMetrics()
	if r.activeTheme != nil {
		metrics = r.activeTheme.Metrics
	}
	height := Style_SizeValue(int32(props.Size), metrics.ControlHeightSmall,
		metrics.ControlHeightMedium, metrics.ControlHeightLarge)
	availableWidth := float32(0)
	if props.FullWidth && props.Bounds.Width <= 0 {
		right := float32(r.GetScreenWidth())
		if len(r.layout) > 0 {
			parent := r.layout[len(r.layout)-1].bounds
			if parent.Width > 0 {
				right = parent.X + parent.Width
			}
		}
		availableWidth = right - props.Bounds.X
	}
	props.Bounds = r.Button_MeasureButton(props, style, height, font, availableWidth, 1, disclosure)
	return props
}

func (r *runtime) MeasureTextWidth(text string, font int32, typeface string) int32 {
	return int32(runtimeTextWidthWithFont(text, font, registeredTypeface(typeface)))
}

func resolveButtonStyle(theme themePalette, dark bool, active *Theme, props ButtonProps, state ButtonState) Style {
	return unpackStyle(resolveButtonFrame(theme, dark, active, props, state, false, 0, 0, 0).Value)
}

func resolveButtonStyleForKind(theme themePalette, dark bool, active *Theme, props ButtonProps, state ButtonState, styleKind int32) Style {
	return unpackStyle(resolveButtonFrameForKind(theme, dark, active, props, state, false, 0, 0, 0, styleKind).Value)
}

func resolveButtonFrame(theme themePalette, dark bool, active *Theme, props ButtonProps, state ButtonState,
	automatic bool, hover, press, focusAmount float32) StyleFrame {
	return resolveButtonFrameForKind(theme, dark, active, props, state, automatic,
		hover, press, focusAmount, StyleSheet_StyleKindButton())
}

func resolveButtonFrameForKind(theme themePalette, dark bool, active *Theme, props ButtonProps, state ButtonState,
	automatic bool, hover, press, focusAmount float32, styleKind int32) StyleFrame {
	if styleKind == 0 {
		styleKind = StyleSheet_StyleKindButton()
	}
	return resolveMinimalControlFrame(props, state, automatic, hover, press,
		focusAmount, styleKind)
}

func resolveMinimalControlState(props ButtonProps, state ButtonState, styleKind int32) StyleData {
	return resolveMinimalControlRoleState(props, state, styleKind, StyleSheet_StyleAny())
}

// Generic control style resolves from a zero base; packs own every visual
// value. Opacity composes as declared-or-visible so absent opacity never
// hides content, while field bits keep declared-ness for explicit zeros.
func resolveMinimalControlRoleState(props ButtonProps, state ButtonState, styleKind int32, role int32) StyleData {
	facts := Button_ButtonRoleFactsFor(styleKind, props.ID, props.ClassName,
		role, int32(props.Tone), int32(props.Emphasis), int32(props.Size), int32(state))
	value := ResolveActiveStyle(StyleData{}, facts, int32(state))
	value.Opacity = Style_StyleOpacityValue(value.Fields, value.Opacity)
	return value
}

func resolveMinimalControlFrame(props ButtonProps, state ButtonState, automatic bool,
	hover, press, focusAmount float32, styleKind int32) StyleFrame {
	return resolveMinimalControlRoleFrame(props, state, automatic, hover, press,
		focusAmount, styleKind, StyleSheet_StyleAny())
}

func resolveMinimalControlRoleFrame(props ButtonProps, state ButtonState, automatic bool,
	hover, press, focusAmount float32, styleKind int32, role int32) StyleFrame {
	flags := Style_ResolveFlags(int32(props.State), props.Disabled, props.Loading, props.Selected)
	frame := StyleFrame{Value: resolveMinimalControlRoleState(props, state, styleKind, role)}
	frame.Fill = Surface_FillState(frame.Value.Fields, frame.Value.Background, frame.Value.BackgroundEnd)
	if automatic && !flags.Disabled && !flags.Loading && !flags.Selected &&
		(hover > 0 || press > 0 || focusAmount > 0) {
		normal := frame.Value
		if state != ButtonStateNormal {
			normal = resolveMinimalControlRoleState(props, ButtonStateNormal, styleKind, role)
		}
		hoverStyle, pressStyle, focusStyle := normal, normal, normal
		if hover > 0 {
			hoverStyle = resolveMinimalControlRoleState(props, ButtonStateHover, styleKind, role)
		}
		if press > 0 {
			pressStyle = resolveMinimalControlRoleState(props, ButtonStatePressed, styleKind, role)
		}
		if focusAmount > 0 {
			focusStyle = resolveMinimalControlRoleState(props, ButtonStateFocus, styleKind, role)
		}
		frame = Style_TransitionFrame(frame.Value, normal, hoverStyle,
			pressStyle, focusStyle, hover, press, focusAmount)
	}
	return frame
}
