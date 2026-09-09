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
	key := instanceKey{typeID: (*T)(nil), id: id}
	if r.instances == nil {
		r.instances = make(map[instanceKey]*instanceEntry)
	}
	entry := r.instances[key]
	if entry == nil {
		entry = &instanceEntry{value: new(T)}
		r.instances[key] = entry
	}
	entry.frameSeen = r.frames
	return entry.value.(*T)
}

func (r *runtime) expireInstances() {
	for key, entry := range r.instances {
		if Instance_InstanceExpired(int64(r.frames - entry.frameSeen)) {
			delete(r.instances, key)
		}
	}
}
