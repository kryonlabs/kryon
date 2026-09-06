package kryon

// Paint layers contain ordinary FrameOps. Reserving their position at begin,
// rather than end, keeps a nested popup above all of its parent's content.
// Ownership is runtime-local; the popup controller decides whether an owner
// is still visible when the frame is completed.
type paintLayer struct {
	owner  int32
	parent int
	ops    []FrameOp
}

type paintLayerToken struct {
	runtime *runtime
	frame   uint64
	index   int
}

type paintLayerScope struct {
	token         paintLayerToken
	start         int
	layout        []layoutFrame
	clips         []Rectangle
	disabledStack []bool
	disabledCount int32
}

func (r *runtime) resetPaintLayers() {
	if len(r.paintLayerScopes) != 0 {
		panic("unclosed paint layer at frame boundary")
	}
	clear(r.paintLayers)
	r.paintLayers = r.paintLayers[:0]
	r.paintLayerFrame++
}

func (r *runtime) beginPaintLayer(owner int32) paintLayerToken {
	parent := -1
	if n := len(r.paintLayerScopes); n != 0 {
		parent = r.paintLayerScopes[n-1].token.index
	}
	token := paintLayerToken{runtime: r, frame: r.paintLayerFrame, index: len(r.paintLayers)}
	r.paintLayers = append(r.paintLayers, paintLayer{owner: owner, parent: parent})
	r.paintLayerScopes = append(r.paintLayerScopes, paintLayerScope{
		token: token, start: len(r.ops), layout: r.layout, clips: r.scrollClips,
		disabledStack: r.disabledStack, disabledCount: r.disabledCount,
	})
	// Popup children lay out independently and escape the owner's scrolling
	// clip. Disabled content is inherited but its stack is isolated.
	r.layout, r.scrollClips, r.disabledStack = nil, nil, nil
	return token
}

func (r *runtime) endPaintLayer(token paintLayerToken) {
	n := len(r.paintLayerScopes)
	if n == 0 || r.paintLayerScopes[n-1].token != token || token.frame != r.paintLayerFrame {
		panic("paint layers must close in their opening frame and stack order")
	}
	scope := r.paintLayerScopes[n-1]
	r.paintLayers[token.index].ops = append([]FrameOp(nil), r.ops[scope.start:]...)
	clear(r.ops[scope.start:])
	r.ops = r.ops[:scope.start]
	r.layout, r.scrollClips = scope.layout, scope.clips
	r.disabledStack, r.disabledCount = scope.disabledStack, scope.disabledCount
	r.paintLayerScopes[n-1] = paintLayerScope{}
	r.paintLayerScopes = r.paintLayerScopes[:n-1]
}

func (r *runtime) appendPaintLayers(visible func(int32) bool) {
	if len(r.paintLayerScopes) != 0 {
		panic("cannot paint an unclosed layer")
	}
	shown := make([]bool, len(r.paintLayers))
	for i, layer := range r.paintLayers {
		shown[i] = (layer.parent < 0 || shown[layer.parent]) && visible(layer.owner)
		if shown[i] {
			r.ops = append(r.ops, layer.ops...)
		}
	}
}
