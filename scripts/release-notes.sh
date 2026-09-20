#!/bin/sh
# Promote curated "## Unreleased" CHANGELOG notes into a dated release entry.
#
# Kryon release notes must be user-facing. Maintainers accumulate notes under
# a single "## Unreleased" heading; this script turns every such block into a
# "## <tag> - <date>" entry and resets one empty "## Unreleased" at the top.
# It never synthesizes entries from commit subjects.
#
# Usage:
#   release-notes.sh --check [changelog]   exit 0 when curated notes exist
#   release-notes.sh <tag> [changelog]     promote notes to <tag>

set -eu

check=0
case "${1:-}" in
    --check)
        check=1
        shift
        ;;
esac

changelog="${2:-CHANGELOG.md}"

if [ "$check" -eq 1 ]; then
    awk '
        /^## Unreleased[[:space:]]*$/ { in_unrel = 1; next }
        in_unrel && /^## / { exit }
        in_unrel && /^- / { found = 1 }
        END { exit (found ? 0 : 1) }
    ' "${1:-CHANGELOG.md}"
    exit
fi

tag="${1:-}"
if [ -z "$tag" ]; then
    echo "usage: $0 [--check] vX.Y.Z [changelog]" >&2
    exit 2
fi

date="$(date +%Y-%m-%d)"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT HUP INT TERM

awk -v tag="$tag" -v date="$date" '
    BEGIN { in_unrel = 0; unrel_n = 0; dated_n = 0 }

    NR == 1 { header = $0; next }  # "# Changelog"

    /^## Unreleased[[:space:]]*$/ { in_unrel = 1; next }
    in_unrel && /^## / { in_unrel = 0 }
    in_unrel { unrel[++unrel_n] = $0; next }

    { dated[++dated_n] = $0 }

    END {
        # Trim blank lines around the combined Unreleased notes.
        first = 1
        last = unrel_n
        while (first <= last && unrel[first] ~ /^[[:space:]]*$/) first++
        while (last >= first && unrel[last] ~ /^[[:space:]]*$/) last--

        has = 0
        for (i = first; i <= last; i++) {
            if (unrel[i] ~ /^- /) has = 1
        }

        # Trim leading blank lines from the dated release sections.
        dfirst = 1
        while (dfirst <= dated_n && dated[dfirst] ~ /^[[:space:]]*$/) dfirst++

        print header
        print ""
        print "## Unreleased"
        print ""
        if (has) {
            printf "## %s - %s\n", tag, date
            print ""
            for (i = first; i <= last; i++) print unrel[i]
            print ""
        }
        for (i = dfirst; i <= dated_n; i++) print dated[i]
    }
' "$changelog" > "$tmp"

mv "$tmp" "$changelog"
echo "release-notes: promoted curated notes to '$tag'"
