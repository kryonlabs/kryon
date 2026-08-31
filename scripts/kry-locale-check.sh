#!/bin/sh
set -eu

usage()
{
    cat <<'USAGE'
usage: kryon locale-check source.kry [...] -- locales/*.txt

Checks t("key") references against locale files using [key] blocks.
USAGE
}

[ $# -gt 0 ] || { usage >&2; exit 2; }

srcs=
locales=
mode=src
for arg in "$@"; do
    if [ "$arg" = "--" ]; then
        mode=locale
        continue
    fi
    if [ "$mode" = src ]; then
        srcs="$srcs
$arg"
    else
        locales="$locales
$arg"
    fi
done

[ -n "$(printf '%s' "$srcs" | tr -d '\n')" ] || { usage >&2; exit 2; }
[ -n "$(printf '%s' "$locales" | tr -d '\n')" ] || { usage >&2; exit 2; }

work=${TMPDIR:-/tmp}/kry-locale-check.$$
mkdir -p "$work"
trap 'rm -rf "$work"' 0 1 2 3 15
printf '0\n' > "$work/status"

printf '%s\n' "$srcs" | sed '/^$/d' | while IFS= read -r src; do
    [ -f "$src" ] || { printf 'kryon locale-check: source not found: %s\n' "$src" >&2; exit 1; }
    awk '
    {
        rest = $0
        while(match(rest, /t[ \t]*\([ \t]*"[^"]+"/)) {
            key = substr(rest, RSTART, RLENGTH)
            sub(/^t[ \t]*\([ \t]*"/, "", key)
            sub(/"$/, "", key)
            print key
            rest = substr(rest, RSTART + RLENGTH)
        }
    }' "$src"
done | sort -u > "$work/used"

en_keys=
en_values=

for loc in $locales; do
    [ -f "$loc" ] || { printf 'kryon locale-check: locale not found: %s\n' "$loc" >&2; exit 1; }
    base=$(basename "$loc")
    lang=${base%.txt}
    keys="$work/keys.$lang"
    vals="$work/vals.$lang"
    awk -v vals="$vals" '
    /^\[[^][]+\][ \t]*$/ {
        key = substr($0, 2, length($0) - 2)
        if(seen[key]++)
            printf("duplicate:%s\n", key) > "/dev/stderr"
        print key
        current = key
        next
    }
    current != "" && $0 != "---" {
        if(value[current] == "")
            value[current] = $0
        else
            value[current] = value[current] "\n" $0
    }
    END {
        for(k in value)
            printf("%s\t%s\n", k, value[k]) > vals
    }' "$loc" 2>"$work/dup.$lang" | sort -u > "$keys" || printf '1\n' > "$work/status"
    if [ -s "$work/dup.$lang" ]; then
        sed "s|^|$loc: |" "$work/dup.$lang" >&2
        printf '1\n' > "$work/status"
    fi
    if [ "$lang" = en ]; then
        en_keys=$keys
        en_values=$vals
    fi
done

for loc in $locales; do
    base=$(basename "$loc")
    lang=${base%.txt}
    keys="$work/keys.$lang"
    vals="$work/vals.$lang"
    missing=$(comm -23 "$work/used" "$keys" || true)
    unused=$(comm -13 "$work/used" "$keys" || true)
    if [ -n "$missing" ]; then
        printf '%s: missing keys:\n%s\n' "$loc" "$missing" >&2
        printf '1\n' > "$work/status"
    fi
    if [ -n "$unused" ]; then
        printf '%s: unused keys:\n%s\n' "$loc" "$unused" >&2
        printf '1\n' > "$work/status"
    fi
    if [ "$lang" != en ] && [ -f "$work/vals.en" ]; then
        while IFS="$(printf '\t')" read -r key value; do
            en_value=$(awk -F "$(printf '\t')" -v k="$key" '$1 == k { print $2; exit }' "$work/vals.en")
            if [ -n "$value" ] && [ "$value" = "$en_value" ]; then
                printf '%s: key %s copies English text\n' "$loc" "$key" >&2
                printf '1\n' > "$work/status"
            fi
        done < "$vals"
    fi
done

exit "$(cat "$work/status")"
