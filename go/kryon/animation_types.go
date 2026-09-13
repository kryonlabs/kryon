package kryon

const (
	AnimationTracksMax = 8
	AnimationKeysMax   = 64
	AnimationNameMax   = 48
)

type NodeId int32

type AnimInterp int32

const (
	AnimInterpLinear AnimInterp = 0
	AnimInterpStep   AnimInterp = 1
)

type Keyframe struct {
	Time  float32
	Value float32
}

type AnimTrack struct {
	Target        NodeId
	Property      [32]byte
	Component     int32
	Interp        AnimInterp
	KeyframeCount int32
	Keyframes     [AnimationKeysMax]Keyframe
}

type Animation struct {
	Name       [AnimationNameMax]byte
	Duration   float32
	Loop       int32
	TrackCount int32
	Tracks     [AnimationTracksMax]AnimTrack
}
