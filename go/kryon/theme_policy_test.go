package kryon

import (
	"math"
	"os"
	"strconv"
	"strings"
	"testing"
)

func TestThemeSchemeFixtures(t *testing.T) {
	data, err := os.ReadFile("../../tests/fixtures/theme/scheme.txt")
	if err != nil {
		t.Fatal(err)
	}
	rows := 0
	for _, line := range strings.Split(string(data), "\n") {
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		fields := strings.Fields(line)
		if len(fields) != 21 {
			t.Fatalf("scheme fixture has %d fields, want 21", len(fields))
		}
		var values [21]uint32
		for i, field := range fields {
			value, err := strconv.ParseUint(field, 16, 32)
			if err != nil {
				t.Fatal(err)
			}
			values[i] = uint32(value)
		}
		roles := Theme_SchemeFor(values[0], values[1], values[2], values[3],
			values[4], values[5] != 0, uint8(values[6]))
		actual := []uint32{
			roles.Primary, roles.OnPrimary, roles.Secondary, roles.OnSecondary,
			roles.Surface, roles.OnSurface, roles.SurfaceContainer,
			roles.SurfaceVariant, roles.OnSurfaceVariant, roles.Outline,
			roles.Error, roles.OnError, roles.DisabledContainer, roles.DisabledContent,
		}
		for i, color := range actual {
			if color != values[i+7] {
				t.Errorf("scheme row %d role %d: %08x != %08x", rows+1, i, color, values[i+7])
			}
		}
		rows++
	}
	if rows != 2 {
		t.Fatalf("read %d fixture rows, want 2", rows)
	}
}

func TestThemeToneLimits(t *testing.T) {
	for _, test := range []struct {
		lightDelta int32
		darkDelta  int32
		dark       bool
		want       uint32
	}{
		{0, 0, false, 0x12345678},
		{math.MinInt32, 0, false, 0xffffff78},
		{math.MaxInt32, 0, false, 0x00000078},
		{0, math.MinInt32, true, 0x00000078},
		{0, math.MaxInt32, true, 0xffffff78},
	} {
		if got := Theme_ToneFor(0x12345678, test.lightDelta, test.darkDelta, test.dark); got != test.want {
			t.Errorf("ToneFor(%d, %d, %t) = %08x, want %08x",
				test.lightDelta, test.darkDelta, test.dark, got, test.want)
		}
	}
}
