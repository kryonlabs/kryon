package kryon

// Storage belongs to the render host; the declaration supplies its state type.
type instanceKey struct {
	typeID any
	id     uint64
}

type instanceEntry struct {
	value     any
	frameSeen int
}

func instanceState[T any](r *runtime, id uint64) *T {
	return r.InstanceValue((*T)(nil), id, func() any { return new(T) }).(*T)
}

// InstanceState binds a generated declaration to the active render host.
func InstanceState[T any](id uint64) *T {
	return active().InstanceValue((*T)(nil), id, func() any { return new(T) }).(*T)
}

func (r *runtime) InstanceValue(typeID any, id uint64, create func() any) any {
	key := instanceKey{typeID: typeID, id: id}
	if r.instances == nil {
		r.instances = make(map[instanceKey]*instanceEntry)
	}
	entry := r.instances[key]
	if entry == nil {
		entry = &instanceEntry{value: create()}
		r.instances[key] = entry
	}
	entry.frameSeen = r.frames
	return entry.value
}

func (r *runtime) expireInstances() {
	for key, entry := range r.instances {
		if Instance_InstanceExpired(int64(r.frames - entry.frameSeen)) {
			delete(r.instances, key)
		}
	}
}
