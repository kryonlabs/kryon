package records

import "testing"

func TestRecordValues(t *testing.T) {
	if result := RecordValues_Check(); result != 0 {
		t.Fatalf("record value semantics failed at check %d", result)
	}
}
