#!/bin/sh
# check-submodule-urls.sh - fail when any submodule URL is not public HTTPS.
#
# SSH (git@… / ssh://…), git:// and http:// submodule URLs break fresh
# clones, CI and release builders: they work only for a developer with the
# right key, so nothing fails until someone else fetches. Run from the
# repository root; exits non-zero listing every offending line.
#
# Apps embed the same check inline in their CI (a guard must not depend on
# fetching the submodule it is validating), but this is the canonical copy
# for local use and kryon's own CI.

set -eu

if [ ! -f .gitmodules ]; then
    echo "submodule-urls: no .gitmodules (no submodules) - ok"
    exit 0
fi

bad=$(grep -nE '^[[:space:]]*url[[:space:]]*=' .gitmodules \
    | grep -vE '^[0-9]+:[[:space:]]*url[[:space:]]*=[[:space:]]*https://' || true)

if [ -n "$bad" ]; then
    echo "submodule-urls: non-HTTPS submodule URL(s) found:" >&2
    echo "$bad" >&2
    echo "Every submodule URL must be public HTTPS (git@/ssh:// break fresh" >&2
    echo "clones and CI). Fix with:" >&2
    echo "  git config -f .gitmodules submodule.<name>.url https://github.com/<org>/<repo>.git" >&2
    echo "  git submodule sync <path>" >&2
    exit 1
fi

echo "submodule-urls: all submodule URLs are HTTPS - ok"
