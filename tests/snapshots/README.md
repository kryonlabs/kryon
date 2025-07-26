# Snapshot Tests

This directory contains snapshot files for visual regression testing.

Snapshots are generated using the `insta` crate and the Ratatui backend as the source of truth.

## Workflow

1. Run tests: `cargo test -p kryon-tests`
2. Review changes: `cargo insta review`
3. Accept/reject snapshots interactively

## Snapshot Files

- `*.snap` - Generated snapshot files
- `*.snap.new` - New snapshots pending review

These files are committed to version control to track visual changes over time.