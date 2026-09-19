package kryon

import (
	"strings"
)

func (r *runtime) Column(props ColumnProps) {
	r.pushLayout(props, false, FrameOpColumn)
}

func (r *runtime) Row(props ColumnProps) {
	r.pushLayout(props, true, FrameOpRow)
}

func (r *runtime) Group(props ColumnProps) {
	r.pushGroup(props, FrameOpGroup)
}

func (r *runtime) Stack(props ColumnProps) {
	r.pushLayout(props, false, FrameOpStack)
}

func (r *runtime) Screen(props ColumnProps) {
	r.pushGroup(props, FrameOpScreen)
}

func (r *runtime) Grid(props GridProps) {
	r.pushGrid(props)
}

func (r *runtime) Flow(props FlowProps) {
	r.Row(ColumnProps(props))
}

func (r *runtime) End() {
	if len(r.layout) > 0 {
		r.layout = r.layout[:len(r.layout)-1]
	}
	r.record(FrameOp{Kind: FrameOpEnd})
}

func (r *runtime) pushLayout(props ColumnProps, horizontal bool, kind FrameOpKind) {
	bounds := r.layoutRect(props.Bounds)
	metrics := Layout_LayoutMetricsFor(bounds, props.Gap, props.Padding)
	r.layout = append(r.layout, layoutFrame{
		bounds:     bounds,
		cursorX:    metrics.Content.X,
		cursorY:    metrics.Content.Y,
		gap:        float32(metrics.Gap),
		padding:    float32(metrics.Padding),
		horizontal: horizontal,
	})
	r.record(FrameOp{Kind: kind, Bounds: bounds, ID: int32(props.Key)})
}

func (r *runtime) pushGrid(props GridProps) {
	bounds := r.layoutRect(props.Bounds)
	props.Bounds = bounds
	cursor := Grid_BeginGridCursor(props)
	r.layout = append(r.layout, layoutFrame{
		bounds:     bounds,
		gridCursor: cursor,
	})
	r.record(FrameOp{Kind: FrameOpGrid, Bounds: bounds, ID: int32(props.Key), Columns: cursor.Metrics.Columns})
}

func (r *runtime) pushGroup(props ColumnProps, kind FrameOpKind) {
	policy := Group_GroupPolicyFor(props.Bounds, props.Gap, props.Padding)
	bounds := policy.Bounds
	gap := policy.Gap
	padding := policy.Padding
	if kind == FrameOpScreen {
		policy = Group_ScreenGroupPolicyFor(props.Bounds, r.GetScreenWidth(), r.GetScreenHeight(), props.Gap, props.Padding)
		bounds = policy.Bounds
		gap = policy.Gap
		padding = policy.Padding
	}
	r.layout = append(r.layout, layoutFrame{
		bounds:   bounds,
		gap:      float32(gap),
		padding:  float32(padding),
		noLayout: true,
	})
	r.record(FrameOp{Kind: kind, Bounds: bounds, ID: int32(props.Key)})
}

func (r *runtime) layoutRect(bounds Rectangle) Rectangle {
	if len(r.layout) == 0 {
		return bounds
	}
	frame := &r.layout[len(r.layout)-1]
	if frame.noLayout {
		return bounds
	}
	if frame.center {
		return Style_CenterChild(bounds, bounds, frame.bounds)
	}
	if bounds.X != 0 || bounds.Y != 0 {
		return bounds
	}
	if frame.gridCursor.Metrics.Columns > 0 {
		height := int32(bounds.Height)
		if height <= 0 {
			height = int32(frame.gridCursor.Metrics.Content.Height)
		}
		frame.gridCursor = Grid_GridStep(frame.gridCursor, height, 1)
		return frame.gridCursor.Item
	}
	out := bounds
	metrics := Layout_LayoutMetricsFor(frame.bounds, int32(frame.gap), int32(frame.padding))
	cursor := frame.cursorY
	if frame.horizontal {
		cursor = frame.cursorX
	}
	out = Layout_LayoutChildBounds(bounds, out, metrics, frame.horizontal, cursor)
	if frame.horizontal {
		frame.cursorX += out.Width + frame.gap
	} else {
		frame.cursorY += out.Height + frame.gap
	}
	return out
}

func (r *runtime) setRoute(path string) {
	oldPath := r.GetRoutePath()
	oldHash := r.GetRouteHash()
	if path == "" {
		r.routePath = "/"
		r.routeHash = ""
	} else if idx := strings.Index(path, "#"); idx >= 0 {
		r.routePath = path[:idx]
		r.routeHash = path[idx:]
	} else {
		r.routePath = path
		r.routeHash = ""
	}
	if r.routePath == "" {
		r.routePath = "/"
	}
	if r.routePath != oldPath || r.routeHash != oldHash {
		r.routeVersion++
	}
}
