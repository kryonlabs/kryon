package kryon

import "testing"

func TestInstanceOwnershipAndLifetime(t *testing.T) {
	type counter struct{ value int }
	type other struct{ value int }
	first := New(AppConfig{}).(*runtime)
	second := New(AppConfig{}).(*runtime)
	value := instanceState[counter](first, 17)
	value.value = 42
	if instanceState[counter](second, 17).value != 0 || instanceState[other](first, 17).value != 0 {
		t.Fatal("state leaked between render hosts or declarations")
	}
	for key := uint64(0); key < 1536; key++ {
		instanceState[counter](first, key+(1<<40)).value = int(key)
	}
	for key := uint64(1536); key > 0; key-- {
		if instanceState[counter](first, key-1+(1<<40)).value != int(key-1) {
			t.Fatal("reordered keys lost their state")
		}
	}
	if instanceState[counter](first, 17) != value || value.value != 42 {
		t.Fatal("growing the store moved or overwrote a live instance")
	}
	first.frames = 12
	first.expireInstances()
	if instanceState[counter](first, 17) != value {
		t.Fatal("state expired at the retention boundary")
	}
	first.frames = 25
	first.expireInstances()
	if instanceState[counter](first, 17).value != 0 {
		t.Fatal("removed state did not expire")
	}
	secondValue := instanceState[counter](second, 17)
	secondValue.value = 7
	first.frames = 100
	first.expireInstances()
	if instanceState[counter](second, 17) != secondValue || secondValue.value != 7 {
		t.Fatal("another host's frames expired a live instance")
	}
	second.Close()
	if len(second.instances) != 0 {
		t.Fatal("closed host retained instance storage")
	}
}
